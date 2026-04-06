/*
 *	ltpkisscli.c:	LTP KISS convergence layer input daemon.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#include "ltpkissP.h"
#include <pthread.h>

/*	*	*	Receiver thread	*	*	*	*	*/

typedef struct
{
	int		serialFd;
	KissConfig	*config;
	int		*running;
} ReceiverThreadParms;

static void	*receiverThread(void *parm)
{
	ReceiverThreadParms	*rtp = (ReceiverThreadParms *) parm;
	unsigned char		serialBuffer[SERIAL_READ_BUFFER_SIZE];
	unsigned char		segmentBuffer[MAX_KISS_FRAME_SIZE];
	KissUnframer		unframer;
	int			bytesRead;
	int			segmentLength;
	int			frameComplete;
	int			result;

	/*	Initialize KISS unframer.				*/

	initKissUnframer(&unframer);

	/*	Main reception loop.					*/

	while (*(rtp->running))
	{
		/*	Read from serial port.				*/

		bytesRead = serialReceive(rtp->serialFd, serialBuffer,
				SERIAL_READ_BUFFER_SIZE);
		
		if (bytesRead < 0)
		{
			putErrmsg("ltpkisscli serial receive failed.", NULL);
			
			/*	Attempt reconnection.			*/

			closeSerialPort(rtp->serialFd);
			snooze(rtp->config->reconnectDelay);
			rtp->serialFd = openSerialPort(
					rtp->config->devicePath,
					rtp->config->baudRate,
					rtp->config->useFlowControl);
			if (rtp->serialFd < 0)
			{
				putErrmsg("ltpkisscli reconnect failed.",
						rtp->config->devicePath);
				*(rtp->running) = 0;
				break;
			}

			/*	Reset unframer after reconnection.	*/

			resetKissUnframer(&unframer);
			continue;
		}

		if (bytesRead == 0)
		{
			/*	No data available, continue polling.	*/

			sm_TaskYield();
			continue;
		}

		/*	Process received bytes through KISS unframer.	*/

		result = kissUnframeStateful(&unframer, serialBuffer,
				bytesRead, segmentBuffer, &segmentLength,
				&frameComplete);
		
		if (result < 0)
		{
			putErrmsg("ltpkisscli KISS unframe failed.", NULL);
			resetKissUnframer(&unframer);
			continue;
		}

		if (!frameComplete)
		{
			/*	Partial frame, continue accumulating.	*/

			continue;
		}

		/*	Complete frame received, deliver to LTP engine.*/

		if (segmentLength > 0)
		{
			/*	Strip AX.25 header if configured.	*/

			if (rtp->config->useAX25)
			{
				const unsigned char	*ltpPayload;
				int			ltpPayloadLen;

				if (ax25StripHeader(segmentBuffer,
						segmentLength,
						&ltpPayload,
						&ltpPayloadLen) < 0)
				{
					putErrmsg("ltpkisscli AX.25 strip \
failed.", NULL);
					continue;
				}

				if (ltpPayloadLen > 0)
				{
					if (ltpHandleInboundSegment(
						(char *) ltpPayload,
						ltpPayloadLen) < 0)
					{
						putErrmsg("ltpkisscli can't \
handle segment.", NULL);
					}
				}
			}
			else
			{
				if (ltpHandleInboundSegment(
					(char *) segmentBuffer,
					segmentLength) < 0)
				{
					putErrmsg("ltpkisscli can't handle \
segment.", NULL);
				}
			}
		}

		/*	Let other tasks run.				*/

		sm_TaskYield();
	}

	writeErrmsgMemos();
	writeMemo("[i] ltpkisscli receiver thread has ended.");
	return NULL;
}

/*	*	*	Main thread functions	*	*	*	*/

static int	running = 1;

static void	shutDown(int signum)
{
	isignal(SIGTERM, shutDown);
	running = 0;
}

#if defined (ION_LWT)
int	ltpkisscli(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	uvast		ownEngineId = (a1 != 0 ? strtouvast((char *) a1) : 0);
#else
int	main(int argc, char *argv[])
{
	uvast		ownEngineId = (argc > 1 ? strtouvast(argv[1]) : 0);
#endif
	KissConfig		config;
	int			serialFd = -1;
	ReceiverThreadParms	rtp;
	pthread_t		receiver;

	if (ownEngineId == 0)
	{
		PUTS("Usage: ltpkisscli <own engine ID>");
		return 0;
	}

	/*	Initialize LTP.						*/

	if (ltpInit(0) < 0)
	{
		putErrmsg("ltpkisscli can't initialize LTP.", NULL);
		return 1;
	}

	/*	Load KISS configuration.				*/

	if (loadKissConfig(ownEngineId, &config) < 0)
	{
		putErrmsg("ltpkisscli can't load configuration.", NULL);
		return 1;
	}

	/*	Open serial port.					*/

	serialFd = openSerialPort(config.devicePath, config.baudRate,
			config.useFlowControl);
	if (serialFd < 0)
	{
		putErrmsg("ltpkisscli can't open serial port.",
				config.devicePath);
		return 1;
	}

	/*	Set up signal handling.					*/

	isignal(SIGTERM, shutDown);

	/*	Start receiver thread.					*/

	rtp.serialFd = serialFd;
	rtp.config = &config;
	rtp.running = &running;

	if (pthread_begin(&receiver, NULL, receiverThread, &rtp,
			"ltpkisscli_receiver"))
	{
		closeSerialPort(serialFd);
		putSysErrmsg("ltpkisscli can't create receiver thread", NULL);
		return 1;
	}

	writeMemo("[i] ltpkisscli is running.");

	/*	Main thread waits for shutdown signal.			*/

	ionPauseMainThread(-1);

	/*	Shutdown initiated.					*/

	running = 0;
	pthread_join(receiver, NULL);

	/*	Clean shutdown.						*/

	closeSerialPort(serialFd);
	writeErrmsgMemos();
	writeMemo("[i] ltpkisscli has ended.");
	ionDetach();
	return 0;
}
