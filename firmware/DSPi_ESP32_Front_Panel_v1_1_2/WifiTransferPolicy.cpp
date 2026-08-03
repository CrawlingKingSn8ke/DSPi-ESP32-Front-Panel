#include "WifiTransferPolicy.h"

namespace WifiTransferPolicy {
namespace {

static PathResult pathResult(PathStatus status, size_t length = 0U,
                             size_t components = 0U) {
  PathResult result = {status, length, components};
  return result;
}

static bool asciiEqualIgnoreCase(char lhs, char rhs) {
  if (lhs >= 'A' && lhs <= 'Z') {
    lhs = static_cast<char>(lhs + ('a' - 'A'));
  }
  if (rhs >= 'A' && rhs <= 'Z') {
    rhs = static_cast<char>(rhs + ('a' - 'A'));
  }
  return lhs == rhs;
}

static bool suffixEqualIgnoreCase(const char *value, size_t valueLength,
                                  const char *suffix, size_t suffixLength) {
  if (value == 0 || suffix == 0 || valueLength < suffixLength) {
    return false;
  }
  const size_t start = valueLength - suffixLength;
  for (size_t index = 0U; index < suffixLength; ++index) {
    if (!asciiEqualIgnoreCase(value[start + index], suffix[index])) {
      return false;
    }
  }
  return true;
}

static int hexValue(uint8_t value) {
  if (value >= static_cast<uint8_t>('0') &&
      value <= static_cast<uint8_t>('9')) {
    return static_cast<int>(value - static_cast<uint8_t>('0'));
  }
  if (value >= static_cast<uint8_t>('a') &&
      value <= static_cast<uint8_t>('f')) {
    return static_cast<int>(value - static_cast<uint8_t>('a')) + 10;
  }
  if (value >= static_cast<uint8_t>('A') &&
      value <= static_cast<uint8_t>('F')) {
    return static_cast<int>(value - static_cast<uint8_t>('A')) + 10;
  }
  return -1;
}

static bool isReservedFatAscii(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>('"'):
    case static_cast<uint8_t>('*'):
    case static_cast<uint8_t>(':'):
    case static_cast<uint8_t>('<'):
    case static_cast<uint8_t>('>'):
    case static_cast<uint8_t>('?'):
    case static_cast<uint8_t>('|'):
      return true;
    default:
      return false;
  }
}

static PathStatus validateUtf8AndControls(const char *value, size_t length) {
  size_t index = 0U;
  while (index < length) {
    const uint8_t first = static_cast<uint8_t>(value[index]);
    uint32_t codePoint = 0U;
    size_t sequenceLength = 0U;

    if (first <= 0x7FU) {
      codePoint = first;
      sequenceLength = 1U;
    } else if (first >= 0xC2U && first <= 0xDFU) {
      if (index + 1U >= length) {
        return PathStatus::InvalidUtf8;
      }
      const uint8_t second = static_cast<uint8_t>(value[index + 1U]);
      if (second < 0x80U || second > 0xBFU) {
        return PathStatus::InvalidUtf8;
      }
      codePoint =
          (static_cast<uint32_t>(first & 0x1FU) << 6U) |
          static_cast<uint32_t>(second & 0x3FU);
      sequenceLength = 2U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      if (index + 2U >= length) {
        return PathStatus::InvalidUtf8;
      }
      const uint8_t second = static_cast<uint8_t>(value[index + 1U]);
      const uint8_t third = static_cast<uint8_t>(value[index + 2U]);
      if (third < 0x80U || third > 0xBFU) {
        return PathStatus::InvalidUtf8;
      }
      if (first == 0xE0U) {
        if (second < 0xA0U || second > 0xBFU) {
          return PathStatus::InvalidUtf8;
        }
      } else if (first == 0xEDU) {
        if (second < 0x80U || second > 0x9FU) {
          return PathStatus::InvalidUtf8;
        }
      } else if (second < 0x80U || second > 0xBFU) {
        return PathStatus::InvalidUtf8;
      }
      codePoint =
          (static_cast<uint32_t>(first & 0x0FU) << 12U) |
          (static_cast<uint32_t>(second & 0x3FU) << 6U) |
          static_cast<uint32_t>(third & 0x3FU);
      sequenceLength = 3U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      if (index + 3U >= length) {
        return PathStatus::InvalidUtf8;
      }
      const uint8_t second = static_cast<uint8_t>(value[index + 1U]);
      const uint8_t third = static_cast<uint8_t>(value[index + 2U]);
      const uint8_t fourth = static_cast<uint8_t>(value[index + 3U]);
      if (third < 0x80U || third > 0xBFU || fourth < 0x80U ||
          fourth > 0xBFU) {
        return PathStatus::InvalidUtf8;
      }
      if (first == 0xF0U) {
        if (second < 0x90U || second > 0xBFU) {
          return PathStatus::InvalidUtf8;
        }
      } else if (first == 0xF4U) {
        if (second < 0x80U || second > 0x8FU) {
          return PathStatus::InvalidUtf8;
        }
      } else if (second < 0x80U || second > 0xBFU) {
        return PathStatus::InvalidUtf8;
      }
      codePoint =
          (static_cast<uint32_t>(first & 0x07U) << 18U) |
          (static_cast<uint32_t>(second & 0x3FU) << 12U) |
          (static_cast<uint32_t>(third & 0x3FU) << 6U) |
          static_cast<uint32_t>(fourth & 0x3FU);
      sequenceLength = 4U;
    } else {
      return PathStatus::InvalidUtf8;
    }

    if (codePoint == 0U) {
      return PathStatus::NulByte;
    }
    if (codePoint <= 0x1FU || codePoint == 0x7FU ||
        (codePoint >= 0x80U && codePoint <= 0x9FU)) {
      return PathStatus::ControlCharacter;
    }
    index += sequenceLength;
  }
  return PathStatus::Ok;
}

static PathStatus validateComponent(const char *component, size_t length,
                                    bool allowTrailingDotOrSpace) {
  if (length == 0U) {
    return PathStatus::EmptyComponent;
  }
  if (length > kMaxComponentBytes) {
    return PathStatus::ComponentTooLong;
  }
  if (length == 1U && component[0] == '.') {
    return PathStatus::DotComponent;
  }
  if (length == 2U && component[0] == '.' && component[1] == '.') {
    return PathStatus::ParentComponent;
  }
  const uint8_t tail = static_cast<uint8_t>(component[length - 1U]);
  if (!allowTrailingDotOrSpace &&
      (tail == static_cast<uint8_t>('.') ||
       tail == static_cast<uint8_t>(' '))) {
    return PathStatus::TrailingDotOrSpace;
  }
  for (size_t index = 0U; index < length; ++index) {
    const uint8_t value = static_cast<uint8_t>(component[index]);
    if (value == 0U) {
      return PathStatus::NulByte;
    }
    if (value == static_cast<uint8_t>('\\')) {
      return PathStatus::Backslash;
    }
    if (value <= 0x1FU || value == 0x7FU) {
      return PathStatus::ControlCharacter;
    }
    if (isReservedFatAscii(value)) {
      return PathStatus::ReservedCharacter;
    }
  }
  return PathStatus::Ok;
}

static PathResult validateRelativeInternal(
    const char *path, size_t pathLength, bool allowTrailingDotOrSpace) {
  if (path == 0) {
    return pathResult(PathStatus::NullArgument);
  }
  if (pathLength == 0U) {
    return pathResult(PathStatus::EmptyPath);
  }
  if (pathLength > kMaxPathBytes) {
    return pathResult(PathStatus::PathTooLong);
  }
  if (path[0] == '/') {
    return pathResult(PathStatus::AbsolutePath);
  }

  const PathStatus utf8Status = validateUtf8AndControls(path, pathLength);
  if (utf8Status != PathStatus::Ok) {
    return pathResult(utf8Status);
  }

  size_t componentStart = 0U;
  size_t componentCount = 0U;
  for (size_t index = 0U; index < pathLength; ++index) {
    if (path[index] != '/') {
      continue;
    }
    if (index == componentStart) {
      return pathResult(PathStatus::RepeatedSeparator);
    }
    const PathStatus componentStatus =
        validateComponent(path + componentStart, index - componentStart,
                          allowTrailingDotOrSpace);
    if (componentStatus != PathStatus::Ok) {
      return pathResult(componentStatus);
    }
    ++componentCount;
    componentStart = index + 1U;
  }
  if (componentStart == pathLength) {
    return pathResult(PathStatus::EmptyComponent);
  }
  const PathStatus componentStatus =
      validateComponent(path + componentStart, pathLength - componentStart,
                        allowTrailingDotOrSpace);
  if (componentStatus != PathStatus::Ok) {
    return pathResult(componentStatus);
  }
  ++componentCount;
  return pathResult(PathStatus::Ok, pathLength, componentCount);
}

static size_t leafLength(const char *path, size_t pathLength) {
  size_t start = 0U;
  for (size_t index = 0U; index < pathLength; ++index) {
    if (path[index] == '/') {
      start = index + 1U;
    }
  }
  return pathLength - start;
}

static UploadPlanResult uploadPlanResult(UploadPlanStatus status,
                                         PathStatus pathStatus,
                                         size_t finalLength = 0U,
                                         size_t temporaryLength = 0U) {
  UploadPlanResult result = {status, pathStatus, finalLength, temporaryLength};
  return result;
}

static uint8_t safeExitStageRank(SafeExitStage stage) {
  return static_cast<uint8_t>(stage);
}

static SafeExitTransition safeExitResult(
    SafeExitStage next, TransitionDisposition disposition) {
  SafeExitTransition result = {next, disposition};
  return result;
}

static PathResult normalizeRelativeInternal(
    const char *input, size_t inputLength, char *output,
    size_t outputCapacity, bool allowTrailingDotOrSpace) {
  if (output != 0 && outputCapacity > 0U) {
    output[0] = '\0';
  }
  if (input == 0 || output == 0) {
    return pathResult(PathStatus::NullArgument);
  }
  if (inputLength == 0U) {
    return pathResult(PathStatus::EmptyPath);
  }
  if (input[0] == '/') {
    return pathResult(PathStatus::AbsolutePath);
  }

  size_t outputLength = 0U;
  for (size_t inputIndex = 0U; inputIndex < inputLength; ++inputIndex) {
    uint8_t value = static_cast<uint8_t>(input[inputIndex]);
    bool wasPercentEncoded = false;
    if (value == static_cast<uint8_t>('%')) {
      if (inputIndex + 2U >= inputLength) {
        return pathResult(PathStatus::MalformedPercentEncoding);
      }
      const int high =
          hexValue(static_cast<uint8_t>(input[inputIndex + 1U]));
      const int low = hexValue(static_cast<uint8_t>(input[inputIndex + 2U]));
      if (high < 0 || low < 0) {
        return pathResult(PathStatus::MalformedPercentEncoding);
      }
      value = static_cast<uint8_t>((high << 4) | low);
      inputIndex += 2U;
      wasPercentEncoded = true;
    }

    if (value == 0U) {
      return pathResult(PathStatus::NulByte);
    }
    if (value == static_cast<uint8_t>('\\')) {
      return pathResult(wasPercentEncoded ? PathStatus::EncodedSeparator
                                          : PathStatus::Backslash);
    }
    if (wasPercentEncoded && value == static_cast<uint8_t>('/')) {
      return pathResult(PathStatus::EncodedSeparator);
    }
    if (value <= 0x1FU || value == 0x7FU) {
      return pathResult(PathStatus::ControlCharacter);
    }
    if (outputLength >= kMaxPathBytes) {
      return pathResult(PathStatus::PathTooLong);
    }
    if (outputLength + 1U >= outputCapacity) {
      return pathResult(PathStatus::OutputTooSmall);
    }
    output[outputLength++] = static_cast<char>(value);
  }
  output[outputLength] = '\0';

  const PathResult validation = validateRelativeInternal(
      output, outputLength, allowTrailingDotOrSpace);
  if (validation.status != PathStatus::Ok) {
    output[0] = '\0';
  }
  return validation;
}

static PathResult joinRelativeUnderRootInternal(
    const char *root, size_t rootLength, const char *relative,
    size_t relativeLength, char *output, size_t outputCapacity,
    bool allowTrailingDotOrSpace) {
  if (output != 0 && outputCapacity > 0U) {
    output[0] = '\0';
  }
  if (root == 0 || relative == 0 || output == 0) {
    return pathResult(PathStatus::NullArgument);
  }
  if (rootLength == 0U || root[0] != '/') {
    return pathResult(PathStatus::AbsolutePath);
  }
  if (rootLength > kMaxPathBytes) {
    return pathResult(PathStatus::PathTooLong);
  }

  size_t rootComponents = 0U;
  if (rootLength > 1U) {
    if (root[rootLength - 1U] == '/') {
      return pathResult(PathStatus::EmptyComponent);
    }
    const PathResult rootValidation =
        validateRelativeInternal(root + 1U, rootLength - 1U, false);
    if (rootValidation.status != PathStatus::Ok) {
      return pathResult(rootValidation.status);
    }
    rootComponents = rootValidation.componentCount;
  }
  const PathResult relativeValidation = validateRelativeInternal(
      relative, relativeLength, allowTrailingDotOrSpace);
  if (relativeValidation.status != PathStatus::Ok) {
    return relativeValidation;
  }

  const size_t separatorBytes = rootLength == 1U ? 0U : 1U;
  if (rootLength > kMaxPathBytes - separatorBytes ||
      relativeLength > kMaxPathBytes - rootLength - separatorBytes) {
    return pathResult(PathStatus::PathTooLong);
  }
  const size_t totalLength = rootLength + separatorBytes + relativeLength;
  if (totalLength + 1U > outputCapacity) {
    return pathResult(PathStatus::OutputTooSmall);
  }

  size_t outputIndex = 0U;
  for (size_t index = 0U; index < rootLength; ++index) {
    output[outputIndex++] = root[index];
  }
  if (separatorBytes != 0U) {
    output[outputIndex++] = '/';
  }
  for (size_t index = 0U; index < relativeLength; ++index) {
    output[outputIndex++] = relative[index];
  }
  output[outputIndex] = '\0';
  return pathResult(PathStatus::Ok, totalLength,
                    rootComponents + relativeValidation.componentCount);
}

}  // namespace

