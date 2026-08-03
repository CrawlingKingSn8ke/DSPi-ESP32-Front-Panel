#pragma once

#include <Arduino.h>
#include <SPI.h>

// The ESP32 Arduino core provides its own FS.h/File type. This project uses
// SdFat's FsFile explicitly, so suppress SdFat's harmless compatibility warning.
#ifndef DISABLE_FS_H_WARNING
#define DISABLE_FS_H_WARNING
#endif
#include <SdFat.h>

// UTF-8 media paths are retained end-to-end. The large browser/queue arrays
// that carry these fields are allocated in PSRAM by the UI, so long exFAT paths
// do not consume the ESP32-S3's normal internal heap.
constexpr size_t MEDIA_FS_PATH_CAPACITY = 512;
constexpr size_t MEDIA_FS_NAME_CAPACITY = 512;

enum MediaSdCardType : uint8_t {
  MEDIA_CARD_NONE = 0,
  MEDIA_CARD_SDV1,
  MEDIA_CARD_SDV2,
  MEDIA_CARD_SDHC,
  MEDIA_CARD_SDXC
};

enum class MediaFsAccessMode : uint8_t {
  Unmounted = 0,
  NormalReadOnly,
  TransferReadWrite
};

class MediaFsStorage;

// Small compatibility wrapper around SdFat's FsFile. It exposes the subset of
// the former Arduino file API used by the media player while retaining the full
// path of a directory entry (SdFat itself exposes the entry name, not its full
// path).
class MediaFsFile {
public:
  MediaFsFile() = default;
  ~MediaFsFile();

  MediaFsFile(const MediaFsFile &) = delete;
  MediaFsFile &operator=(const MediaFsFile &) = delete;

  MediaFsFile(MediaFsFile &&other) noexcept;
  MediaFsFile &operator=(MediaFsFile &&other) noexcept;

  explicit operator bool() const;
  bool isDirectory() const;
  const char *path() const;
  const char *name() const;

  size_t read(uint8_t *buffer, size_t length);
  int read();
  size_t write(const uint8_t *buffer, size_t length);
  bool preAllocate(uint64_t length);
  bool truncate(uint64_t length);
  bool seek(uint64_t position);
  uint64_t position() const;
  uint64_t size() const;
  int available() const;
  bool sync();
  bool rewindDirectory();
  void close();
  bool hadIoError() const;
  void clearIoError();

  // Fill a caller-owned result to avoid placing another 1 KiB path/name
  // wrapper on loopTask while scanning large folders.
  bool openNextFile(MediaFsFile &result,
                    uint32_t *skippedEntries = nullptr);
  // Return the next exact directory entry for recursive deletion. Files are
  // reopened O_RDWR because SdFat exFAT remove() rejects read-only handles;
  // directories remain O_RDONLY because SdFat does not open subdirectories
  // writable and rmdir() promotes the handle internally.
  bool openNextFileForDelete(MediaFsFile &result);
  MediaFsFile openNextFile(uint32_t *skippedEntries = nullptr);

private:
  friend class MediaFsStorage;

  void moveFrom(MediaFsFile &other);
  bool setPath(const char *path);
  bool setChildPath(const char *parentPath, const char *childName);
  bool openNextFileWithFlags(MediaFsFile &result,
                             uint32_t *skippedEntries,
                             oflag_t openFlags);

  FsFile file_;
  char path_[MEDIA_FS_PATH_CAPACITY] = {0};
  char name_[MEDIA_FS_NAME_CAPACITY] = {0};
  bool ioError_ = false;
};

class MediaFsStorage {
public:
  bool begin(uint8_t chipSelectPin, SPIClass &spi, uint32_t frequencyHz,
             const char *mountPoint = "/sd", uint8_t maximumOpenFiles = 8,
             bool formatIfMountFailed = false,
             MediaFsAccessMode accessMode =
                 MediaFsAccessMode::NormalReadOnly);
  void end();

  MediaFsFile open(const char *path);
  // Open an existing exact normal file O_RDWR for permanent deletion.
  // Available only while transfer mode owns the SD card read/write.
  MediaFsFile openTransferDeleteFile(const char *path);
  MediaFsFile createTransferFileExclusive(const char *path);
  bool exists(const char *path);
  bool makeTransferDirectory(const char *path);
  bool renameTransferFile(const char *temporaryPath, const char *finalPath);
  bool removeIncompleteTransferFile(const char *path);
  // Handle-based deletion preserves exact exFAT names, including legacy
  // components ending in spaces or dots that path-based FAT lookup can
  // normalise to a different sibling. Callers must hold the shared SPI lock.
  bool removeTransferFileHandle(MediaFsFile &file);
  bool removeTransferDirectoryHandle(MediaFsFile &directory);
  bool syncTransferDevice();
  bool freeSpaceBytes(uint64_t &bytes);
  // Bounded callers hold the shared SPI lock while invoking this probe.  It
  // validates the existing SdFat session without creating another 1 KiB
  // MediaFsFile wrapper on loopTask.
  bool mountedCardUsable();
  // SdFat does not have separate read-only and read/write mount objects in
  // this project.  AccessMode is the ownership gate enforced by the wrapper.
  // Once every media/browser handle is closed, the same mounted SdFs instance
  // can therefore be handed to transfer mode without tearing down and
  // restarting the shared LCD/SD SPI peripheral.
  bool setAccessMode(MediaFsAccessMode accessMode);

  uint8_t cardType();
  uint64_t cardSize();
  const char *fileSystemName() const;
  uint8_t errorCode();
  uint32_t errorData();
  bool mounted() const;
  MediaFsAccessMode accessMode() const;

private:
  bool transferWriteAllowed() const;
  bool openExactPathFallback(const char *path, MediaFsFile &result,
                             oflag_t finalOpenFlags);

  SdFs fs_;
  // SdFat's path parser follows FAT-style normalisation rules.  Some existing
  // music libraries contain legal exFAT directory names with trailing spaces;
  // those entries can be enumerated but cannot always be reopened by a single
  // text path.  Keep the exact-component traversal workspace off loopTask.
  FsFile exactDirectoryScratch_;
  FsFile exactEntryScratch_;
  FsFile mountProbeScratch_;
  char exactComponentScratch_[MEDIA_FS_NAME_CAPACITY] = {0};
  char exactNameScratch_[MEDIA_FS_NAME_CAPACITY] = {0};
  bool mounted_ = false;
  MediaFsAccessMode accessMode_ = MediaFsAccessMode::Unmounted;
};

extern MediaFsStorage mediaFs;
