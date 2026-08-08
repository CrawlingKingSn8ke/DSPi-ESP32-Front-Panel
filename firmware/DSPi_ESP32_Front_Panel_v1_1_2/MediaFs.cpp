#include "MediaFs.h"

#include <climits>
#include <cstdio>
#include <cstring>

MediaFsStorage mediaFs;

namespace {

bool hasUploadingSuffix(const char *path)
{
  static constexpr char suffix[] = ".uploading";
  if (!path) return false;
  const size_t length = std::strlen(path);
  const size_t suffixLength = sizeof(suffix) - 1;
  return length > suffixLength &&
         std::strcmp(path + length - suffixLength, suffix) == 0;
}

bool pathNeedsExactTraversal(const char *path)
{
  if (!path || path[0] != '/') return false;
  const char *component = path + 1;
  while (*component) {
    const char *separator = std::strchr(component, '/');
    const size_t length = separator
        ? static_cast<size_t>(separator - component)
        : std::strlen(component);
    if (length &&
        (component[length - 1] == ' ' || component[length - 1] == '.')) {
      return true;
    }
    if (!separator) break;
    component = separator + 1;
  }
  return false;
}

}  // namespace

MediaFsFile::~MediaFsFile()
{
  close();
}

MediaFsFile::MediaFsFile(MediaFsFile &&other) noexcept
{
  moveFrom(other);
}

MediaFsFile &MediaFsFile::operator=(MediaFsFile &&other) noexcept
{
  if (this != &other) {
    close();
    moveFrom(other);
  }
  return *this;
}

void MediaFsFile::moveFrom(MediaFsFile &other)
{
  // FsBaseFile::move transfers the open FAT/exFAT file object without copying
  // its internal volume pointers or directory state.
  file_.move(&other.file_);
  strlcpy(path_, other.path_, sizeof(path_));
  strlcpy(name_, other.name_, sizeof(name_));
  ioError_ = other.ioError_;
  other.path_[0] = '\0';
  other.name_[0] = '\0';
  other.ioError_ = false;
}

MediaFsFile::operator bool() const
{
  return file_.isOpen();
}

bool MediaFsFile::isDirectory() const
{
  return file_.isDir();
}

const char *MediaFsFile::path() const
{
  return path_;
}

const char *MediaFsFile::name() const
{
  return name_;
}

size_t MediaFsFile::read(uint8_t *buffer, size_t length)
{
  if (!buffer || length == 0 || !file_.isOpen() || ioError_) return 0;
  int result = file_.read(buffer, length);
  if (result < 0) {
    ioError_ = true;
    return 0;
  }

  // A short read is normal only when it reaches the known end of the file.
  // Treat any other partial result as a persistent I/O error so decoder
  // callbacks cannot repeatedly re-enter the same failed SdFat transaction.
  if (static_cast<size_t>(result) < length &&
      file_.curPosition() < file_.fileSize()) {
    ioError_ = true;
  }
  return result > 0 ? static_cast<size_t>(result) : 0;
}

int MediaFsFile::read()
{
  if (!file_.isOpen() || ioError_) return -1;
  int result = file_.read();
  if (result < 0 && file_.curPosition() < file_.fileSize()) ioError_ = true;
  return result;
}

size_t MediaFsFile::write(const uint8_t *buffer, size_t length)
{
  if (!buffer || length == 0 || !file_.isOpen()) return 0;
  size_t result = file_.write(buffer, length);
  if (result != length || file_.getWriteError()) ioError_ = true;
  return result;
}

bool MediaFsFile::preAllocate(uint64_t length)
{
  if (!file_.isOpen() || length == 0) return false;
  const bool ok = file_.preAllocate(length);
  if (!ok) ioError_ = true;
  return ok;
}

bool MediaFsFile::truncate(uint64_t length)
{
  if (!file_.isOpen()) return false;
  const bool ok = file_.truncate(length);
  if (!ok || file_.getWriteError()) ioError_ = true;
  return ok;
}

bool MediaFsFile::seek(uint64_t position)
{
  if (!file_.isOpen()) return false;
  bool ok = file_.seekSet(position);
  if (!ok) ioError_ = true;
  return ok;
}

uint64_t MediaFsFile::position() const
{
  return file_.isOpen() ? file_.curPosition() : 0;
}

uint64_t MediaFsFile::size() const
{
  return file_.isOpen() ? file_.fileSize() : 0;
}

