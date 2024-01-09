#include "ReaderWriter.h"

#include <sys/ioctl.h>

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

ReaderWriter::ReaderWriter(std::string const& deviceFileName) {
  _fileDescriptor = open(deviceFileName.c_str(), O_RDWR);

  if(_fileDescriptor <= 0) {
    std::stringstream errorMessage;
    errorMessage << "Could not open file " << deviceFileName << ". Check that the udev rules are installed and the "
                 << "kernel module is loaded!";
    throw DeviceIOException(errorMessage.str());
  }
}

ReaderWriter::~ReaderWriter() {
  close(_fileDescriptor);
}

void ReaderWriter::ioctlExec(uint32_t request, void* data) {
  int retCode;
  if((retCode = ioctl(_fileDescriptor, request, data)) < 0) {
    std::stringstream errorMessage;
    errorMessage << "Error executing ioctl call. Request: " << request << "Return code: " << retCode;
    throw DeviceIOException(errorMessage.str());
  }
}

void ReaderWriter::procFileTest(std::string const& procFileName) {
  std::ifstream procFile;

  procFile.open(procFileName.c_str());

  if(!procFile.is_open()) {
    std::stringstream errorMessage;
    errorMessage << "Could not open proc file " << procFileName << ". Check that the kernel module is loaded!";
    throw DeviceIOException(errorMessage.str());
  }

  procFile.get();
  while(procFile.good()) {
    procFile.get();
  }

  procFile.close();
}
