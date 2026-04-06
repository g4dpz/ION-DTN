/*
 *	serial.c:	Serial port operations for LTP KISS CLA.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#include "ltpkissP.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

/*	Serial Port Functions						*/

int	openSerialPort(const char *device, int baudRate,
		int useFlowControl)
{
	int		fd;
	struct termios	tty;
	int		baudConst;

	/*	Validate parameters					*/
	if (device == NULL)
	{
		putErrmsg("NULL device path in openSerialPort.", NULL);
		return -1;
	}

	/*	Convert baud rate to termios constant			*/
	baudConst = getBaudRateConstant(baudRate);
	if (baudConst < 0)
	{
		putErrmsg("Invalid baud rate in openSerialPort.",
				itoa(baudRate));
		return -1;
	}

	/*	Open serial device					*/
	fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
	{
		putSysErrmsg("Cannot open serial device", device);
		return -1;
	}

	/*	Get current terminal attributes				*/
	if (tcgetattr(fd, &tty) != 0)
	{
		putSysErrmsg("Cannot get terminal attributes", device);
		close(fd);
		return -1;
	}

	/*	Set baud rate						*/
	cfsetospeed(&tty, baudConst);
	cfsetispeed(&tty, baudConst);

	/*	Configure 8N1 mode (8 data bits, no parity, 1 stop bit)*/
	tty.c_cflag &= ~PARENB;		/*	No parity		*/
	tty.c_cflag &= ~CSTOPB;		/*	1 stop bit		*/
	tty.c_cflag &= ~CSIZE;		/*	Clear size bits		*/
	tty.c_cflag |= CS8;		/*	8 data bits		*/

	/*	Configure flow control					*/
	if (useFlowControl)
	{
		tty.c_cflag |= CRTSCTS;	/*	Hardware flow control	*/
		writeMemo("[i] Hardware flow control enabled.");
	}
	else
	{
		tty.c_cflag &= ~CRTSCTS;/*	No hardware flow control*/
	}

	/*	Disable software flow control (XON/XOFF)		*/
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	/*	Configure raw mode (no line processing)			*/
	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tty.c_oflag &= ~OPOST;

	/*	Configure non-blocking reads with timeout		*/
	tty.c_cc[VMIN] = 0;		/*	No minimum chars	*/
	tty.c_cc[VTIME] = 1;		/*	0.1 second timeout	*/

	/*	Enable receiver, ignore modem control lines		*/
	tty.c_cflag |= (CLOCAL | CREAD);

	/*	Apply terminal attributes				*/
	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		putSysErrmsg("Cannot set terminal attributes", device);
		close(fd);
		return -1;
	}

	/*	Flush any stale data					*/
	tcflush(fd, TCIOFLUSH);

	writeMemo("[i] Serial port opened successfully.");
	return fd;
}

int	serialSend(int fd, unsigned char *data, int length)
{
	ssize_t		bytesWritten;
	ssize_t		totalWritten = 0;
	int		remaining;
	unsigned char	*ptr;

	/*	Validate parameters					*/
	if (fd < 0)
	{
		putErrmsg("Invalid file descriptor in serialSend.", NULL);
		return -1;
	}

	if (data == NULL || length < 0)
	{
		putErrmsg("Invalid parameters in serialSend.", NULL);
		return -1;
	}

	if (length == 0)
	{
		return 0;	/*	Nothing to send			*/
	}

	/*	Write data, handling partial writes			*/
	ptr = data;
	remaining = length;

	while (remaining > 0)
	{
		bytesWritten = write(fd, ptr, remaining);

		if (bytesWritten < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				/*	Would block, try again		*/
				usleep(1000);	/*	1ms delay	*/
				continue;
			}
			else if (errno == EINTR)
			{
				/*	Interrupted, retry		*/
				continue;
			}
			else
			{
				/*	Real error			*/
				putSysErrmsg("Write error in serialSend", NULL);
				return -1;
			}
		}
		else if (bytesWritten == 0)
		{
			/*	No data written, unusual		*/
			writeMemo("[?] Zero bytes written in serialSend.");
			usleep(1000);
			continue;
		}

		totalWritten += bytesWritten;
		ptr += bytesWritten;
		remaining -= bytesWritten;
	}

	/*	Wait for transmission to complete			*/
	if (tcdrain(fd) != 0)
	{
		putSysErrmsg("Error draining serial port", NULL);
		/*	Continue anyway, data was written		*/
	}

	return totalWritten;
}