int MediaFsFile::available() const
{
  if (!file_.isOpen()) return 0;
  uint64_t bytes = file_.available64();
  return bytes > static_cast<uint64_t>(INT_MAX)
             ? INT_MAX : static_cast<int>(bytes);
}

bool MediaFsFile::sync()
{
  if (!file_.isOpen()) return false;
  bool ok = file_.sync();
  if (!ok || file_.getWriteError()) ioError_ = true;
  return ok && !ioError_;
}

bool MediaFsFile::rewindDirectory()
{
  if (!file_.isOpen() || !file_.isDir()) return false;
  file_.rewind();
  return true;
}

void MediaFsFile::close()
{
  if (file_.isOpen()) file_.close();
  path_[0] = '\0';
  name_[0] = '\0';
  ioError_ = false;
}

bool MediaFsFile::hadIoError() const
{
  return ioError_;
}

void MediaFsFile::clearIoError()
{
  ioError_ = false;
}

bool MediaFsFile::setPath(const char *path)
{
  if (!path || !path[0] || std::strlen(path) >= sizeof(path_)) {
    path_[0] = '\0';
    name_[0] = '\0';
    return false;
  }

  strlcpy(path_, path, sizeof(path_));
  const char *slash = std::strrchr(path_, '/');
  const char *base = slash ? slash + 1 : path_;
  if (!base[0] && path_[0] == '/' && path_[1] == '\0') base = "/";
  strlcpy(name_, base, sizeof(name_));
  return true;
}

bool MediaFsFile::setChildPath(const char *parentPath, const char *childName)
{
  if (!parentPath || !parentPath[0] || !childName || !childName[0] ||
      std::strlen(childName) >= sizeof(name_)) {
    return false;
  }

  if (childName != name_) strlcpy(name_, childName, sizeof(name_));
  int written;
  if (std::strcmp(parentPath, "/") == 0) {
    written = snprintf(path_, sizeof(path_), "/%s", childName);
  } else {
    written = snprintf(path_, sizeof(path_), "%s/%s", parentPath, childName);
  }
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(path_)) {
    path_[0] = '\0';
    name_[0] = '\0';
    return false;
  }
  return true;
}

bool MediaFsFile::openNextFileWithFlags(MediaFsFile &result,
                                            uint32_t *skippedEntries,
                                            oflag_t openFlags)
{
  result.close();
  if (!file_.isOpen() || !file_.isDir()) return false;

  while (result.file_.openNext(&file_, openFlags)) {
    // Read directly into the result object's bounded name storage.  The old
    // implementation also placed a 512-byte temporary on loopTask for every
    // directory entry.
    size_t childLength = result.file_.getName(result.name_,
                                               sizeof(result.name_));
    if (childLength > 0 && result.setChildPath(path_, result.name_)) {
      return true;
    }

    // An entry whose UTF-8 name or complete path exceeds the panel's bounded
    // buffers must not masquerade as end-of-directory. Advance past it and keep
    // scanning so later valid files remain visible.
    result.close();
    if (skippedEntries && *skippedEntries != UINT32_MAX) (*skippedEntries)++;
  }
  return false;
}

bool MediaFsFile::openNextFile(MediaFsFile &result,
                               uint32_t *skippedEntries)
{
  return openNextFileWithFlags(result, skippedEntries, O_RDONLY);
}

bool MediaFsFile::openNextFileForDelete(MediaFsFile &result)
{
  result.close();
  if (!file_.isOpen() || !file_.isDir()) return false;

  while (result.file_.openNext(&file_, O_RDONLY)) {
    const uint32_t exactIndex = result.file_.dirIndex();
    const bool directory = result.file_.isDir();
    size_t childLength = result.file_.getName(result.name_,
                                               sizeof(result.name_));
    if (childLength == 0 || !result.setChildPath(path_, result.name_)) {
      result.close();
      continue;
    }
    if (directory) return true;

    // ExFatFile::remove() requires isWritable(). Reopen this exact directory
    // index O_RDWR rather than using the normal O_RDONLY browser handle.
    result.close();
    if (!result.file_.open(&file_, exactIndex, O_RDWR)) {
      Serial.printf("MEDIA FS: recursive delete writable reopen failed parent=%s index=%lu\n",
                    path_, static_cast<unsigned long>(exactIndex));
      return false;
    }
    childLength = result.file_.getName(result.name_, sizeof(result.name_));
    if (childLength == 0 || !result.setChildPath(path_, result.name_)) {
      result.close();
      return false;
    }
    return true;
  }
  return false;
}

MediaFsFile MediaFsFile::openNextFile(uint32_t *skippedEntries)
{
  MediaFsFile result;
  openNextFile(result, skippedEntries);
  return result;
}

