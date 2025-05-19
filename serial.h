/*
	Creative Commons Attribution-NonCommercial-NoDerivs (CC-BY-NC-ND)
	https://creativecommons.org/licenses/by-nc-nd/4.0/
	The most restrictive creative commons license.
	This only allows people to download and share your work for no commercial gain and for no other purposes.
*/


#ifndef SERIAL_H
#define SERIAL_H 1

#include <stdlib.h>

int serial_open(const char * path, const char * protocol);
int serial_send(int fd, const char * buffer, size_t size);
int serial_recv(int fd, char * buffer, size_t max_size, int ms);
int serial_flush(int fd, int input, int output);
int serial_drain(int fd);
int serial_has_buffer(int fd, int io);
int serial_close(int fd);

#endif
