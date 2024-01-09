#include "NormalReaderWriter.h"

#include "gpcieuni/pcieuni_io.h"

#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <stdint.h>
#include <unistd.h>

// sorry, this is rather C-Style

NormalReaderWriter::NormalReaderWriter(std::string const& deviceFileName) : ReaderWriter(deviceFileName) {}

int32_t NormalReaderWriter::readSingle(uint64_t offset, uint32_t bar, uint32_t count) {
  int32_t returnValue;
  readArea(offset, bar, count, 1, &returnValue);
  return returnValue;
}

void NormalReaderWriter::readArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords, int32_t* readBuffer) {
  if(bar > 5) {
    throw DeviceIOException("Bar number is too large.");
  }

  off_t virtualOffset = PCIEUNI_BAR_OFFSETS[bar] + offset;

  if(pread(_fileDescriptor, readBuffer, nWords * count, virtualOffset) != nWords * count) {
    throw DeviceIOException("Error reading from device");
  }
}

void NormalReaderWriter::writeSingle(uint64_t offset, uint32_t bar, uint32_t count, int32_t value) {
  writeArea(offset, bar, count, 1, &value);
}

void NormalReaderWriter::writeArea(
    uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords, int32_t const* writeBuffer) {
  if(bar > 5) {
    throw DeviceIOException("Bar number is too large.");
  }

  off_t virtualOffset = PCIEUNI_BAR_OFFSETS[bar] + offset;
  ssize_t writeStatus = pwrite(_fileDescriptor, writeBuffer, nWords * count, virtualOffset);
  if(writeStatus != nWords * count) {
    throw DeviceIOException("Error writing to device");
  }
}
