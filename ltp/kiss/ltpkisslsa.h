/*
	ltpkisslsa.h:	common definitions for KISS link service
			adapter modules.  Wraps LTP segments in
			AX.25 UI frames and KISS frames for
			transmission over serial-connected TNCs.

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.
									*/

#ifndef LTPKISSLSA_H
#define LTPKISSLSA_H

#include "ltpP.h"
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/*	KISS protocol constants						*/

#define KISS_FEND		0xC0
#define KISS_FESC		0xDB
#define KISS_TFEND		0xDC
#define KISS_TFESC		0xDD
#define KISS_CMD_DATA		0x00

/*	AX.25 constants							*/

#define AX25_CALLSIGN_LEN	6
#define AX25_ADDR_LEN		7	/*	callsign + SSID byte	*/
#define AX25_HEADER_LEN		16	/*	dst(7)+src(7)+ctrl+pid	*/
#define AX25_CONTROL_UI		0x03
#define AX25_PID_NOLAYER3	0xF0

/*	Buffer sizes							*/

#define KISS_MAX_MTU		1024
#define KISS_DEFAULT_MTU	512
#define KISS_BUFSZ		((KISS_MAX_MTU + AX25_HEADER_LEN) * 2 + 4)

/*	Configuration structure						*/

typedef struct
{
	char		devicePath[256];
	int		baudRate;
	int		mtu;
	int		maxRate;	/*	Bytes per second.	*/
	int		useFlowControl;
	char		srcCallsign[AX25_ADDR_LEN];
	int		srcSSID;
	char		dstCallsign[AX25_ADDR_LEN];
	int		dstSSID;
	int		useAX25;	/*	Boolean.		*/
} KissConfig;

/*	Configuration file parser					*/

extern int	loadKissConfig(uvast engineId, KissConfig *config);

/*	Serial port operations						*/

extern int	openSerialPort(KissConfig *config);
extern void	closeSerialPort(int fd);
extern int	serialSend(int fd, unsigned char *data, int length);
extern int	serialReceive(int fd, unsigned char *buf, int maxLen);

/*	KISS framing							*/

extern int	kissFrame(unsigned char *in, int inLen,
			unsigned char *out, int outMax);
extern int	kissUnframe(unsigned char *in, int inLen,
			unsigned char *out, int outMax);

/*	AX.25 UI frame construction and parsing				*/

extern int	ax25BuildUIFrame(KissConfig *config,
			unsigned char *payload, int payloadLen,
			unsigned char *out, int outMax);
extern int	ax25StripHeader(unsigned char *frame, int frameLen,
			unsigned char **payload);

#ifdef __cplusplus
}
#endif

#endif /* LTPKISSLSA_H */
