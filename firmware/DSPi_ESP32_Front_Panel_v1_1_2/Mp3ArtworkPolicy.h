#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Mp3ArtworkPolicy {

enum class Id3TagStatus : uint8_t {
  Valid = 0,
  TooShort,
  NotId3,
  UnsupportedVersion,
  MalformedSynchsafe,
  Unsynchronised,
  UnsupportedV22Compression,
  Empty,
  TooLarge,
  Truncated
};

struct Id3TagInfo {
  Id3TagStatus status = Id3TagStatus::TooShort;
  uint8_t version = 0;
  uint32_t payloadBytes = 0;
  uint64_t tagEnd = 0;
};

inline uint32_t readSynchsafe32(const uint8_t *p) {
  return ((uint32_t)(p[0] & 0x7FU) << 21) |
         ((uint32_t)(p[1] & 0x7FU) << 14) |
         ((uint32_t)(p[2] & 0x7FU) << 7) |
         (uint32_t)(p[3] & 0x7FU);
}

inline Id3TagInfo inspectId3TagHeader(const uint8_t *header,
                                      size_t headerBytes,
                                      uint64_t fileBytes,
                                      uint32_t maximumMetadataBytes) {
  Id3TagInfo result;
  if (!header || headerBytes < 10U || fileBytes < 10U) {
    result.status = Id3TagStatus::TooShort;
    return result;
  }
  if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
    result.status = Id3TagStatus::NotId3;
    return result;
  }
  result.version = header[3];
  if (result.version < 2U || result.version > 4U) {
    result.status = Id3TagStatus::UnsupportedVersion;
    return result;
  }
  for (size_t i = 6; i < 10; ++i) {
    if ((header[i] & 0x80U) != 0U) {
      result.status = Id3TagStatus::MalformedSynchsafe;
      return result;
    }
  }
  if ((header[5] & 0x80U) != 0U) {
    result.status = Id3TagStatus::Unsynchronised;
    return result;
  }
  if (result.version == 2U && (header[5] & 0x40U) != 0U) {
    result.status = Id3TagStatus::UnsupportedV22Compression;
    return result;
  }
  result.payloadBytes = readSynchsafe32(header + 6);
  result.tagEnd = 10ULL + result.payloadBytes;
  if (result.payloadBytes == 0U) {
    result.status = Id3TagStatus::Empty;
    return result;
  }
  if (result.payloadBytes > maximumMetadataBytes) {
    result.status = Id3TagStatus::TooLarge;
    return result;
  }
  if (result.tagEnd > fileBytes) {
    result.status = Id3TagStatus::Truncated;
    return result;
  }
  result.status = Id3TagStatus::Valid;
  return result;
}

enum class JpegCoding : uint8_t {
  Baseline = 0,
  Progressive,
  Unsupported,
  Malformed,
  NotJpeg
};

inline bool isStandaloneMarker(uint8_t marker) {
  return marker == 0x01U || (marker >= 0xD0U && marker <= 0xD9U);
}

inline bool isStartOfFrame(uint8_t marker) {
  switch (marker) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3:
    case 0xC5: case 0xC6: case 0xC7:
    case 0xC9: case 0xCA: case 0xCB:
    case 0xCD: case 0xCE: case 0xCF:
      return true;
    default:
      return false;
  }
}

// Inspect the encoded image payload extracted from an APIC frame. This is a
// bounded marker walk only; the hardware decoder remains authoritative.
inline JpegCoding classifyApicJpeg(const uint8_t *data, size_t length) {
  if (!data || length < 4U || data[0] != 0xFFU || data[1] != 0xD8U) {
    return JpegCoding::NotJpeg;
  }

  size_t cursor = 2U;
  while (cursor < length) {
    while (cursor < length && data[cursor] == 0xFFU) ++cursor;
    if (cursor >= length) return JpegCoding::Malformed;
    const uint8_t marker = data[cursor++];
    if (marker == 0xD9U) return JpegCoding::Malformed;  // EOI before SOF.
    if (marker == 0xDAU) return JpegCoding::Malformed;  // SOS before SOF.
    if (isStartOfFrame(marker)) {
      if (cursor + 2U > length) return JpegCoding::Malformed;
      const uint16_t segmentLength =
          (uint16_t)((uint16_t)data[cursor] << 8) | data[cursor + 1U];
      if (segmentLength < 8U || cursor + segmentLength > length) {
        return JpegCoding::Malformed;
      }
      if (marker == 0xC0U) return JpegCoding::Baseline;
      if (marker == 0xC2U) return JpegCoding::Progressive;
      return JpegCoding::Unsupported;
    }
    if (isStandaloneMarker(marker)) continue;
    if (cursor + 2U > length) return JpegCoding::Malformed;
    const uint16_t segmentLength =
        (uint16_t)((uint16_t)data[cursor] << 8) | data[cursor + 1U];
    if (segmentLength < 2U || cursor + segmentLength > length) {
      return JpegCoding::Malformed;
    }
    cursor += segmentLength;
  }
  return JpegCoding::Malformed;
}

inline const char *jpegCodingName(JpegCoding coding) {
  switch (coding) {
    case JpegCoding::Baseline: return "baseline";
    case JpegCoding::Progressive: return "progressive";
    case JpegCoding::Unsupported: return "unsupported";
    case JpegCoding::Malformed: return "malformed";
    case JpegCoding::NotJpeg: return "not-jpeg";
  }
  return "unknown";
}

}  // namespace Mp3ArtworkPolicy
