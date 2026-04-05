/*
	ltpkisslsi.c:	LTP KISS-based link service input daemon.
			Receives KISS frames from a serial-connected
			TNC and delivers LTP segments to the ION
			LTP engine.

			RX flow:
			serial port -> KISS frame -> strip AX.25
			header (optional) -> LTP segment -> ION

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.
									*/

#include "ltpkisslsa.h"

static void	interruptThread(int signum)
{
	(void)signum;
	isignal(SIGTERM, interruptThread);
	ionKillMainThread("ltpkisslsi");
}

/*	*	*	Receiver thread		*	*	*	*/

typedef struct
{
	int		serialFd;
	int		running;
	KissConfig	config;
} KissReceiverParms;

/*	KISS frame accumulator state machine.
 *
 *	Reads bytes from the serial port and assembles complete
 *	KISS frames.  A frame starts with FEND and ends with FEND.
 *	Back-to-back FENDs are treated as a single delimiter.		*/

static void	*receiveFrames(void *parm)
{
	KissReceiverParms	*rtp = (KissReceiverParms *) parm;
	unsigned char		rawBuf[KISS_BUFSZ];
	unsigned char		frameBuf[KISS_BUFSZ];
	int			frameIdx;
	int			inFrame;
	unsigned char		unframedBuf[KISS_BUFSZ];
	int			unframedLen;
	unsigned char		*ltpPayload;
	int			ltpLen;
	int			bytesRead;
	int			i;

	writeMemo("[i] ltpkisslsi receiver thread started.");

	frameIdx = 0;
	inFrame = 0;

	while (rtp->running)
	{
		bytesRead = serialReceive(rtp->serialFd, rawBuf,
				sizeof(rawBuf));
		if (bytesRead < 0)
		{
			putErrmsg("ltpkisslsi: serial read error.",
					NULL);
			rtp->running = 0;
			break;
		}

		if (bytesRead == 0)
		{
			continue;	/*	Timeout, no data.	*/
		}

		/*	Process each received byte.			*/

		for (i = 0; i < bytesRead; i++)
		{
			if (rawBuf[i] == KISS_FEND)
			{
				if (inFrame && frameIdx > 0)
				{
					/*	End of frame.  Add
					 *	closing FEND and
					 *	process.		*/

					if (frameIdx < (int) sizeof(frameBuf))
					{
						frameBuf[frameIdx++]
							= KISS_FEND;
					}

					/*	Unframe KISS.		*/

					unframedLen = kissUnframe(
						frameBuf, frameIdx,
						unframedBuf,
						sizeof(unframedBuf));

					if (unframedLen > 0)
					{
						/*	Strip AX.25
						 *	if enabled.	*/

						if (rtp->config.useAX25)
						{
							ltpLen =
							  ax25StripHeader(
							    unframedBuf,
							    unframedLen,
							    &ltpPayload);
						}
						else
						{
							ltpPayload
							  = unframedBuf;
							ltpLen
							  = unframedLen;
						}

						if (ltpLen > 0)
						{
							ltpHandleInboundSegment(
							  (char *)
							  ltpPayload,
							  ltpLen);
						}
					}

					frameIdx = 0;
				}

				/*	Start of new frame.  Store
				 *	the opening FEND.		*/

				inFrame = 1;
				frameIdx = 0;
				if (frameIdx < (int) sizeof(frameBuf))
				{
					frameBuf[frameIdx++] = KISS_FEND;
				}
			}
			else if (inFrame)
			{
				if (frameIdx < (int) sizeof(frameBuf))
				{
					frameBuf[frameIdx++] = rawBuf[i];
				}
				else
				{
					/*	Frame too large;
					 *	discard.		*/

					inFrame = 0;
					frameIdx = 0;
				}
			}

			/*	Else: byte outside frame, discard.	*/
		}
	}

	writeMemo("[i] ltpkisslsi receiver thread ended.");
	return NULL;
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	ltpkisslsi(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	uvast		remoteEngineId = a1 != 0 ? (uvast) a1 : 0;
#else
int	main(int argc, char *argv[])
{
	uvast		remoteEngineId = argc > 1 ? strtouvast(argv[1]) : 0;
#endif
	Sdr			sdr;
	char			lsiCmd[256];
	LtpVseat		*vseat;
	PsmAddress		vseatElt;
	KissReceiverParms	rtp;
	pthread_t		receiverThread;

	if (remoteEngineId == 0)
	{
		PUTS("Usage: ltpkisslsi <remote engine ID>");
		return 0;
	}

	/*	Initialize LTP.						*/

	if (ltpInit(0) < 0)
	{
		putErrmsg("ltpkisslsi can't initialize LTP.", NULL);
		return 1;
	}

	sdr = getIonsdr();
	isprintf(lsiCmd, sizeof lsiCmd, "ltpkisslsi " UVAST_FIELDSPEC,
			remoteEngineId);
	CHKERR(sdr_begin_xn(sdr));
	findSeat(lsiCmd, &vseat, &vseatElt);
	sdr_exit_xn(sdr);
	if (vseatElt == 0)
	{
		putErrmsg("Undefined LSI", lsiCmd);
		return 1;
	}

	if (vseat->lsiPid != ERROR && vseat->lsiPid != sm_TaskIdSelf())
	{
		putErrmsg("LSI task is already started.",
				itoa(vseat->lsiPid));
		return 1;
	}

	/*	Load KISS configuration.				*/

	memset(&rtp, 0, sizeof(rtp));
	if (loadKissConfig(remoteEngineId, &rtp.config) < 0)
	{
		putErrmsg("ltpkisslsi can't load KISS config.",
				itoa(remoteEngineId));
		return 1;
	}

	/*	Open serial port.					*/

	rtp.serialFd = openSerialPort(&rtp.config);
	if (rtp.serialFd < 0)
	{
		putErrmsg("ltpkisslsi can't open serial port.",
				rtp.config.devicePath);
		return 1;
	}

	/*	Set up signal handling.					*/

	ionNoteMainThread("ltpkisslsi");
	isignal(SIGTERM, interruptThread);

	/*	Start the receiver thread.				*/

	rtp.running = 1;

	if (pthread_begin(&receiverThread, NULL, receiveFrames,
			&rtp, "ltpkisslsi_rx"))
	{
		closeSerialPort(rtp.serialFd);
		putSysErrmsg("ltpkisslsi can't create receiver thread.",
				NULL);
		return 1;
	}

	/*	Log startup.						*/

	{
		char	txt[512];

		isprintf(txt, sizeof(txt),
			"[i] ltpkisslsi is running, device=%s, baud=%d, "
			"ax25=%d, rengine=" UVAST_FIELDSPEC ".",
			rtp.config.devicePath, rtp.config.baudRate,
			rtp.config.useAX25, remoteEngineId);
		writeMemo(txt);
	}

	/*	Wait for shutdown signal.				*/

	ionPauseMainThread(-1);

	/*	Time to shut down.					*/

	writeMemo("[i] ltpkisslsi shutting down...");
	rtp.running = 0;

	pthread_join(receiverThread, NULL);

	closeSerialPort(rtp.serialFd);
	writeErrmsgMemos();
	writeMemo("[i] ltpkisslsi has ended.");
	ionDetach();
	return 0;
}