PathResult normalizeRelativePath(const char *input, size_t inputLength,
                                 char *output, size_t outputCapacity) {
  return normalizeRelativeInternal(input, inputLength, output, outputCapacity,
                                   false);
}

PathResult normalizeExistingRelativePath(
    const char *input, size_t inputLength, char *output,
    size_t outputCapacity) {
  return normalizeRelativeInternal(input, inputLength, output, outputCapacity,
                                   true);
}

PathResult validateCanonicalRelativePath(const char *path, size_t pathLength) {
  return validateRelativeInternal(path, pathLength, false);
}

PathResult validateExistingRelativePath(const char *path, size_t pathLength) {
  return validateRelativeInternal(path, pathLength, true);
}

PathResult joinCanonicalUnderRoot(const char *root, size_t rootLength,
                                  const char *relative, size_t relativeLength,
                                  char *output, size_t outputCapacity) {
  return joinRelativeUnderRootInternal(root, rootLength, relative,
                                       relativeLength, output, outputCapacity,
                                       false);
}

PathResult joinExistingUnderRoot(const char *root, size_t rootLength,
                                 const char *relative, size_t relativeLength,
                                 char *output, size_t outputCapacity) {
  return joinRelativeUnderRootInternal(root, rootLength, relative,
                                       relativeLength, output, outputCapacity,
                                       true);
}

