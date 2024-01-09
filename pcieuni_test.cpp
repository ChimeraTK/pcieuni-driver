#include <gpcieuni/pcieuni_io.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <ctype.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* useconds from struct timeval */
#define MIKRS(tv) (((double)(tv).tv_usec) + ((double)(tv).tv_sec * 1000000.0))
#define MILLS(tv) (((double)(tv).tv_usec / 1000) + ((double)(tv).tv_sec * 1000.0))

int fd;
struct timeval start_time;
struct timeval end_time;

int main(int argc, char* argv[]) {
  int ch_in = 0;
  char nod_name[15] = "";
  device_rw l_Read;
  device_ioctrl_data io_RW;
  device_ioctrl_dma DMA_RW;
  device_ioctrl_time DMA_TIME;
  int tmp_mode;
  u_int tmp_offset;
  int tmp_data;
  int tmp_barx;
  float tmp_fdata;
  int len = 0;
  int k = 0;
  int itemsize = 0;
  int* tmp_dma_buf;
  u_int tmp_size;
  int code = 0;
  double time_tmp = 0;
  double time_dlt;
  int tmp_print = 0;
  int tmp_print_start = 0;
  int tmp_print_stop = 0;

  itemsize = sizeof(device_rw);
  printf("ITEMSIZE %i \n", itemsize);

  if(argc == 1) {
    printf("Input \"prog /dev/damc0\" \n");
    return 0;
  }

  strncpy(nod_name, argv[1], sizeof(nod_name));
  fd = open(nod_name, O_RDWR);
  if(fd < 0) {
    printf("#CAN'T OPEN FILE \n");
    exit(1);
  }

  while(ch_in != 11) {
    printf("\n READ (1) or WRITE (0) ?-");
    printf("\n GET DRIVER VERSION (2) or GET FIRMWARE VERSION (3) ?-");
    printf("\n GET SLOT NUM (4) or GET_DMA_TIME (5) or GET_INFO (6) ?-");
    printf("\n CTRL_DMA READ (30) CTRL_DMA WRITE (31) ?-");
    printf("\n END (11) ?-");
    scanf("%d", &ch_in);
    fflush(stdin);
    l_Read.offset_rw = 0;
    l_Read.data_rw = 0;
    l_Read.mode_rw = 0;
    l_Read.barx_rw = 0;
    l_Read.size_rw = 0;
    l_Read.rsrvd_rw = 0;
    switch(ch_in) {
      case 0:
        printf("\n INPUT  BARx (0,1,2,3...)  -");
        scanf("%x", &tmp_barx);
        fflush(stdin);

        printf("\n INPUT  MODE  (0-D8,1-D16,2-D32)  -");
        scanf("%x", &tmp_mode);
        fflush(stdin);

        printf("\n INPUT  ADDRESS (IN HEX)  -");
        scanf("%x", &tmp_offset);
        fflush(stdin);

        printf("\n INPUT DATA (IN HEX)  -");
        scanf("%x", &tmp_data);
        fflush(stdin);

        l_Read.data_rw = tmp_data;
        l_Read.offset_rw = tmp_offset;
        l_Read.mode_rw = tmp_mode;
        l_Read.barx_rw = tmp_barx;
        l_Read.size_rw = 0;
        l_Read.rsrvd_rw = 0;

        printf("MODE - %X , OFFSET - %X, DATA - %X\n", l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);

        len = write(fd, &l_Read, sizeof(device_rw));
        if(len != itemsize) {
          printf("#CAN'T READ FILE return %i\n", len);
        }

        break;
      case 1:
        printf("\n INPUT  BARx (0,1,2,3)  -");
        scanf("%x", &tmp_barx);
        fflush(stdin);
        printf("\n INPUT  MODE  (0-D8,1-D16,2-D32)  -");
        scanf("%x", &tmp_mode);
        fflush(stdin);
        printf("\n INPUT OFFSET (IN HEX)  -");
        scanf("%x", &tmp_offset);
        fflush(stdin);
        l_Read.data_rw = 0;
        l_Read.offset_rw = tmp_offset;
        l_Read.mode_rw = tmp_mode;
        l_Read.barx_rw = tmp_barx;
        l_Read.size_rw = 0;
        l_Read.rsrvd_rw = 0;
        printf("MODE - %X , OFFSET - %X, DATA - %X\n", l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);
        len = read(fd, &l_Read, sizeof(device_rw));
        if(len != itemsize) {
          printf("#CAN'T READ FILE return %i\n", len);
        }

        printf("READED : MODE - %X , OFFSET - %X, DATA - %X\n", l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);
        break;
      case 2:
        ioctl(fd, PCIEUNI_DRIVER_VERSION, &io_RW);
        tmp_fdata = (float)((float)io_RW.offset / 10.0);
        tmp_fdata += (float)io_RW.data;
        printf("DRIVER VERSION IS %f\n", tmp_fdata);
        break;
      case 3:
        ioctl(fd, PCIEUNI_FIRMWARE_VERSION, &io_RW);
        printf("FIRMWARE VERSION IS - %X\n", io_RW.data);
        break;
      case 4:
        ioctl(fd, PCIEUNI_PHYSICAL_SLOT, &io_RW);
        printf("SLOT NUM IS - %X\n", io_RW.data);
        break;
      case 5:
        len = ioctl(fd, PCIEUNI_GET_DMA_TIME, &DMA_TIME);
        if(len) {
          printf("######ERROR GET TIME %d\n", len);
        }
        printf("===========DRIVER TIME \n");
        printf("STOP DRIVER TIME START %li:%li STOP %li:%li\n", DMA_TIME.start_time.tv_sec, DMA_TIME.start_time.tv_usec,
            DMA_TIME.stop_time.tv_sec, DMA_TIME.stop_time.tv_usec);
        break;
      case 6:
        l_Read.offset_rw = 0;
        l_Read.data_rw = 0;
        l_Read.mode_rw = 4;
        l_Read.barx_rw = 0;
        l_Read.size_rw = 0;
        l_Read.rsrvd_rw = 0;

        len = read(fd, &l_Read, sizeof(device_rw));
        if(len != itemsize) {
          printf("#CAN'T READ FILE return %i\n", len);
        }

        printf("READED : DRV_VERSION - %i.%i \n", l_Read.data_rw, l_Read.offset_rw);
        printf("READED : FRM_VERSION - %i\n", l_Read.mode_rw);
        printf("READED : SLOT_NUM    - %i \n", l_Read.size_rw);
        printf("READED : DRV_MEMS    - %X \n", l_Read.barx_rw);
        for(k = 0; k < 6; k++) {
          printf("BAR Nm %i - %i\n", k, ((l_Read.barx_rw >> k) & 0x1));
        }
        break;
      case 30:
        DMA_RW.dma_offset = 0;
        DMA_RW.dma_size = 0;
        DMA_RW.dma_cmd = 0;
        DMA_RW.dma_pattern = 0;
        printf("\n INPUT  DMA_SIZE (num of sumples (int))  -");
        scanf("%d", &tmp_size);
        fflush(stdin);
        DMA_RW.dma_size = sizeof(int) * tmp_size;
        printf("\n INPUT OFFSET (int)  -");
        scanf("%d", &tmp_offset);
        fflush(stdin);
        DMA_RW.dma_offset = tmp_offset;

        printf("DMA_OFFSET - %X, DMA_SIZE - %X\n", DMA_RW.dma_offset, DMA_RW.dma_size);
        printf("MAX_MEM- %X, DMA_MEM - %X:%X\n", 536870912, (DMA_RW.dma_offset + DMA_RW.dma_size),
            (DMA_RW.dma_offset + DMA_RW.dma_size * 4));

        tmp_dma_buf = new int[tmp_size + DMA_DATA_OFFSET];
        memcpy(tmp_dma_buf, &DMA_RW, sizeof(device_ioctrl_dma));

        gettimeofday(&start_time, 0);
        code = ioctl(fd, PCIEUNI_READ_DMA, tmp_dma_buf);
        gettimeofday(&end_time, 0);
        printf("===========READED  CODE %i\n", code);
        time_tmp = MIKRS(end_time) - MIKRS(start_time);
        time_dlt = MILLS(end_time) - MILLS(start_time);
        printf("STOP READING TIME %fms : %fmks  SIZE %lu\n", time_dlt, time_tmp, (sizeof(int) * tmp_size));
        printf("STOP READING KBytes/Sec %f\n", ((sizeof(int) * tmp_size * 1000) / time_tmp));
        code = ioctl(fd, PCIEUNI_GET_DMA_TIME, &DMA_TIME);
        if(code) {
          printf("######ERROR GET TIME %d\n", code);
        }
        printf("===========DRIVER TIME \n");
        time_tmp = MIKRS(DMA_TIME.stop_time) - MIKRS(DMA_TIME.start_time);
        time_dlt = MILLS(DMA_TIME.stop_time) - MILLS(DMA_TIME.start_time);
        printf("STOP DRIVER TIME START %li:%li STOP %li:%li\n", DMA_TIME.start_time.tv_sec, DMA_TIME.start_time.tv_usec,
            DMA_TIME.stop_time.tv_sec, DMA_TIME.stop_time.tv_usec);
        printf("STOP DRIVER READING TIME %fms : %fmks  SIZE %lu\n", time_dlt, time_tmp, (sizeof(int) * tmp_size));
        printf("STOP DRIVER READING KBytes/Sec %f\n", ((sizeof(int) * tmp_size * 1000) / time_tmp));
        printf("PRINT (0 NO, 1 YES)  -\n");
        scanf("%d", &tmp_print);
        fflush(stdin);
        while(tmp_print) {
          printf("START POS  -\n");
          scanf("%d", &tmp_print_start);
          fflush(stdin);
          printf("STOP POS  -\n");
          scanf("%d", &tmp_print_stop);
          fflush(stdin);
          k = tmp_print_start * 4;
          for(int i = tmp_print_start; i < tmp_print_stop; i++) {
            printf("NUM %i OFFSET %X : DATA %X\n", i, k, (u_int)(tmp_dma_buf[i] & 0xFFFFFFFF));
            k += 4;
          }
          printf("PRINT (0 NO, 1 YES)  -\n");
          scanf("%d", &tmp_print);
          fflush(stdin);
        }
        if(tmp_dma_buf) delete tmp_dma_buf;
        break;
      default:
        break;
    }
  }

  close(fd);
  return 0;
}