bool MediaFsStorage::begin(uint8_t chipSelectPin, SPIClass &spi,
                           uint32_t frequencyHz, const char *, uint8_t, bool,
                           MediaFsAccessMode accessMode)
{
  mounted_ = false;
  accessMode_ = MediaFsAccessMode::Unmounted;
  if (accessMode == MediaFsAccessMode::Unmounted) return false;
  mounted_ = fs_.begin(SdSpiConfig(chipSelectPin,
                                   SHARED_SPI | USER_SPI_BEGIN,
                                   SD_SCK_HZ(frequencyHz), &spi));
  if (mounted_) accessMode_ = accessMode;
  return mounted_;
}

void MediaFsStorage::end()
{
  mountProbeScratch_.close();
  exactEntryScratch_.close();
  exactDirectoryScratch_.close();
  fs_.end();
  mounted_ = false;
  accessMode_ = MediaFsAccessMode::Unmounted;
}

MediaFsFile MediaFsStorage::open(const char *path)
{
  MediaFsFile result;
  if (!mounted_ || !path || !path[0]) return result;

  // Prefer the byte-exact component walk for names ending in a space or dot.
  // A direct FAT-style lookup may successfully open a different normalised
  // sibling, so waiting for the direct open to fail is not sufficient.
  if (pathNeedsExactTraversal(path)) {
    if (openExactPathFallback(path, result, O_RDONLY)) {
      Serial.printf("MEDIA FS: exact path opened ambiguous=%s\n", path);
    } else {
      // Never fall through to SdFat's text-path parser for an ambiguous
      // trailing-space/dot component.  A normalised lookup could open a
      // different sibling and make the browser show misleading contents.
      Serial.printf("MEDIA FS: exact path not found ambiguous=%s\n", path);
    }
    return result;
  }

  if (result.file_.open(fs_.vol(), path, O_RDONLY)) {
    if (!result.setPath(path)) {
      result.close();
    } else if (result.file_.isDir()) {
      result.file_.rewind();
    }
    return result;
  }

  // Some exFAT libraries accept names containing legal trailing spaces while
  // path-based reopen helpers normalise those same components.  The panel had
  // already discovered the directory by openNext(), but reopening a selected
  // album such as "1971 - Electric Warrior " then appeared empty.  Fall back
  // to an exact component walk that compares the raw directory-entry names and
  // moves the matching FsFile handle forward without normalising the name.
  if (openExactPathFallback(path, result, O_RDONLY)) {
    Serial.printf("MEDIA FS: exact path fallback opened=%s\n", path);
  }
  return result;
}

bool MediaFsStorage::openExactPathFallback(const char *path,
                                           MediaFsFile &result,
                                           oflag_t finalOpenFlags)
{
  result.close();
  exactEntryScratch_.close();
  exactDirectoryScratch_.close();
  exactComponentScratch_[0] = '\0';
  exactNameScratch_[0] = '\0';

  if (!mounted_ || !path || path[0] != '/' || path[1] == '\0' ||
      std::strlen(path) >= MEDIA_FS_PATH_CAPACITY) {
    return false;
  }
  if (!exactDirectoryScratch_.open(fs_.vol(), "/", O_RDONLY) ||
      !exactDirectoryScratch_.isDir()) {
    exactDirectoryScratch_.close();
    return false;
  }

  const char *cursor = path + 1;
  while (*cursor) {
    const char *separator = std::strchr(cursor, '/');
    const size_t componentLength = separator
        ? static_cast<size_t>(separator - cursor)
        : std::strlen(cursor);
    if (componentLength == 0 ||
        componentLength >= sizeof(exactComponentScratch_)) {
      exactDirectoryScratch_.close();
      return false;
    }
    std::memcpy(exactComponentScratch_, cursor, componentLength);
    exactComponentScratch_[componentLength] = '\0';

    const bool finalComponent = separator == nullptr;
    bool found = false;
    uint32_t exactIndex = 0;
    exactEntryScratch_.close();
    exactDirectoryScratch_.rewind();
    while (exactEntryScratch_.openNext(&exactDirectoryScratch_, O_RDONLY)) {
      const size_t nameLength = exactEntryScratch_.getName(
          exactNameScratch_, sizeof(exactNameScratch_));
      if (nameLength == componentLength &&
          std::memcmp(exactNameScratch_, exactComponentScratch_,
                      componentLength) == 0) {
        exactIndex = exactEntryScratch_.dirIndex();
        found = true;
        break;
      }
      exactEntryScratch_.close();
    }
    if (!found) {
      exactEntryScratch_.close();
      exactDirectoryScratch_.close();
      return false;
    }

    // openNext() is intentionally read-only while comparing raw names. For
    // the final component, reopen that exact directory index with the caller's
    // requested flags. This avoids both path normalisation and SdFat's exFAT
    // remove() rejection of read-only handles.
    if (finalComponent && finalOpenFlags != O_RDONLY) {
      exactEntryScratch_.close();
      if (!exactEntryScratch_.open(&exactDirectoryScratch_, exactIndex,
                                   finalOpenFlags)) {
        exactDirectoryScratch_.close();
        return false;
      }
    }

    if (!finalComponent && !exactEntryScratch_.isDir()) {
      exactEntryScratch_.close();
      exactDirectoryScratch_.close();
      return false;
    }

    exactDirectoryScratch_.close();
    exactDirectoryScratch_.move(&exactEntryScratch_);
    if (finalComponent) break;
    cursor = separator + 1;
    if (!*cursor) {
      exactDirectoryScratch_.close();
      return false;
    }
  }

  if (!exactDirectoryScratch_.isOpen()) return false;
  result.file_.move(&exactDirectoryScratch_);
  if (!result.setPath(path)) {
    result.close();
    return false;
  }
  if (result.file_.isDir()) result.file_.rewind();
  return true;
}

