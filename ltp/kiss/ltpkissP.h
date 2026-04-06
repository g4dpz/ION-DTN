/*
 *	ltpkissP.h:	private definitions supporting LTP KISS CLA.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#ifndef _LTPKISSP_H_
#define _LTPKISSP_H_

#include "ltpkiss.h"
#include "ltpP.h"

#ifdef __cplusplus
extern "C" {
#endif

/*	Internal Buffer Sizes						*/

#define KISS_FRAME_BUFFER_SIZE	(MAX_KISS_FRAME_SIZE * 2 + 4)
#define SERIAL_READ_BUFFER_SIZE	4096

/*	KISS Unframer State Structure					*/

typedef struct
{
	unsigned char	buffer[KISS_FRAME_BUFFER_SIZE];
	int		bufferLen;
	int		inFrame;
	int		escaped;
	time_t		frameStartTime;
} KissUnframer;

/*	Rate Control State Structure					*/

typedef struct
{
	int		maxBytesPerSec;
	struct timeval	lastSendTime;
	unsigned long	bytesSentInWindow;
} RateControlState;

/*	Serial Port State Structure					*/

typedef struct
{
	int		fd;
	char		devicePath[256];
	int		baudRate;
	int		useFlowControl;
	int		reconnectAttempts;
	time_t		lastReconnectTime;
} SerialPortState;

/*	Internal KISS Functions						*/

extern void	initKissUnframer(KissUnframer *unframer);
			/*	Initializes KISS unframer state.	*/

extern void	resetKissUnframer(KissUnframer *unframer);
			/*	Resets unframer after error.		*/

extern int	kissUnframeStateful(KissUnframer *unframer,
			unsigned char *input, int inputLen,
			unsigned char *output, int *outputLen,
			int *frameComplete);
			/*	Unframes KISS data with persistent state.
			 *	Returns 0 on success, -1 on error.
			 *	Sets frameComplete to 1 when a
			 *	complete frame is extracted.
			 *	Maintains state across calls for
			 *	handling partial frames.		*/

/*	Internal Rate Control Functions					*/

extern void	initRateControl(RateControlState *rc, int maxBytesPerSec);
			/*	Initializes rate control state.		*/

extern void	applyRateControl(RateControlState *rc, int bytesSent);
			/*	Applies rate limiting delay.		*/

/*	Internal Serial Port Functions					*/

extern int	checkSerialPort(int fd);
			/*	Checks serial port status.
			 *	Returns 0 if OK, -1 on error.		*/

extern int	reconnectSerialPort(SerialPortState *state);
			/*	Attempts to reconnect serial port.
			 *	Returns 0 on success, -1 on error.	*/

/*	Utility Functions						*/

extern unsigned long	getUsecTimestamp(void);
			/*	Returns current time in microseconds.	*/

extern int	getBaudRateConstant(int baudRate);
			/*	Converts baud rate to termios constant.
			 *	Returns constant on success, -1 on
			 *	error.					*/

#ifdef __cplusplus
}
#endif

#endif	/* _LTPKISSP_H_ */
