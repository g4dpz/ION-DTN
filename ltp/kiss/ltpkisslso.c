/*
	ltpkisslso.c:	LTP KISS-based link service output daemon.
			Dedicated to serial/KISS transmission to
			a single remote LTP engine via a TNC.

			TX flow:
			LTP segment -> AX.25 UI frame (optional)
			-> KISS frame -> serial port -> TNC

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.
									*/

#include "ltpkisslsa.h"

static sm_SemId		kissLsoSemaphore(sm_SemId *semid)
{
	static sm_SemId	semaphore = -1;

	if (semid)
	{
		semaphore = *semid;
	}

	return semaphore;
}

static void	shutDownLso(int signum)
{
	(void)signum;
	sm_SemEnd(kissLsoSemaphore(NULL));
}

/*	*	*	Rate control	*	*	*	*	*/

static unsigned long	getUsecTimestamp(void)
{
	struct timeval	tv;

	getCurrentTime(&tv);
	return ((tv.tv_sec * 1000000) + tv.tv_usec);
}

typedef struct
{
	unsigned long	lastRefillTime;
	long		tokens;
	long		bucketSize;
	int		maxRate;	/*	Bytes per second.	*/
} KissTokenBucket;

static void	applyRateControl(KissTokenBucket *tb, int bytesSent)
{
	unsigned long	now;
	unsigned long	elapsed;
	long		added;

	if (tb->maxRate <= 0)
	{
		return;		/*	No rate control.		*/
	}

	now = getUsecTimestamp();
	elapsed = now - tb->lastRefillTime;
	tb->lastRefillTime = now;

	/*	Refill tokens based on elapsed time.			*/

	added = (elapsed * tb->maxRate) / 1000000;
	tb->tokens += added;
	if (tb->tokens > tb->bucketSize)
	{
		tb->tokens = tb->bucketSize;
	}

	/*	Deduct bytes sent.					*/

	tb->tokens -= bytesSent;

	/*	If bucket is empty, pace transmission.			*/

	if (tb->tokens < 0)
	{
		unsigned long	deficit;
		unsigned long	waitUsec;

		deficit = (unsigned long)(-(tb->tokens));
		waitUsec = (deficit * 1000000) / tb->maxRate;
		if (waitUsec > 250)
		{
			microsnooze(waitUsec);
		}
	}
}

/*	*	*	Main thread functions	*	*	*	*/

#if defined (ION_LWT)
int	ltpkisslso(saddr a1, saddr a2, saddr a3, saddr a4, saddr a5,
		saddr a6, saddr a7, saddr a8, saddr a9, saddr a10)
{
	uvast		remoteEngineId = a1 != 0 ? (uvast) a1 : 0;
#else
int	main(int argc, char *argv[])
{
	uvast		remoteEngineId = argc > 1 ? strtouvast(argv[1]) : 0;
#endif
	Sdr		sdr;
	LtpVspan	*vspan;
	PsmAddress	vspanElt;
	KissConfig	config;
	int		serialFd;
	int		segmentLength;
	char		*segment;
	unsigned char	ax25Buf[KISS_BUFSZ];
	unsigned char	kissBuf[KISS_BUFSZ];
	int		frameLen;
	int		kissLen;
	int		running;
	KissTokenBucket	tb;

	if (remoteEngineId == 0)
	{
		PUTS("Usage: ltpkisslso <remote engine ID>");
		return 0;
	}

	/*	Initialize LTP.						*/

	if (ltpInit(0) < 0)
	{
		putErrmsg("ltpkisslso can't initialize LTP.", NULL);
		return 1;
	}

	sdr = getIonsdr();
	CHKZERO(sdr_begin_xn(sdr));
	findSpan(remoteEngineId, &vspan, &vspanElt);
	if (vspanElt == 0)
	{
		sdr_exit_xn(sdr);
		putErrmsg("No such engine in database.",
				itoa(remoteEngineId));
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
		putErrmsg("ltpkisslso can't load KISS config.",
				itoa(remoteEngineId));
		return 1;
	}

	/*	Open serial port.					*/

	serialFd = openSerialPort(&config);
	if (serialFd < 0)
	{
		putErrmsg("ltpkisslso can't open serial port.",
				config.devicePath);
		return 1;
	}

	/*	Set up signal handling.					*/

	oK(kissLsoSemaphore(&(vspan->segSemaphore)));
	isignal(SIGTERM, shutDownLso);

	/*	Log startup.						*/

	{
		char	txt[512];

		isprintf(txt, sizeof(txt),
			"[i] ltpkisslso is running, device=%s, baud=%d, "
			"mtu=%d, rate=%d, ax25=%d, rengine=" UVAST_FIELDSPEC ".",
			config.devicePath, config.baudRate,
			config.mtu, config.maxRate, config.useAX25,
			remoteEngineId);
		writeMemo(txt);
	}

	/*	Initialize rate control.				*/

	tb.lastRefillTime = getUsecTimestamp();
	tb.tokens = 0;
	tb.bucketSize = config.mtu * 4;
	tb.maxRate = config.maxRate;

	/*	Main transmission loop.					*/

	running = 1;
	while (running)
	{
		if (sm_SemEnded(kissLsoSemaphore(NULL)))
		{
			break;
		}

		segmentLength = ltpDequeueOutboundSegment(vspan, &segment);
		if (segmentLength < 0)
		{
			running = 0;
			continue;
		}

		if (segmentLength == 0)
		{
			continue;	/*	Interrupted.		*/
		}

		if (segmentLength > config.mtu)
		{
			putErrmsg("Segment too big for KISS LSO.",
					itoa(segmentLength));
			running = 0;
			continue;
		}

		/*	Build the frame: optionally wrap in AX.25,
		 *	then always wrap in KISS.			*/

		if (config.useAX25)
		{
			frameLen = ax25BuildUIFrame(&config,
					(unsigned char *) segment,
					segmentLength,
					ax25Buf, sizeof(ax25Buf));
			if (frameLen < 0)
			{
				putErrmsg("AX.25 framing failed.", NULL);
				running = 0;
				continue;
			}

			kissLen = kissFrame(ax25Buf, frameLen,
					kissBuf, sizeof(kissBuf));
		}
		else
		{
			kissLen = kissFrame(
					(unsigned char *) segment,
					segmentLength,
					kissBuf, sizeof(kissBuf));
		}

		if (kissLen < 0)
		{
			putErrmsg("KISS framing failed.", NULL);
			running = 0;
			continue;
		}

		/*	Transmit over serial port.			*/

		if (serialSend(serialFd, kissBuf, kissLen) < 0)
		{
			writeMemo("[!] ltpkisslso: serial write "
					"failed, attempting reconnect.");

			closeSerialPort(serialFd);
			snooze(2);
			serialFd = openSerialPort(&config);
			if (serialFd < 0)
			{
				putErrmsg("ltpkisslso can't reopen "
					"serial port.",
					config.devicePath);
				running = 0;
				continue;
			}

			writeMemo("[i] ltpkisslso: serial port "
					"reconnected.");

			/*	Retry the send.				*/

			if (serialSend(serialFd, kissBuf, kissLen) < 0)
			{
				putErrmsg("ltpkisslso: retry failed.",
						NULL);
				running = 0;
				continue;
			}
		}

		applyRateControl(&tb, kissLen);

		/*	Let other tasks run.				*/

		sm_TaskYield();
	}

	/*	Shut down.						*/

	closeSerialPort(serialFd);
	writeErrmsgMemos();
	writeMemo("[i] ltpkisslso has ended.");
	ionDetach();
	return 0;
}
