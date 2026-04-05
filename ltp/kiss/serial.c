/*
	serial.c:	Serial port operations for KISS TNC
			communication using POSIX termios.

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.
									*/

#include "ltpkisslsa.h"

/*	mapBaudRate: convert integer baud rate to termios constant.	*/

static speed_t	mapBaudRate(int rate)
{
	switch (rate)
	{
	case 1200:	return B1200;
	case 2400:	return B2400;
	case 4800:	return B4800;
	case 9600:	return B9600;
	case 19200:	return B19200;
	case 38400:	return B38400;
	case 57600:	return B57600;
	case 115200:	return B115200;
	default:	return B9600;
	}
}

/*	openSerialPort: open and configure a serial port.
 *
 *	Configures the port for raw mode, 8N1, with optional
 *	hardware flow control.  Uses non-blocking reads with
 *	VMIN=0, VTIME=1 (100ms timeout).
 *
 *	Returns the file descriptor, or -1 on error.			*/

int	openSerialPort(KissConfig *config)
{
	int		fd;
	struct termios	tty;
	speed_t		speed;

	if (config == NULL)
	{
		return -1;
	}

	fd = open(config->devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
	{
		putSysErrmsg("Can't open serial port", config->devicePath);
		return -1;
	}

	/*	Clear the non-blocking flag after open; we'll use
	 *	termios VMIN/VTIME for read timing instead.		*/

	{
		int	flags;

		flags = fcntl(fd, F_GETFL, 0);
		if (flags >= 0)
		{
			fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	/*	Get current terminal attributes.			*/

	if (tcgetattr(fd, &tty) != 0)
	{
		putSysErrmsg("Can't get terminal attributes",
				config->devicePath);
		close(fd);
		return -1;
	}

	/*	Set baud rate.						*/

	speed = mapBaudRate(config->baudRate);
	cfsetispeed(&tty, speed);
	cfsetospeed(&tty, speed);

	/*	Configure for raw mode, 8N1.				*/

	cfmakeraw(&tty);

	tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
	tty.c_cflag |= CS8;		/*	8 data bits.		*/
	tty.c_cflag |= CLOCAL;		/*	Ignore modem control.	*/
	tty.c_cflag |= CREAD;		/*	Enable receiver.	*/

	/*	Hardware flow control.					*/

	if (config->useFlowControl)
	{
		tty.c_cflag |= CRTSCTS;
	}
	else
	{
		tty.c_cflag &= ~CRTSCTS;
	}

	/*	Non-blocking read with 100ms timeout.
	 *	VMIN=0, VTIME=1 means read returns immediately if
	 *	data is available, or after 100ms if not.		*/

	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;		/*	100ms timeout.		*/

	/*	Apply settings.						*/

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		putSysErrmsg("Can't set terminal attributes",
				config->devicePath);
		close(fd);
		return -1;
	}

	/*	Flush any stale data.					*/

	tcflush(fd, TCIOFLUSH);

	return fd;
}

/*	closeSerialPort: close a serial port.				*/

void	closeSerialPort(int fd)
{
	if (fd >= 0)
	{
		close(fd);
	}
}

/*	serialSend: write data to the serial port.
 *
 *	Handles partial writes by retrying.
 *
 *	Returns the number of bytes written, or -1 on error.		*/

int	serialSend(int fd, unsigned char *data, int length)
{
	int	totalSent = 0;
	int	bytesWritten;

	if (fd < 0 || data == NULL || length <= 0)
	{
		return -1;
	}

	while (totalSent < length)
	{
		bytesWritten = write(fd, data + totalSent,
				length - totalSent);
		if (bytesWritten < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}

			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				microsnooze(10000);	/*	10ms.	*/
				continue;
			}

			putSysErrmsg("Serial write failed", NULL);
			return -1;
		}

		totalSent += bytesWritten;
	}

	return totalSent;
}

/*	serialReceive: read data from the serial port.
 *
 *	Non-blocking read; returns whatever data is available
 *	up to maxLen bytes.  Uses the VTIME timeout configured
 *	in openSerialPort.
 *
 *	Returns the number of bytes read, 0 if no data, or
 *	-1 on error.							*/

int	serialReceive(int fd, unsigned char *buf, int maxLen)
{
	int	bytesRead;

	if (fd < 0 || buf == NULL || maxLen <= 0)
	{
		return -1;
	}

	bytesRead = read(fd, buf, maxLen);
	if (bytesRead < 0)
	{
		if (errno == EINTR || errno == EAGAIN
				|| errno == EWOULDBLOCK)
		{
			return 0;
		}

		putSysErrmsg("Serial read failed", NULL);
		return -1;
	}

	return bytesRead;
}
