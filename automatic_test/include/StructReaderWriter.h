#ifndef STRUCT_READER_WRITER_H
#define STRUCT_READER_WRITER_H

#include <exception>
#include <stdint.h>

#include "ReaderWriter.h"

/** Implementation of the ReaderWriter using a struct.
 */
class StructReaderWriter: public ReaderWriter {
public:
    StructReaderWriter(std::string const & deviceFileName);

    /// The actual read implementation with struct
    int32_t readSingle(uint64_t offset, uint32_t bar, uint32_t count);
    /// A loop around readSingle
    void readArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords,
            int32_t * readBuffer);

    /// The actual write implementation with struct
    void writeSingle(uint64_t offset, uint32_t bar, uint32_t count, int32_t value);
    /// A loop around writeSingle
    void writeArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords,
            int32_t const * writeBuffer);
};

#endif //STRUCT_READER_WRITER_H
