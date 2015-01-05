#ifndef READER_WRITER_H
#define READER_WRITER_H

#include <exception>
#include <string>
#include <stdint.h>

/** a class to throw which basically is a std::exception, 
 but as it's derived it allows selective catching,
 plus it gets the what() message in the constructor

 */
class DeviceIOException: public std::exception {
protected:
    std::string _message; /**< exception description*/
public:
    DeviceIOException(const std::string & message)
    : _message(message) {
    }
    const char* what() const throw() {
        return _message.c_str();
    }
    ~DeviceIOException() throw() {
    }
};

/** Just a class for testing. 
 The actual read/write functions are purely virtual so a
 version with and without struct can be implemented.
 */
class ReaderWriter {
public:
    ReaderWriter(std::string const & deviceFileName);
    virtual ~ReaderWriter();

    /// The actual read implementation with struct
    virtual int32_t readSingle(uint64_t offset, uint32_t bar, uint32_t count)=0;
    /// A loop around readSingle
    virtual void readArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords,
            int32_t * readBuffer)=0;

    /// The actual write implementation with struct
    virtual void writeSingle(uint64_t offset, uint32_t bar, uint32_t count, int32_t value)=0;
    /// A loop around writeSingle
    virtual void writeArea(uint64_t offset, uint32_t bar, uint32_t count, uint32_t nWords,
            int32_t const * writeBuffer)=0;

    /// A wrapper to the system ioctl data
    virtual void ioctlExec(uint32_t request, void *data);

    /// Test of functionality in /proc folder
    virtual void procFileTest(std::string const & procFileName);
protected:
    int _fileDescriptor;
};

#endif //READER_WRITER_H