const char *pathStatusText(PathStatus status) {
  switch (status) {
    case PathStatus::Ok:
      return "ok";
    case PathStatus::NullArgument:
      return "null argument";
    case PathStatus::EmptyPath:
      return "empty path";
    case PathStatus::AbsolutePath:
      return "absolute path";
    case PathStatus::PathTooLong:
      return "path exceeds 511 bytes";
    case PathStatus::OutputTooSmall:
      return "output buffer too small";
    case PathStatus::EmptyComponent:
      return "empty path component";
    case PathStatus::RepeatedSeparator:
      return "repeated separator";
    case PathStatus::DotComponent:
      return "dot component";
    case PathStatus::ParentComponent:
      return "parent component";
    case PathStatus::Backslash:
      return "backslash separator";
    case PathStatus::MalformedPercentEncoding:
      return "malformed percent encoding";
    case PathStatus::EncodedSeparator:
      return "encoded separator";
    case PathStatus::NulByte:
      return "NUL byte";
    case PathStatus::ControlCharacter:
      return "control character";
    case PathStatus::InvalidUtf8:
      return "invalid UTF-8";
    case PathStatus::ComponentTooLong:
      return "component exceeds 255 bytes";
    case PathStatus::ReservedCharacter:
      return "filesystem-reserved character";
    case PathStatus::TrailingDotOrSpace:
      return "component ends in dot or space";
    default:
      return "unknown path error";
  }
}

