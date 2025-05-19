/*
	Creative Commons Attribution-NonCommercial-NoDerivs (CC-BY-NC-ND)
	https://creativecommons.org/licenses/by-nc-nd/4.0/
	The most restrictive creative commons license.
	This only allows people to download and share your work for no commercial gain and for no other purposes.
*/


#include "serial.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#define ARRAY_SIZE(x)				(sizeof(x) / sizeof((x)[0]))

static const struct
{
	long rate;
	int code;
} rate_define[] = {
		{50, B50},
		{75, B75},
		{110, B110},
		{134, B134},
		{150, B150},
		{200, B200},
		{300, B300},
		{600, B600},
		{1200, B1200},
		{1800, B1800},
		{2400, B2400},
		{4800, B4800},
		{9600, B9600},
		{19200, B19200},
		{38400, B38400},
		{57600, B57600},
		{115200, B115200},
		{230400, B230400},
		{460800, B460800},
		{500000, B500000},
		{576000, B576000},
		{921600, B921600},
		{1000000, B1000000},
		{1152000, B1152000},
		{1500000, B1500000},
		{2000000, B2000000},
		{2500000, B2500000},
		{3000000, B3000000},
		{3500000, B3500000},
		{4000000, B4000000},
};

int serial_baud_rate_macro(long baud_rate)
{
	for (int i = 0; i < ARRAY_SIZE(rate_define); ++i)
	{
		if (rate_define[i].rate == baud_rate)
		{
			return rate_define[i].code;
		}
	}

	return -1;
}

int serial_close(int fd)
{
	if (fd >= 0)
	{
		serial_flush(fd, 1, 1);
		return close(fd);
	}

	return -1;
}

int serial_flush(int fd, int input, int output)
{
	if (fd < 0)
	{
		return -1;
	}

	if (input)
	{
		return tcflush(fd, output ? TCIOFLUSH : TCIFLUSH);
	}
	else
	{
		return output ? tcflush(fd, TCOFLUSH) : -1;
	}
}

int serial_drain(int fd)
{
	return tcdrain(fd);
}

int serial_has_buffer(int fd, int io)
{
	if (fd < 0)
	{
		return -1;
	}

	int count;
	if (ioctl(fd, io ? TIOCINQ : TIOCOUTQ, &count) < 0)
	{
		return -1;
	}
	else
    {
		return count;
	}
}

int serial_send(int fd, const char * buffer, size_t size)
{
	if (fd < 0)
	{
		return -1;
	}

	return write(fd, buffer, size);
}

int serial_recv(int fd, char * buffer, size_t max_size, int ms)
{
	if (fd < 0)
    {
		return -1;
	}

	if (ms > 0)
	{
		struct timeval time = {.tv_sec = 0, .tv_usec = ms * 1000};

		fd_set set;
		FD_ZERO(&set);
		FD_SET(fd, &set);

		if (select(fd + 1, &set, NULL, NULL, &time) < 0)
		{
			return -1;
		}

		if (FD_ISSET(fd, &set))
		{
			return read(fd, buffer, max_size);
		}
		else
		{
			return 0;
		}
	}

    return read(fd, buffer, max_size);
}

int serial_open(const char * path, const char * protocol)
{
	long rate;
	char bit, p, sbit;
	struct termios newtio;

	if (sscanf(protocol, "%ld,%c,%c,%c", &rate, &bit, &p, &sbit) != 4)
	{
		return -1;
	}

	/* Initialize configuration messages*/
	bzero(&newtio, sizeof (newtio));
	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	/* Close soft flow control*/
	newtio.c_iflag &= ~(ICRNL | IXON | IXOFF);

	/* Close hard flowcontrol*/
	newtio.c_cflag &= ~CRTSCTS;

	/* Set data bits */
	if (bit == '5') newtio.c_cflag |= CS5;
	else if (bit == '6') newtio.c_cflag |= CS6;
	else if (bit == '7') newtio.c_cflag |= CS7;
	else if (bit == '8') newtio.c_cflag |= CS8;
	else return -1;

	/* Set parity */
	if (p == 'n')
	{
		newtio.c_cflag &= ~PARENB;
	}
	else if (p == 'o')
	{
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
	}
	else if (p == 'e')
	{
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
	}
	else
	{
		return -1;
	}

	/* Set stop bits */
	if (sbit == '1') newtio.c_cflag &= ~CSTOPB;
	else if (sbit == '2') newtio.c_cflag |= CSTOPB;
	else return -1;

	/* Set baudrate */
	int baudrate = serial_baud_rate_macro(rate);
	if (baudrate < 0)
	{
		return -1;
	}
	else
	{
		cfsetispeed(&newtio, baudrate);
		cfsetospeed(&newtio, baudrate);
	}

	int fd = open(path, O_RDWR);
	if (fd < 0)
	{
		return -1;
	}

	if (tcsetattr(fd, TCSANOW, &newtio) < 0)
	{
		close(fd);
		return -1;
	}

	return fd;
}
