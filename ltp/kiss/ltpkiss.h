/*
 *	ltpkiss.h:	definitions supporting LTP KISS convergence layer.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#ifndef _LTPKISS_H_
#define _LTPKISS_H_

#include "ltp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*	KISS Protocol Constants						*/

#define KISS_FEND		0xC0	/* Frame delimiter		*/
#define KISS_FESC		0xDB	/* Frame escape			*/
#define KISS_TFEND		0xDC	/* Transposed frame end		*/
#define KISS_TFESC		0xDD	/* Transposed frame escape	*/

#define KISS_CMD_DATA		0x00	/* Data frame command		*/

/*	Configuration Constants						*/

#define MAX_KISS_FRAME_SIZE	2048	/* Maximum frame size		*/
#define DEFAULT_MTU		512	/* Default MTU (bytes)		*/
#define DEFAULT_BAUD_RATE	9600	/* Default baud rate		*/
#define DEFAULT_MAX_RATE	960	/* Default max rate (bytes/sec)	*/

/*	KISS Configuration Structure					*/

typedef struct
{
	char	devicePath[256];	/* Serial device path		*/
	int	baudRate;		/* Baud rate (e.g., B9600)	*/
	int	mtu;			/* Maximum transmission unit	*/
	int	maxRate;		/* Max bytes per second		*/
	int	useFlowControl;		/* Hardware flow control flag	*/
	int	reconnectDelay;		/* Seconds between reconnects	*/
	int	frameTimeout;		/* Milliseconds for incomplete	*/
	char	srcCallsign[7];		/* Source callsign (6 chars max)	*/
	int	srcSSID;		/* Source SSID (0-15)		*/
	char	dstCallsign[7];		/* Destination callsign		*/
	int	dstSSID;		/* Destination SSID (0-15)	*/
	int	useAX25;		/* 1 = wrap in AX.25 UI frames	*/
	int	burstSize;		/* Max segments per TX burst	*/
	int	listenWindowMs;		/* RX listen pause in ms	*/
} KissConfig;

/*	AX.25 Constants							*/

#define AX25_CALLSIGN_LEN	6	/* Callsign field length	*/
#define AX25_ADDR_LEN		7	/* Callsign + SSID byte		*/
#define AX25_HEADER_LEN		16	/* 2*addr + control + PID	*/
#define AX25_CONTROL_UI		0x03	/* UI frame control field	*/
#define AX25_PID_NOLAYER3	0xF0	/* No layer 3 protocol		*/

/*	KISS Framing Functions						*/

extern int	kissFrame(unsigned char *input, int inputLen,
			unsigned char *output, int *outputLen);
			/*	Frames data with KISS protocol.
			 *	Returns 0 on success, -1 on error.	*/

extern int	kissUnframe(unsigned char *input, int inputLen,
			unsigned char *output, int *outputLen,
			int *frameComplete);
			/*	Unframes KISS data.
			 *	Returns 0 on success, -1 on error.
			 *	Sets frameComplete to 1 when a
			 *	complete frame is extracted.		*/

/*	Serial Port Functions						*/

extern int	openSerialPort(const char *device, int baudRate,
			int useFlowControl);
			/*	Opens and configures serial port.
			 *	Returns file descriptor on success,
			 *	-1 on error.				*/

extern int	serialSend(int fd, unsigned char *data, int length);
			/*	Sends data to serial port.
			 *	Returns bytes sent on success,
			 *	-1 on error.				*/

extern int	serialReceive(int fd, unsigned char *buffer, int maxLen);
			/*	Receives data from serial port.
			 *	Returns bytes received on success,
			 *	0 if no data, -1 on error.		*/

extern void	closeSerialPort(int fd);
			/*	Closes serial port.			*/

/*	Configuration Functions						*/

extern int	loadKissConfig(uvast engineId, KissConfig *config);
			/*	Loads KISS configuration.
			 *	Returns 0 on success, -1 on error.	*/

/*	AX.25 Framing Functions						*/

extern int	ax25BuildUIFrame(const KissConfig *config,
			const unsigned char *payload, int payloadLen,
			unsigned char *output, int *outputLen);
			/*	Wraps payload in an AX.25 UI frame
			 *	with source/destination callsigns from
			 *	config. Returns 0 on success, -1 on
			 *	error.					*/

extern int	ax25StripHeader(const unsigned char *frame, int frameLen,
			const unsigned char **payload, int *payloadLen);
			/*	Strips AX.25 header from a received
			 *	frame, returning pointer to the payload
			 *	(information field) and its length.
			 *	Returns 0 on success, -1 on error.	*/

#ifdef __cplusplus
}
#endif

#endif	/* _LTPKISS_H_ */
