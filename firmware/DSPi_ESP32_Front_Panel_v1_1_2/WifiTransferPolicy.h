#ifndef DSPI_WIFI_TRANSFER_POLICY_H
#define DSPI_WIFI_TRANSFER_POLICY_H

#include <stddef.h>
#include <stdint.h>

// Portable, allocation-free policy helpers for the exclusive Wi-Fi transfer
// mode.  This unit deliberately has no Arduino, filesystem, network, String,
// STL, heap, or FreeRTOS dependency so the same rules can be exercised by host
// tests.
namespace WifiTransferPolicy {

// The established DSPi path buffer is 512 bytes including its terminator.
static const size_t kPathCapacity = 512U;
static const size_t kMaxPathBytes = kPathCapacity - 1U;

// FAT long file names are limited to 255 name units.  Limiting the encoded
// UTF-8 byte count to 255 is conservative for FAT16/FAT32/exFAT and keeps every
// component inside SdFat's long-name boundary.  An upload leaf reserves ten of
// those bytes for the literal ".uploading" recovery suffix.
static const size_t kMaxComponentBytes = 255U;
static const size_t kUploadingSuffixBytes = 10U;
static const size_t kMaxUploadLeafBytes =
    kMaxComponentBytes - kUploadingSuffixBytes;

static const uint32_t kDirectoryPageEntries = 96U;
static const uint64_t kDefaultFreeSpaceMarginBytes =
    UINT64_C(16) * UINT64_C(1024) * UINT64_C(1024);

enum class PathStatus : uint8_t {
  Ok = 0,
  NullArgument,
  EmptyPath,
  AbsolutePath,
  PathTooLong,
  OutputTooSmall,
  EmptyComponent,
  RepeatedSeparator,
  DotComponent,
  ParentComponent,
  Backslash,
  MalformedPercentEncoding,
  EncodedSeparator,
  NulByte,
  ControlCharacter,
  InvalidUtf8,
  ComponentTooLong,
  ReservedCharacter,
  TrailingDotOrSpace
};

struct PathResult {
  PathStatus status;
  size_t length;
  size_t componentCount;
};

// Decodes percent escapes exactly once and produces a canonical relative path.
// Raw '/' is the only accepted separator. Encoded separators are rejected
// rather than introduced during decoding. The explicit input length is
// required so an embedded NUL cannot hide unvalidated bytes. Input and output
// must not overlap; callers should keep these fixed buffers in transfer-owned
// storage rather than on loopTask's stack.
PathResult normalizeRelativePath(const char *input, size_t inputLength,
                                 char *output, size_t outputCapacity);

// Validates an already-decoded relative path (for example, a name returned by
// SdFat). Literal '%' bytes are ordinary filename bytes in this form.
PathResult validateCanonicalRelativePath(const char *path, size_t pathLength);

// Existing exFAT libraries may contain legal names ending in spaces or dots.
// These helpers are for read-only browsing and explicit folder deletion only.
// Upload and mkdir paths must continue to use the strict canonical helpers.
PathResult normalizeExistingRelativePath(const char *input, size_t inputLength,
                                         char *output, size_t outputCapacity);
PathResult validateExistingRelativePath(const char *path, size_t pathLength);

// Joins a trusted, canonical absolute transfer root ("/" or "/folder") and a
// canonical relative path. Both inputs are revalidated and the complete
// absolute result remains within the same 512-byte capacity.
PathResult joinCanonicalUnderRoot(const char *root, size_t rootLength,
                                  const char *relative, size_t relativeLength,
                                  char *output, size_t outputCapacity);
PathResult joinExistingUnderRoot(const char *root, size_t rootLength,
                                 const char *relative, size_t relativeLength,
                                 char *output, size_t outputCapacity);

const char *pathStatusText(PathStatus status);

bool hasAllowedExtension(const char *path, size_t pathLength);
bool hasUploadingSuffix(const char *path, size_t pathLength);

enum class UploadPlanStatus : uint8_t {
  Ok = 0,
  PathRejected,
  UnsupportedExtension,
  TemporaryNameSupplied,
  UploadLeafTooLong,
  TemporaryPathTooLong,
  OutputBufferTooSmall
};

struct UploadPlanResult {
  UploadPlanStatus status;
  PathStatus pathStatus;
  size_t finalLength;
  size_t temporaryLength;
};

// Builds canonical relative final and temporary paths. Filesystem existence is
// checked separately, after these safe names have been joined beneath the
// active transfer root. The two output buffers and input must not overlap.
UploadPlanResult prepareUploadPaths(const char *encodedRelative,
                                    size_t encodedLength, char *finalPath,
                                    size_t finalCapacity, char *temporaryPath,
                                    size_t temporaryCapacity);

enum class DestinationStatus : uint8_t {
  Available = 0,
  FinalFileExists,
  IncompleteFileExists
};

// v13 never overwrites either a completed file or an earlier incomplete file.
DestinationStatus checkDestinationAvailability(bool finalFileExists,
                                                bool temporaryFileExists);

// Only a canonical "<allowed music/artwork filename>.uploading" path is
// eligible for the incomplete-file delete endpoint.
bool isDeletableIncompletePath(const char *canonicalRelative,
                               size_t pathLength);

const char *uploadPlanStatusText(UploadPlanStatus status);

enum class DeclaredSizeStatus : uint8_t {
  Ok = 0,
  LengthRequired,
  EmptyFile,
  InsufficientFreeSpace
};

struct DeclaredSizeResult {
  DeclaredSizeStatus status;
  uint64_t requiredBytes;
};

// All arithmetic is 64-bit and saturating. A file is admitted only when its
// known non-zero length plus the selected safety margin fits in freeBytes.
DeclaredSizeResult checkDeclaredSize(bool lengthKnown, uint64_t declaredBytes,
                                     uint64_t freeBytes,
                                     uint64_t safetyMarginBytes);

enum class ByteCountStatus : uint8_t {
  Ok = 0,
  NullOutput,
  CounterOverflow,
  ExceedsDeclaredSize
};

ByteCountStatus addReceivedBytes(uint64_t receivedBytes, uint64_t chunkBytes,
                                 uint64_t declaredBytes,
                                 uint64_t *updatedBytes);
bool finalByteCountMatches(uint64_t receivedBytes, uint64_t declaredBytes);

enum class PageStatus : uint8_t {
  Ok = 0,
  InvalidPageSize,
  PageOutOfRange,
  ArithmeticOverflow
};

struct PageWindow {
  PageStatus status;
  uint64_t firstEntry;
  uint32_t entryCount;
  uint64_t totalPages;
  bool hasPrevious;
  bool hasNext;
};

PageWindow directoryPage(uint64_t totalEntries, uint64_t pageIndex,
                         uint32_t pageSize = kDirectoryPageEntries);

enum class TransitionDisposition : uint8_t {
  Applied = 0,
  NoChange,
  Rejected,
  Busy,
  BlockedByActiveWriter
};

enum class UploadPhase : uint8_t {
  Idle = 0,
  Writing,
  ClosingIncomplete,
  Finalizing,
  Complete,
  Incomplete
};

enum class UploadSignal : uint8_t {
  Begin = 0,
  BodyComplete,
  Cancel,
  Timeout,
  Disconnect,
  WriterClosed,
  CommitSucceeded,
  CommitFailed
};

struct UploadTransition {
  UploadPhase next;
  TransitionDisposition disposition;
};

UploadTransition transitionUpload(UploadPhase current, UploadSignal signal);
bool uploadWriterIsActive(UploadPhase phase);

// Safe-exit stages are deliberately ordered to match the required data-safety
// sequence. A repeated completion signal for an already-passed stage is
// idempotent; skipping a stage is rejected.
enum class SafeExitStage : uint8_t {
  Serving = 0,
  StopAcceptance,
  CloseHandles,
  SyncFilesystem,
  UnmountReadWrite,
  StopHttp,
  StopWifi,
  MountReadOnly,
  RestartNormalServices,
  Complete,
  RecoveryError
};

enum class SafeExitSignal : uint8_t {
  Request = 0,
  AcceptanceStopped,
  HandlesClosed,
  FilesystemSynced,
  ReadWriteUnmounted,
  HttpStopped,
  WifiStopped,
  ReadOnlyMounted,
  NormalServicesRestarted,
  Failure
};

struct SafeExitTransition {
  SafeExitStage next;
  TransitionDisposition disposition;
};

// Request is blocked (and does not cancel implicitly) while a writer is active.
// The browser must explicitly finish or cancel the upload first.
SafeExitTransition transitionSafeExit(SafeExitStage current,
                                      SafeExitSignal signal,
                                      bool writerActive);

}  // namespace WifiTransferPolicy

#endif  // DSPI_WIFI_TRANSFER_POLICY_H