bool hasAllowedExtension(const char *path, size_t pathLength) {
  if (path == 0 || pathLength == 0U) {
    return false;
  }
  size_t leafStart = 0U;
  for (size_t index = 0U; index < pathLength; ++index) {
    if (path[index] == '/') {
      leafStart = index + 1U;
    }
  }
  const char *extensions[] = {".flac", ".wav", ".mp3", ".jpg", ".jpeg"};
  const size_t extensionLengths[] = {5U, 4U, 4U, 4U, 5U};
  for (size_t extensionIndex = 0U; extensionIndex < 5U;
       ++extensionIndex) {
    const size_t extensionLength = extensionLengths[extensionIndex];
    if (pathLength <= leafStart + extensionLength) {
      continue;
    }
    if (suffixEqualIgnoreCase(path, pathLength,
                              extensions[extensionIndex],
                              extensionLength)) {
      return true;
    }
  }
  return false;
}

bool hasUploadingSuffix(const char *path, size_t pathLength) {
  static const char suffix[] = ".uploading";
  if (path == 0 || pathLength <= kUploadingSuffixBytes) {
    return false;
  }
  return suffixEqualIgnoreCase(path, pathLength, suffix,
                               kUploadingSuffixBytes);
}

UploadPlanResult prepareUploadPaths(const char *encodedRelative,
                                    size_t encodedLength, char *finalPath,
                                    size_t finalCapacity, char *temporaryPath,
                                    size_t temporaryCapacity) {
  if (temporaryPath != 0 && temporaryCapacity > 0U) {
    temporaryPath[0] = '\0';
  }
  if (finalPath == 0 || temporaryPath == 0) {
    return uploadPlanResult(UploadPlanStatus::OutputBufferTooSmall,
                            PathStatus::NullArgument);
  }
  const PathResult normalized =
      normalizeRelativePath(encodedRelative, encodedLength, finalPath,
                            finalCapacity);
  if (normalized.status != PathStatus::Ok) {
    return uploadPlanResult(UploadPlanStatus::PathRejected,
                            normalized.status);
  }
  if (hasUploadingSuffix(finalPath, normalized.length)) {
    finalPath[0] = '\0';
    return uploadPlanResult(UploadPlanStatus::TemporaryNameSupplied,
                            PathStatus::Ok);
  }
  if (!hasAllowedExtension(finalPath, normalized.length)) {
    finalPath[0] = '\0';
    return uploadPlanResult(UploadPlanStatus::UnsupportedExtension,
                            PathStatus::Ok);
  }
  if (leafLength(finalPath, normalized.length) > kMaxUploadLeafBytes) {
    finalPath[0] = '\0';
    return uploadPlanResult(UploadPlanStatus::UploadLeafTooLong,
                            PathStatus::Ok);
  }
  if (normalized.length > kMaxPathBytes - kUploadingSuffixBytes) {
    finalPath[0] = '\0';
    return uploadPlanResult(UploadPlanStatus::TemporaryPathTooLong,
                            PathStatus::Ok);
  }
  const size_t temporaryLength =
      normalized.length + kUploadingSuffixBytes;
  if (temporaryLength + 1U > temporaryCapacity) {
    finalPath[0] = '\0';
    return uploadPlanResult(UploadPlanStatus::OutputBufferTooSmall,
                            PathStatus::OutputTooSmall);
  }
  for (size_t index = 0U; index < normalized.length; ++index) {
    temporaryPath[index] = finalPath[index];
  }
  static const char suffix[] = ".uploading";
  for (size_t index = 0U; index < kUploadingSuffixBytes; ++index) {
    temporaryPath[normalized.length + index] = suffix[index];
  }
  temporaryPath[temporaryLength] = '\0';
  return uploadPlanResult(UploadPlanStatus::Ok, PathStatus::Ok,
                          normalized.length, temporaryLength);
}

