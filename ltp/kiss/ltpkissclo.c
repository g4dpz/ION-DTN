/*
 *	ltpkissclo.c:	LTP KISS convergence layer output daemon.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#include "ltpkissP.h"

static int	running = 1;

/*	*	*	Signal handling	*	*	*	*	*/

static void	shutDown(int signum)
{
	isignal(SIGTERM, shutDown);
	running = 0;
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	ltpkissclo(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	uvast		remoteEngineId = (a1 != 0 ? strtouvast((char *) a1) : 0);
#else
int	main(int argc, char *argv[])
{
	uvast		remoteEngineId = (argc > 1 ? strtouvast(argv[1]) : 0);
#endif
	Sdr		sdr;
	LtpVspan	*vspan;
	PsmAddress	vspanElt;
	KissConfig	config;
	int		serialFd = -1;
	char		*segment;
	int		segmentLength;
	unsigned char	ax25Buffer[MAX_KISS_FRAME_SIZE];
	int		ax25Len;
	unsigned char	kissFrameBuffer[KISS_FRAME_BUFFER_SIZE];
	int		kissFrameLen;
	int		bytesSent;
	RateControlState rc;

	if (remoteEngineId == 0)
	{
		PUTS("Usage: ltpkissclo <remote engine ID>");
		return 0;
	}

	/*	Initialize LTP.						*/

	if (ltpInit(0) < 0)
	{
		putErrmsg("ltpkissclo can't initialize LTP.", NULL);
		return 1;
	}

	sdr = getIonsdr();
	CHKZERO(sdr_begin_xn(sdr));	/*	Lock memory.		*/
	findSpan(remoteEngineId, &vspan, &vspanElt);
	if (vspanElt == 0)
	{
		sdr_exit_xn(sdr);
		putErrmsg("No such engine in database.", itoa(remoteEngineId));
		return 1;
	}

	if (vspan->lsoPid != ERROR && vspan->lsoPid != sm_TaskIdSelf())
	{
		sdr_exit_xn(sdr);
		putErrmsg("LSO task is already started for this span.",
				itoa(vspan->lsoPid));
		return 1;
	}

	sdr_exit_xn(sdr);

	/*	Load KISS configuration.				*/

	if (loadKissConfig(remoteEngineId, &config) < 0)
	{
		putErrmsg("ltpkissclo can't load configuration.", NULL);
		return 1;
	}

	/*	Open serial port.					*/

	serialFd = openSerialPort(config.devicePath, config.baudRate,
			config.useFlowControl);
	if (serialFd < 0)
	{
		putErrmsg("ltpkissclo can't open serial port.",
				config.devicePath);
		return 1;
	}

	/*	Initialize rate control.				*/

	initRateControl(&rc, config.maxRate);

	/*	Set up signal handling.					*/

	isignal(SIGTERM, shutDown);

	/*	Main transmission loop.					*/

	writeMemo("[i] ltpkissclo is running.");
	while (running && !(sm_SemEnded(vspan->segSemaphore)))
	{
		/*	Get next outbound LTP segment.			*/

		segmentLength = ltpDequeueOutboundSegment(vspan, &segment);
		if (segmentLength < 0)
		{
			putErrmsg("ltpkissclo segment dequeue failed.", NULL);
			running = 0;
			continue;
		}

		if (segmentLength == 0)		/*	Interrupted.	*/
		{
			continue;
		}

		/*	Check segment size.				*/

		if (segmentLength > config.mtu)
		{
			putErrmsg("Segment exceeds MTU.", itoa(segmentLength));
			continue;	/*	Discard oversized segment.*/
		}

		/*	Wrap in AX.25 UI frame if callsigns configured.	*/

		if (config.useAX25)
		{
			if (ax25BuildUIFrame(&config,
					(unsigned char *) segment,
					segmentLength,
					ax25Buffer, &ax25Len) < 0)
			{
				putErrmsg("ltpkissclo AX.25 framing failed.",
						NULL);
				continue;
			}

			/*	Frame AX.25 frame with KISS protocol.	*/

			if (kissFrame(ax25Buffer, ax25Len,
					kissFrameBuffer, &kissFrameLen) < 0)
			{
				putErrmsg("ltpkissclo KISS framing failed.",
						NULL);
				continue;
			}
		}
		else
		{
			/*	Frame raw segment with KISS protocol.	*/

			if (kissFrame((unsigned char *) segment,
					segmentLength,
					kissFrameBuffer, &kissFrameLen) < 0)
			{
				putErrmsg("ltpkissclo KISS framing failed.",
						NULL);
				continue;
			}
		}

		/*	Send framed segment to serial port.		*/

		bytesSent = serialSend(serialFd, kissFrameBuffer, kissFrameLen);
		if (bytesSent < 0)
		{
			putErrmsg("ltpkissclo serial send failed.", NULL);
			
			/*	Attempt reconnection.			*/

			closeSerialPort(serialFd);
			snooze(config.reconnectDelay);
			serialFd = openSerialPort(config.devicePath,
					config.baudRate, config.useFlowControl);
			if (serialFd < 0)
			{
				putErrmsg("ltpkissclo reconnect failed.",
						config.devicePath);
				running = 0;
			}

			continue;
		}

		/*	Apply rate control.				*/

		applyRateControl(&rc, bytesSent);

		/*	Let other tasks run.				*/

		sm_TaskYield();
	}

	/*	Clean shutdown.						*/

	closeSerialPort(serialFd);
	writeErrmsgMemos();
	writeMemo("[i] ltpkissclo has ended.");
	return 0;
}
