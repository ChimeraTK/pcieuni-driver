#include <boost/test/included/unit_test.hpp>

#include <sstream>
using namespace boost::unit_test_framework;
#include "NormalReaderWriter.h"
#include "ReaderWriter.h"

#include <boost/shared_ptr.hpp>

// we use the defines from the original implementation
#include <gpcieuni/pcieuni_io.h>

#define PCIEUNI_NAME "pcieuni"
#define PCIEUNI_SLOT "s6"

#define AREA_SPI_DIV 0x1000      // bar 0
#define WORD_CLK_MUX 0x80        // bar 0
#define WORD_CLK_SEL 0x9c        // bar 0
#define WORD_RESET_N 0x200       // bar 0
#define AREA_SPI_ADC 0x2000      // bar 0
#define WORD_ADC_ENA 0x100       // bar 0
#define WORD_TIMING_FREQ 0x20    // bar 1
#define WORD_TIMING_INT_ENA 0x10 // bar 1
#define WORD_TIMING_TRG_SEL 0x80 // bar 1
#define WORD_DAQ_ENABLE 0x08     // bar 1

#define MIKRS(tv) (((double)(tv).tv_usec) + ((double)(tv).tv_sec * 1000000.0))

class PcieuniTest {
 public:
  PcieuniTest(boost::shared_ptr<ReaderWriter> const& readerWriter);

  void testUserFunctions();
  void testIoctl();
  void testReadWrite();

 private:
  boost::shared_ptr<ReaderWriter> _readerWriter;
};

template<class T>
class PcieuniTestSuite : public test_suite {
 public:
  PcieuniTestSuite(std::string const& deviceFileName) : test_suite("PCIeuni test suite") {
    // create an instance of the test class
    boost::shared_ptr<ReaderWriter> readerWriter(new T(deviceFileName));

    boost::shared_ptr<PcieuniTest> pcieuniTest(new PcieuniTest(readerWriter));

    add(BOOST_CLASS_TEST_CASE(&PcieuniTest::testIoctl, pcieuniTest));
    add(BOOST_CLASS_TEST_CASE(&PcieuniTest::testReadWrite, pcieuniTest));
    add(BOOST_CLASS_TEST_CASE(&PcieuniTest::testUserFunctions, pcieuniTest));
  }
};

test_suite* init_unit_test_suite(int /*argc*/, char* /*argv*/[]) {
  framework::master_test_suite().p_name.value = "MtcaDummy test suite";
  framework::master_test_suite().add(
      new PcieuniTestSuite<NormalReaderWriter>(std::string("/dev/") + PCIEUNI_NAME + PCIEUNI_SLOT));

  return NULL;
}

PcieuniTest::PcieuniTest(boost::shared_ptr<ReaderWriter> const& readerWriter) : _readerWriter(readerWriter) {}

void PcieuniTest::testIoctl() {
  device_ioctrl_data ioData;
  device_ioctrl_dma dmaData;
  device_ioctrl_time timeData;

  float driverVersion;
  int* dmaBuffer;

  BOOST_CHECK_NO_THROW(_readerWriter->ioctlExec(PCIEUNI_DRIVER_VERSION, &ioData));
  driverVersion = (float)((float)ioData.offset / 10.0);
  driverVersion += (float)ioData.data;
  BOOST_CHECK(driverVersion > 0);
  printf("DRIVER VERSION IS %g\n", driverVersion);

  BOOST_CHECK_NO_THROW(_readerWriter->ioctlExec(PCIEUNI_FIRMWARE_VERSION, &ioData));
  BOOST_CHECK(ioData.data > 0);
  printf("FIRMWARE VERSION IS %X\n", ioData.data);

  BOOST_CHECK_NO_THROW(_readerWriter->ioctlExec(PCIEUNI_PHYSICAL_SLOT, &ioData));
  BOOST_CHECK(ioData.data > 0);
  printf("DEVICE IS IN SLOT %X\n", ioData.data);

  // DMA read test
  dmaData.dma_offset = 0;
  dmaData.dma_size = 1000;
  dmaData.dma_cmd = 0;
  dmaData.dma_pattern = 0;
  dmaBuffer = new int[dmaData.dma_size];
  memcpy(dmaBuffer, &dmaData, sizeof(device_ioctrl_dma));
  BOOST_CHECK_NO_THROW(_readerWriter->ioctlExec(PCIEUNI_READ_DMA, &dmaBuffer));
  delete dmaBuffer;

  // Driver slot and board number
  BOOST_CHECK_NO_THROW(_readerWriter->ioctlExec(PCIEUNI_GET_DMA_TIME, &timeData));
  BOOST_CHECK(timeData.start_time.tv_sec == timeData.stop_time.tv_sec);
  BOOST_CHECK(timeData.start_time.tv_usec == timeData.stop_time.tv_usec);
  printf("DEVICE IN SLOT %ld\n", timeData.start_time.tv_sec);
  printf("DEVICE BOARD NUMBER: %ld\n", timeData.start_time.tv_usec);
}

void PcieuniTest::testReadWrite() {
  // Read and write in designated spots on bar 0 and bar 1
  BOOST_CHECK_NO_THROW(_readerWriter->readSingle(WORD_RESET_N + 0x00, 0, 4));
  BOOST_CHECK_NO_THROW(_readerWriter->readSingle(WORD_TIMING_FREQ + 0x00, 1, 4));
  BOOST_CHECK_NO_THROW(_readerWriter->writeSingle(WORD_RESET_N + 0x00, 0, 4, 1));
  BOOST_CHECK_NO_THROW(_readerWriter->writeSingle(WORD_TIMING_FREQ + 0x00, 1, 4, 81250000));

  // Try to read and write from bar 2, 3, 4 or 5
  BOOST_CHECK_THROW(_readerWriter->readSingle(0, 2, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->readSingle(0, 3, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->readSingle(0, 4, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->readSingle(0, 5, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(0, 2, 4, 1), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(0, 3, 4, 1), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(0, 4, 4, 1), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(0, 5, 4, 1), DeviceIOException);

  // Try to read or write just 3 bytes
  BOOST_CHECK_THROW(_readerWriter->readSingle(WORD_RESET_N, 0, 3), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(WORD_RESET_N, 0, 3, 1), DeviceIOException);

  // Try to read/write to offset that is not a multiple of 4
  BOOST_CHECK_THROW(_readerWriter->readSingle(WORD_RESET_N + 1, 0, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle(WORD_RESET_N + 1, 0, 4, 1), DeviceIOException);

  // Try to read/write over bar 5
  BOOST_CHECK_THROW(_readerWriter->readSingle((6LL) << 60, 0, 4), DeviceIOException);
  BOOST_CHECK_THROW(_readerWriter->writeSingle((6LL) << 60, 0, 4, 1), DeviceIOException);
}

void PcieuniTest::testUserFunctions() {
  BOOST_CHECK_NO_THROW(_readerWriter->procFileTest(std::string("/proc/") + PCIEUNI_NAME + PCIEUNI_SLOT));
}