DestinationStatus checkDestinationAvailability(bool finalFileExists,
                                                bool temporaryFileExists) {
  if (finalFileExists) {
    return DestinationStatus::FinalFileExists;
  }
  if (temporaryFileExists) {
    return DestinationStatus::IncompleteFileExists;
  }
  return DestinationStatus::Available;
}

bool isDeletableIncompletePath(const char *canonicalRelative,
                               size_t pathLength) {
  const PathResult validation =
      validateRelativeInternal(canonicalRelative, pathLength, false);
  if (validation.status != PathStatus::Ok ||
      !hasUploadingSuffix(canonicalRelative, pathLength)) {
    return false;
  }
  const size_t finalLength = pathLength - kUploadingSuffixBytes;
  return hasAllowedExtension(canonicalRelative, finalLength);
}

const char *uploadPlanStatusText(UploadPlanStatus status) {
  switch (status) {
    case UploadPlanStatus::Ok:
      return "ok";
    case UploadPlanStatus::PathRejected:
      return "path rejected";
    case UploadPlanStatus::UnsupportedExtension:
      return "unsupported extension";
    case UploadPlanStatus::TemporaryNameSupplied:
      return "client supplied temporary name";
    case UploadPlanStatus::UploadLeafTooLong:
      return "upload leaf leaves no suffix capacity";
    case UploadPlanStatus::TemporaryPathTooLong:
      return "temporary path exceeds 511 bytes";
    case UploadPlanStatus::OutputBufferTooSmall:
      return "output buffer too small";
    default:
      return "unknown upload plan error";
  }
}