int	serialReceive(int fd, unsigned char *buffer, int maxLen)
{
	ssize_t		bytesRead;
	fd_set		readfds;
	struct timeval	timeout;
	int		result;

	/*	Validate parameters					*/
	if (fd < 0)
	{
		putErrmsg("Invalid file descriptor in serialReceive.", NULL);
		return -1;
	}

	if (buffer == NULL || maxLen <= 0)
	{
		putErrmsg("Invalid parameters in serialReceive.", NULL);
		return -1;
	}

	/*	Use select() for efficient I/O with timeout		*/
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;	/*	100ms timeout		*/

	result = select(fd + 1, &readfds, NULL, NULL, &timeout);

	if (result < 0)
	{
		if (errno == EINTR)
		{
			/*	Interrupted, no data		*/
			return 0;
		}

		putSysErrmsg("Select error in serialReceive", NULL);
		return -1;
	}
	else if (result == 0)
	{
		/*	Timeout, no data available		*/
		return 0;
	}

	/*	Data available, read it					*/
	if (FD_ISSET(fd, &readfds))
	{
		bytesRead = read(fd, buffer, maxLen);

		if (bytesRead < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				/*	No data after all		*/
				return 0;
			}
			else if (errno == EINTR)
			{
				/*	Interrupted			*/
				return 0;
			}
			else
			{
				/*	Real error			*/
				putSysErrmsg("Read error in serialReceive",
						NULL);
				return -1;
			}
		}
		else if (bytesRead == 0)
		{
			/*	EOF or disconnect			*/
			writeMemo("[?] EOF on serial port.");
			return 0;
		}

		return bytesRead;
	}

	return 0;
}

void	closeSerialPort(int fd)
{
	if (fd >= 0)
	{
		/*	Flush output before closing			*/
		tcdrain(fd);

		/*	Close the port					*/
		close(fd);

		writeMemo("[i] Serial port closed.");
	}
}

int	checkSerialPort(int fd)
{
	int	status;

	if (fd < 0)
	{
		return -1;
	}

	/*	Check modem status lines				*/
	if (ioctl(fd, TIOCMGET, &status) < 0)
	{
		putSysErrmsg("Cannot get modem status", NULL);
		return -1;
	}

	/*	Port is accessible					*/
	return 0;
}

int	reconnectSerialPort(SerialPortState *state)
{
	int	newFd;
	int	baudConst;

	if (state == NULL)
	{
		putErrmsg("NULL state in reconnectSerialPort.", NULL);
		return -1;
	}

	/*	Close old descriptor if open				*/
	if (state->fd >= 0)
	{
		close(state->fd);
		state->fd = -1;
	}

	/*	Check reconnection attempts				*/
	state->reconnectAttempts++;

	if (state->reconnectAttempts > 10)
	{
		putErrmsg("Too many reconnection attempts.", NULL);
		return -1;
	}

	/*	Wait before reconnecting				*/
	writeMemo("[i] Attempting to reconnect serial port...");
	sleep(5);	/*	5 second delay				*/

	/*	Convert baud rate					*/
	baudConst = getBaudRateConstant(state->baudRate);
	if (baudConst < 0)
	{
		putErrmsg("Invalid baud rate in reconnect.", NULL);
		return -1;
	}

	/*	Attempt to reopen					*/
	newFd = openSerialPort(state->devicePath, state->baudRate,
			state->useFlowControl);

	if (newFd < 0)
	{
		putErrmsg("Failed to reconnect serial port.", NULL);
		return -1;
	}

	/*	Success							*/
	state->fd = newFd;
	state->reconnectAttempts = 0;
	state->lastReconnectTime = time(NULL);

	writeMemo("[i] Serial port reconnected successfully.");
	return 0;
}

/*	Utility Functions						*/

unsigned long	getUsecTimestamp(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return (unsigned long)(tv.tv_sec * 1000000 + tv.tv_usec);
}

int	getBaudRateConstant(int baudRate)
{
	switch (baudRate)
	{
	case 300:	return B300;
	case 1200:	return B1200;
	case 2400:	return B2400;
	case 4800:	return B4800;
	case 9600:	return B9600;
	case 19200:	return B19200;
	case 38400:	return B38400;
	case 57600:	return B57600;
	case 115200:	return B115200;
	default:	return -1;
	}
}

void	initRateControl(RateControlState *rc, int maxBytesPerSec)
{
	if (rc == NULL)
	{
		return;
	}

	rc->maxBytesPerSec = maxBytesPerSec;
	gettimeofday(&(rc->lastSendTime), NULL);
	rc->bytesSentInWindow = 0;
}

void	applyRateControl(RateControlState *rc, int bytesSent)
{
	struct timeval	now;
	struct timeval	elapsed;
	unsigned long	elapsedUsec;
	unsigned long	requiredUsec;
	unsigned long	sleepUsec;

	if (rc == NULL || rc->maxBytesPerSec <= 0)
	{
		return;
	}

	/*	Update bytes sent					*/
	rc->bytesSentInWindow += bytesSent;

	/*	Calculate elapsed time					*/
	gettimeofday(&now, NULL);
	timersub(&now, &(rc->lastSendTime), &elapsed);
	elapsedUsec = elapsed.tv_sec * 1000000 + elapsed.tv_usec;

	/*	Calculate required time for bytes sent			*/
	requiredUsec = (rc->bytesSentInWindow * 1000000)
			/ rc->maxBytesPerSec;

	/*	If we're sending too fast, sleep			*/
	if (requiredUsec > elapsedUsec)
	{
		sleepUsec = requiredUsec - elapsedUsec;
		usleep(sleepUsec);
	}

	/*	Reset window if enough time has passed (1 second)	*/
	if (elapsedUsec >= 1000000)
	{
		rc->bytesSentInWindow = 0;
		gettimeofday(&(rc->lastSendTime), NULL);
	}
}
