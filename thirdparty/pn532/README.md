# pn532c
PN532 simple driver in C / Unix / Linux

This project uses the code from [this](https://www.waveshare.com/wiki/PN532_NFC_HAT) link with an additional serial port driver which i wrote myself.

It was written to work with Raspberry pi but now it can be used in any Unix/Linux operating systems with `/dev/ttyUSBx` or `/dev/ttySx` devices.

Just change `/dev/ttyUSB0` in `pn532_rpi.c` to any port you want to use.