DeclaredSizeResult checkDeclaredSize(bool lengthKnown, uint64_t declaredBytes,
                                     uint64_t freeBytes,
                                     uint64_t safetyMarginBytes) {
  DeclaredSizeResult result = {DeclaredSizeStatus::Ok, 0U};
  if (!lengthKnown) {
    result.status = DeclaredSizeStatus::LengthRequired;
    return result;
  }
  if (declaredBytes == 0U) {
    result.status = DeclaredSizeStatus::EmptyFile;
    return result;
  }
  if (declaredBytes > UINT64_MAX - safetyMarginBytes) {
    result.requiredBytes = UINT64_MAX;
  } else {
    result.requiredBytes = declaredBytes + safetyMarginBytes;
  }
  if (freeBytes < safetyMarginBytes ||
      declaredBytes > freeBytes - safetyMarginBytes) {
    result.status = DeclaredSizeStatus::InsufficientFreeSpace;
  }
  return result;
}

ByteCountStatus addReceivedBytes(uint64_t receivedBytes, uint64_t chunkBytes,
                                 uint64_t declaredBytes,
                                 uint64_t *updatedBytes) {
  if (updatedBytes == 0) {
    return ByteCountStatus::NullOutput;
  }
  *updatedBytes = receivedBytes;
  if (chunkBytes > UINT64_MAX - receivedBytes) {
    return ByteCountStatus::CounterOverflow;
  }
  const uint64_t next = receivedBytes + chunkBytes;
  if (next > declaredBytes) {
    return ByteCountStatus::ExceedsDeclaredSize;
  }
  *updatedBytes = next;
  return ByteCountStatus::Ok;
}