MediaFsFile MediaFsStorage::openTransferDeleteFile(const char *path)
{
  MediaFsFile result;
  if (!transferWriteAllowed() || !path || !path[0]) return result;

  // A normal-file deletion target must be opened writable. SdFat's exFAT
  // remove() checks isWritable() on the handle; normal media/browser open() is
  // deliberately O_RDONLY and therefore cannot be reused here.
  if (pathNeedsExactTraversal(path)) {
    if (openExactPathFallback(path, result, O_RDWR) &&
        !result.file_.isDir() && result.file_.isWritable()) {
      Serial.printf("MEDIA FS: writable exact delete file opened=%s\n",
                    path);
    } else {
      result.close();
      Serial.printf("MEDIA FS: writable exact delete file failed=%s\n",
                    path);
    }
    return result;
  }

  if (result.file_.open(fs_.vol(), path, O_RDWR)) {
    if (!result.setPath(path) || result.file_.isDir() ||
        !result.file_.isWritable()) {
      result.close();
    }
    return result;
  }

  if (openExactPathFallback(path, result, O_RDWR) &&
      !result.file_.isDir() && result.file_.isWritable()) {
    Serial.printf("MEDIA FS: writable exact delete file fallback opened=%s\n",
                  path);
  } else {
    result.close();
    Serial.printf("MEDIA FS: writable delete file failed=%s\n", path);
  }
  return result;
}

MediaFsFile MediaFsStorage::createTransferFileExclusive(const char *path)
{
  MediaFsFile result;
  if (!transferWriteAllowed() || !path || !path[0] ||
      !hasUploadingSuffix(path)) {
    return result;
  }
  if (result.file_.open(fs_.vol(), path,
                        O_WRONLY | O_CREAT | O_EXCL)) {
    if (!result.setPath(path)) result.close();
  }
  return result;
}

bool MediaFsStorage::exists(const char *path)
{
  return mounted_ && path && path[0] && fs_.exists(path);
}

bool MediaFsStorage::makeTransferDirectory(const char *path)
{
  return transferWriteAllowed() && path && path[0] &&
         fs_.mkdir(path, false);
}

bool MediaFsStorage::renameTransferFile(const char *temporaryPath,
                                        const char *finalPath)
{
  if (!transferWriteAllowed() || !temporaryPath || !temporaryPath[0] ||
      !finalPath || !finalPath[0] || !hasUploadingSuffix(temporaryPath) ||
      hasUploadingSuffix(finalPath) || fs_.exists(finalPath)) {
    return false;
  }
  return fs_.rename(temporaryPath, finalPath);
}

bool MediaFsStorage::removeIncompleteTransferFile(const char *path)
{
  if (!transferWriteAllowed() || !path || !path[0] ||
      !hasUploadingSuffix(path)) return false;
  return fs_.remove(path);
}

bool MediaFsStorage::removeTransferFileHandle(MediaFsFile &file)
{
  if (!transferWriteAllowed() || !file.file_.isOpen() ||
      file.file_.isDir() || !file.file_.isWritable()) {
    Serial.printf("MEDIA FS: file delete handle rejected open=%s dir=%s writable=%s\n",
                  file.file_.isOpen() ? "yes" : "no",
                  file.file_.isDir() ? "yes" : "no",
                  file.file_.isWritable() ? "yes" : "no");
    return false;
  }
  const bool removed = file.file_.remove();
  file.close();
  return removed;
}

