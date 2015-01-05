	#include "StructReaderWriter.h"

#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <cstdio>

StructReaderWriter::StructReaderWriter(std::string const & deviceFileName) :
        ReaderWriter(deviceFileName) {
}

int32_t StructReaderWriter::readSingle(uint64_t offset, uint32_t bar, uint32_t count) {
    int32_t data = 0;

    if (pread(_fileDescriptor, &data, count, offset) != count) {
        throw DeviceIOException("Error reading from device");
    }

    return data;
}

void StructReaderWriter::readArea(uint64_t offset, uint32_t bar, uint32_t count,
        uint32_t nWords, int32_t * readBuffer) {
    for (uint32_t i = 0; i < nWords; ++i) {
        readBuffer[i] = readSingle(offset + i, count, bar);
    }
}

void StructReaderWriter::writeSingle(uint64_t offset, uint32_t bar, uint32_t count,
        int32_t value) {

    if (pwrite(_fileDescriptor, &value, count, offset)
            != count) {
        throw DeviceIOException("Error writing to device");
    }
}

void StructReaderWriter::writeArea(uint64_t offset, uint32_t bar, uint32_t count,
        uint32_t nWords, int32_t const * writeBuffer) {
    for (uint32_t i = 0; i < nWords; ++i) {
        writeSingle(offset + i * count, count, bar, writeBuffer[i]);
    }
}