bool finalByteCountMatches(uint64_t receivedBytes, uint64_t declaredBytes) {
  return receivedBytes == declaredBytes;
}

PageWindow directoryPage(uint64_t totalEntries, uint64_t pageIndex,
                         uint32_t pageSize) {
  PageWindow result = {PageStatus::Ok, 0U, 0U, 0U, false, false};
  if (pageSize == 0U) {
    result.status = PageStatus::InvalidPageSize;
    return result;
  }
  result.totalPages = totalEntries / static_cast<uint64_t>(pageSize);
  if (totalEntries % static_cast<uint64_t>(pageSize) != 0U) {
    ++result.totalPages;
  }
  if (totalEntries == 0U) {
    if (pageIndex != 0U) {
      result.status = PageStatus::PageOutOfRange;
    }
    return result;
  }
  if (pageIndex >= result.totalPages) {
    result.status = PageStatus::PageOutOfRange;
    return result;
  }
  if (pageIndex > UINT64_MAX / static_cast<uint64_t>(pageSize)) {
    result.status = PageStatus::ArithmeticOverflow;
    return result;
  }
  result.firstEntry = pageIndex * static_cast<uint64_t>(pageSize);
  const uint64_t remaining = totalEntries - result.firstEntry;
  result.entryCount =
      remaining < static_cast<uint64_t>(pageSize)
          ? static_cast<uint32_t>(remaining)
          : pageSize;
  result.hasPrevious = pageIndex > 0U;
  result.hasNext = pageIndex + 1U < result.totalPages;
  return result;
}

UploadTransition transitionUpload(UploadPhase current, UploadSignal signal) {
  UploadTransition result = {current, TransitionDisposition::Rejected};

  if (signal == UploadSignal::Begin) {
    if (current == UploadPhase::Idle || current == UploadPhase::Complete ||
        current == UploadPhase::Incomplete) {
      result.next = UploadPhase::Writing;
      result.disposition = TransitionDisposition::Applied;
    } else {
      result.disposition = TransitionDisposition::Busy;
    }
    return result;
  }

  switch (current) {
    case UploadPhase::Idle:
      break;
    case UploadPhase::Writing:
      if (signal == UploadSignal::BodyComplete) {
        result.next = UploadPhase::Finalizing;
        result.disposition = TransitionDisposition::Applied;
      } else if (signal == UploadSignal::Cancel ||
                 signal == UploadSignal::Timeout ||
                 signal == UploadSignal::Disconnect) {
        result.next = UploadPhase::ClosingIncomplete;
        result.disposition = TransitionDisposition::Applied;
      }
      break;
    case UploadPhase::ClosingIncomplete:
      if (signal == UploadSignal::WriterClosed) {
        result.next = UploadPhase::Incomplete;
        result.disposition = TransitionDisposition::Applied;
      } else if (signal == UploadSignal::Cancel ||
                 signal == UploadSignal::Timeout ||
                 signal == UploadSignal::Disconnect) {
        result.disposition = TransitionDisposition::NoChange;
      }
      break;
    case UploadPhase::Finalizing:
      if (signal == UploadSignal::CommitSucceeded) {
        result.next = UploadPhase::Complete;
        result.disposition = TransitionDisposition::Applied;
      } else if (signal == UploadSignal::CommitFailed ||
                 signal == UploadSignal::Cancel ||
                 signal == UploadSignal::Timeout ||
                 signal == UploadSignal::Disconnect) {
        result.next = UploadPhase::ClosingIncomplete;
        result.disposition = TransitionDisposition::Applied;
      } else if (signal == UploadSignal::BodyComplete) {
        result.disposition = TransitionDisposition::NoChange;
      }
      break;
    case UploadPhase::Complete:
      if (signal == UploadSignal::CommitSucceeded) {
        result.disposition = TransitionDisposition::NoChange;
      }
      break;
    case UploadPhase::Incomplete:
      if (signal == UploadSignal::WriterClosed ||
          signal == UploadSignal::Cancel ||
          signal == UploadSignal::Timeout ||
          signal == UploadSignal::Disconnect ||
          signal == UploadSignal::CommitFailed) {
        result.disposition = TransitionDisposition::NoChange;
      }
      break;
  }
  return result;
}

