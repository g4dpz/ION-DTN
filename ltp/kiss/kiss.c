/*
 *	kiss.c:		KISS protocol implementation for LTP CLA.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#include "ltpkissP.h"
#include <string.h>
#include <time.h>

/*	KISS Framing Functions						*/

int	kissFrame(unsigned char *input, int inputLen,
		unsigned char *output, int *outputLen)
{
	int	i;
	int	j = 0;

	/*	Validate parameters					*/
	if (input == NULL || output == NULL || outputLen == NULL)
	{
		putErrmsg("Invalid parameters to kissFrame.", NULL);
		return -1;
	}

	if (inputLen < 0 || inputLen > MAX_KISS_FRAME_SIZE)
	{
		putErrmsg("Invalid input length for kissFrame.", itoa(inputLen));
		return -1;
	}

	/*	Check output buffer size (worst case: all bytes escaped)*/
	if (j + 4 + (inputLen * 2) > KISS_FRAME_BUFFER_SIZE)
	{
		putErrmsg("Output buffer too small for kissFrame.", NULL);
		return -1;
	}

	/*	Start frame with FEND					*/
	output[j++] = KISS_FEND;

	/*	Add command byte (data frame, port 0)			*/
	output[j++] = KISS_CMD_DATA;

	/*	Escape and copy data					*/
	for (i = 0; i < inputLen; i++)
	{
		if (input[i] == KISS_FEND)
		{
			output[j++] = KISS_FESC;
			output[j++] = KISS_TFEND;
		}
		else if (input[i] == KISS_FESC)
		{
			output[j++] = KISS_FESC;
			output[j++] = KISS_TFESC;
		}
		else
		{
			output[j++] = input[i];
		}
	}

	/*	End frame with FEND					*/
	output[j++] = KISS_FEND;

	*outputLen = j;
	return 0;
}

int	kissUnframe(unsigned char *input, int inputLen,
		unsigned char *output, int *outputLen,
		int *frameComplete)
{
	KissUnframer	unframer;

	/*	Simple wrapper for stateless operation.			*/

	initKissUnframer(&unframer);
	return kissUnframeStateful(&unframer, input, inputLen,
			output, outputLen, frameComplete);
}

int	kissUnframeStateful(KissUnframer *unframer, unsigned char *input,
		int inputLen, unsigned char *output, int *outputLen,
		int *frameComplete)
{
	int		i;

	/*	Validate parameters					*/

	if (unframer == NULL || input == NULL || output == NULL
		|| outputLen == NULL || frameComplete == NULL)
	{
		putErrmsg("Invalid parameters to kissUnframeStateful.", NULL);
		return -1;
	}

	if (inputLen < 0)
	{
		putErrmsg("Invalid input length for kissUnframeStateful.",
				itoa(inputLen));
		return -1;
	}

	/*	Process input bytes					*/

	for (i = 0; i < inputLen; i++)
	{
		if (input[i] == KISS_FEND)
		{
			/*	Frame delimiter detected		*/

			if (unframer->inFrame && unframer->bufferLen > 1)
			{
				/*	Complete frame received.
				 *	Skip command byte (first byte).	*/

				if (unframer->bufferLen - 1 > MAX_KISS_FRAME_SIZE)
				{
					putErrmsg("Frame too large in kissUnframe.",
						itoa(unframer->bufferLen - 1));
					resetKissUnframer(unframer);
					continue;
				}

				memcpy(output, unframer->buffer + 1,
					unframer->bufferLen - 1);
				*outputLen = unframer->bufferLen - 1;
				*frameComplete = 1;
				
				/*	Reset for next frame.		*/

				unframer->inFrame = 0;
				unframer->bufferLen = 0;
				unframer->escaped = 0;
				return 0;
			}

			/*	Start new frame				*/

			unframer->inFrame = 1;
			unframer->bufferLen = 0;
			unframer->escaped = 0;
			unframer->frameStartTime = time(NULL);
		}
		else if (unframer->inFrame)
		{
			/*	Processing frame data			*/

			if (unframer->escaped)
			{
				/*	Handle escaped character	*/

				if (input[i] == KISS_TFEND)
				{
					unframer->buffer[unframer->bufferLen++]
						= KISS_FEND;
				}
				else if (input[i] == KISS_TFESC)
				{
					unframer->buffer[unframer->bufferLen++]
						= KISS_FESC;
				}
				else
				{
					/*	Invalid escape sequence	*/

					writeMemo("[?] Invalid KISS escape \
sequence.");
					resetKissUnframer(unframer);
					continue;
				}

				unframer->escaped = 0;
			}
			else if (input[i] == KISS_FESC)
			{
				/*	Escape character detected	*/

				unframer->escaped = 1;
			}
			else
			{
				/*	Normal data byte		*/

				if (unframer->bufferLen >= KISS_FRAME_BUFFER_SIZE)
				{
					putErrmsg("Frame buffer overflow in \
kissUnframe.", NULL);
					resetKissUnframer(unframer);
					continue;
				}

				unframer->buffer[unframer->bufferLen++] = input[i];
			}
		}
	}

	/*	No complete frame yet					*/
	*frameComplete = 0;
	return 0;
}

void	initKissUnframer(KissUnframer *unframer)
{
	if (unframer == NULL)
	{
		return;
	}

	memset(unframer->buffer, 0, KISS_FRAME_BUFFER_SIZE);
	unframer->bufferLen = 0;
	unframer->inFrame = 0;
	unframer->escaped = 0;
	unframer->frameStartTime = 0;
}

void	resetKissUnframer(KissUnframer *unframer)
{
	if (unframer == NULL)
	{
		return;
	}

	unframer->bufferLen = 0;
	unframer->inFrame = 0;
	unframer->escaped = 0;
	unframer->frameStartTime = 0;
}
