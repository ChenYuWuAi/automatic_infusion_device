/**************************************************************************
 *  @file     pn532_rpi.c
 *  @author   Yehui from Waveshare
 *  @license  BSD
 *  
 *  This implements the peripheral interfaces.
 *  
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>

#include "serial.h"
#include "pn532_rpi.h"

#define _RESET_PIN                      (20)
#define _REQ_PIN                        (16)

static int fd = 0;

void delay(int ms)
{
    struct timespec remaining, request = {.tv_sec = 0, .tv_nsec = ms * 1000};
    nanosleep(&request, &remaining);
}

int serialOpen(const char * dev, int baudrate)
{
    char protocol[30];
    sprintf(protocol, "%d,8,n,1", baudrate);
    return serial_open(dev, protocol);
}

/**************************************************************************
 * Reset and Log implements
 **************************************************************************/
void PN532_Log(const char* log) {
    printf(log);
    printf("\r\n");
}
/**************************************************************************
 * UART
 **************************************************************************/
int PN532_UART_ReadData(uint8_t* data, uint16_t count) {
    int index = 0;
    int length = count; // length of frame (data[3]) might be shorter than the count
    while (index < 4) {
        if (serial_has_buffer(fd, 1) > 0) {
            serial_recv(fd, data + index, 1, 0);
            index++;
        } else {
            delay(5);
        }
    }
    if (data[3] != 0) {
        length = data[3] + 7;
    }
    while (index < length) {
        if (serial_has_buffer(fd, 1) > 0) {
            serial_recv(fd, data + index, 1, 0);
            if (index == 3 && data[index] != 0) {
                length = data[index] + 7;
            }
            index++;
        } else {
            delay(5);
        }
    }
    return PN532_STATUS_OK;
}

int PN532_UART_WriteData(uint8_t *data, uint16_t count) {
    // clear FIFO queue of UART
    serial_flush(fd, 1, 1);
    serial_send(fd, data, count);
    return PN532_STATUS_OK;
}

bool PN532_UART_WaitReady(uint32_t timeout) {
    struct timespec timenow;
    struct timespec timestart;
    clock_gettime(CLOCK_MONOTONIC, &timestart);
    while (1) {
        if (serial_has_buffer(fd, 1) > 0) {
            return true;
        } else {
            delay(50);
        }
        clock_gettime(CLOCK_MONOTONIC, &timenow);
        if ((timenow.tv_sec - timestart.tv_sec) * 1000 + \
            (timenow.tv_nsec - timestart.tv_nsec) / 1000000 > timeout) {
            break;
        }
    }
    // Time out!
    return false;
}

int PN532_UART_Wakeup(void) {
    // Send any special commands/data to wake up PN532
//    uint8_t data[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00,
//                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//                      0x00, 0x00, 0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00};
    uint8_t data[] = {0x55, 0x00, 0x00, 0x55};
    serial_send(fd, data, sizeof(data));
    delay(50);
    return PN532_STATUS_OK;
}

void PN532_UART_Close(void)
{
    serial_close(fd);
    fd = -1;
}

void PN532_UART_Init(PN532* pn532) {
    // init the pn532 functions
    pn532->read_data = PN532_UART_ReadData;
    pn532->write_data = PN532_UART_WriteData;
    pn532->wait_ready = PN532_UART_WaitReady;
    pn532->wakeup = PN532_UART_Wakeup;
    pn532->close = PN532_UART_Close;
    pn532->log = PN532_Log;
    // UART setup
    fd = serialOpen("/dev/ttyUSB0", 115200);
    if (fd < 0) {
        fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
        return;
    }
    // hardware wakeup
    pn532->wakeup();
}
/**************************************************************************
 * End: UART
 **************************************************************************/