bool uploadWriterIsActive(UploadPhase phase) {
  return phase == UploadPhase::Writing ||
         phase == UploadPhase::ClosingIncomplete ||
         phase == UploadPhase::Finalizing;
}

SafeExitTransition transitionSafeExit(SafeExitStage current,
                                      SafeExitSignal signal,
                                      bool writerActive) {
  if (signal == SafeExitSignal::Request) {
    if (current == SafeExitStage::Serving) {
      if (writerActive) {
        return safeExitResult(
            current, TransitionDisposition::BlockedByActiveWriter);
      }
      return safeExitResult(SafeExitStage::StopAcceptance,
                            TransitionDisposition::Applied);
    }
    return safeExitResult(current, TransitionDisposition::NoChange);
  }

  if (signal == SafeExitSignal::Failure) {
    if (current == SafeExitStage::RecoveryError) {
      return safeExitResult(current, TransitionDisposition::NoChange);
    }
    if (current == SafeExitStage::Complete) {
      return safeExitResult(current, TransitionDisposition::Rejected);
    }
    return safeExitResult(SafeExitStage::RecoveryError,
                          TransitionDisposition::Applied);
  }
  if (current == SafeExitStage::RecoveryError) {
    return safeExitResult(current, TransitionDisposition::Rejected);
  }

  SafeExitStage target = current;
  switch (signal) {
    case SafeExitSignal::AcceptanceStopped:
      target = SafeExitStage::CloseHandles;
      break;
    case SafeExitSignal::HandlesClosed:
      target = SafeExitStage::SyncFilesystem;
      break;
    case SafeExitSignal::FilesystemSynced:
      target = SafeExitStage::UnmountReadWrite;
      break;
    case SafeExitSignal::ReadWriteUnmounted:
      target = SafeExitStage::StopHttp;
      break;
    case SafeExitSignal::HttpStopped:
      target = SafeExitStage::StopWifi;
      break;
    case SafeExitSignal::WifiStopped:
      target = SafeExitStage::MountReadOnly;
      break;
    case SafeExitSignal::ReadOnlyMounted:
      target = SafeExitStage::RestartNormalServices;
      break;
    case SafeExitSignal::NormalServicesRestarted:
      target = SafeExitStage::Complete;
      break;
    default:
      return safeExitResult(current, TransitionDisposition::Rejected);
  }

  const uint8_t currentRank = safeExitStageRank(current);
  const uint8_t targetRank = safeExitStageRank(target);
  if (currentRank >= targetRank) {
    return safeExitResult(current, TransitionDisposition::NoChange);
  }
  if (targetRank == static_cast<uint8_t>(currentRank + 1U)) {
    return safeExitResult(target, TransitionDisposition::Applied);
  }
  return safeExitResult(current, TransitionDisposition::Rejected);
}

}  // namespace WifiTransferPolicy