bool MediaFsStorage::removeTransferDirectoryHandle(MediaFsFile &directory)
{
  if (!transferWriteAllowed() || !directory.file_.isOpen() ||
      !directory.file_.isDir()) {
    Serial.printf("MEDIA FS: directory delete handle rejected open=%s dir=%s\n",
                  directory.file_.isOpen() ? "yes" : "no",
                  directory.file_.isDir() ? "yes" : "no");
    return false;
  }
  const bool removed = directory.file_.rmdir();
  directory.close();
  return removed;
}

bool MediaFsStorage::syncTransferDevice()
{
  return transferWriteAllowed() && fs_.card() &&
         fs_.card()->syncDevice();
}

bool MediaFsStorage::freeSpaceBytes(uint64_t &bytes)
{
  bytes = 0;
  if (!mounted_ || !fs_.vol()) return false;
  int32_t freeClusters = fs_.vol()->freeClusterCount();
  uint32_t bytesPerCluster = fs_.vol()->bytesPerCluster();
  if (freeClusters < 0 || bytesPerCluster == 0) return false;
  bytes = static_cast<uint64_t>(static_cast<uint32_t>(freeClusters)) *
          static_cast<uint64_t>(bytesPerCluster);
  return true;
}

bool MediaFsStorage::mountedCardUsable()
{
  mountProbeScratch_.close();
  exactEntryScratch_.close();
  exactDirectoryScratch_.close();
  if (!mounted_ || !fs_.card()) return false;
  const bool opened = mountProbeScratch_.open(fs_.vol(), "/", O_RDONLY);
  const bool usable = opened && mountProbeScratch_.isDir() &&
                      cardType() != MEDIA_CARD_NONE && cardSize() > 0;
  mountProbeScratch_.close();
  return usable;
}

bool MediaFsStorage::setAccessMode(MediaFsAccessMode accessMode)
{
  if (!mounted_ || accessMode == MediaFsAccessMode::Unmounted) return false;
  if (accessMode_ == MediaFsAccessMode::Unmounted) return false;

  // These storage-owned handles are only temporary lookup/probe scratch, but
  // make their release explicit at each ownership boundary.  The panel closes
  // its browser handles before calling this method, so transfer mode begins
  // and ends with no hidden directory handle retained by MediaFsStorage.
  mountProbeScratch_.close();
  exactEntryScratch_.close();
  exactDirectoryScratch_.close();
  accessMode_ = accessMode;
  return true;
}

uint8_t MediaFsStorage::cardType()
{
  if (!mounted_ || !fs_.card()) return MEDIA_CARD_NONE;
  uint8_t type = fs_.card()->type();
  if (type == SD_CARD_TYPE_SD1) return MEDIA_CARD_SDV1;
  if (type == SD_CARD_TYPE_SD2) return MEDIA_CARD_SDV2;
  if (type == SD_CARD_TYPE_SDHC) {
    return cardSize() > (32ULL * 1024ULL * 1024ULL * 1024ULL)
               ? MEDIA_CARD_SDXC : MEDIA_CARD_SDHC;
  }
  return MEDIA_CARD_NONE;
}

uint64_t MediaFsStorage::cardSize()
{
  if (!mounted_ || !fs_.card()) return 0;
  return static_cast<uint64_t>(fs_.card()->sectorCount()) * 512ULL;
}

const char *MediaFsStorage::fileSystemName() const
{
  if (!mounted_) return "NONE";
  uint8_t type = fs_.fatType();
  if (type == FAT_TYPE_EXFAT) return "exFAT";
  if (type == FAT_TYPE_FAT32) return "FAT32";
  if (type == FAT_TYPE_FAT16) return "FAT16";
  return "UNKNOWN";
}

uint8_t MediaFsStorage::errorCode()
{
  return fs_.sdErrorCode();
}

uint32_t MediaFsStorage::errorData()
{
  return fs_.sdErrorData();
}

bool MediaFsStorage::mounted() const
{
  return mounted_;
}

MediaFsAccessMode MediaFsStorage::accessMode() const
{
  return accessMode_;
}

bool MediaFsStorage::transferWriteAllowed() const
{
  return mounted_ &&
         accessMode_ == MediaFsAccessMode::TransferReadWrite;
}
