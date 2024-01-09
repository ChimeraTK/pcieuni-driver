#ifndef NORMAL_READER_WRITER_H
#define NORMAL_READER_WRITER_H

#include "ReaderWriter.h"

#include <exception>
#include <stdint.h>

/** Implementation of the ReaderWriter without struct.
 */
class NormalReaderWriter : public ReaderWriter {
 public:
  NormalReaderWriter(std::string const& deviceFileName);

  /// The actual read implementation with normal
  int32_t readSingle(uint64_t offset, uint32_t bar, uint32_t count);
  /// A loop around readSingle
  void readArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords, int32_t* readBuffer);

  /// The actual write implementation without struct
  void writeSingle(uint64_t offset, uint32_t bar, uint32_t count, int32_t value);
  /// A loop around writeSingle
  void writeArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords, int32_t const* writeBuffer);
};

#endif // NORMAL_READER_WRITER_H
