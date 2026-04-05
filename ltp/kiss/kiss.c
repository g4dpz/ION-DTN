/*
	kiss.c:		KISS protocol framing and unframing.

			Wraps data in KISS frames for transmission
			to a TNC, and extracts data from received
			KISS frames.

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.
									*/

#include "ltpkisslsa.h"

/*	kissFrame: wrap data in a KISS frame.
 *
 *	Format: FEND + CMD_DATA + escaped_data + FEND
 *
 *	Any FEND (0xC0) in the data is replaced with FESC TFEND.
 *	Any FESC (0xDB) in the data is replaced with FESC TFESC.
 *
 *	Returns the length of the framed output, or -1 on error.	*/

int	kissFrame(unsigned char *in, int inLen,
		unsigned char *out, int outMax)
{
	int	outIdx = 0;
	int	i;

	if (in == NULL || out == NULL || inLen < 0)
	{
		return -1;
	}

	/*	Worst case: every byte escapes (2x) + FEND+CMD+FEND.	*/

	if (outMax < (inLen * 2) + 3)
	{
		return -1;
	}

	/*	Opening FEND.						*/

	out[outIdx++] = KISS_FEND;

	/*	Command byte: data frame on port 0.			*/

	out[outIdx++] = KISS_CMD_DATA;

	/*	Escape and copy data.					*/

	for (i = 0; i < inLen; i++)
	{
		if (in[i] == KISS_FEND)
		{
			out[outIdx++] = KISS_FESC;
			out[outIdx++] = KISS_TFEND;
		}
		else if (in[i] == KISS_FESC)
		{
			out[outIdx++] = KISS_FESC;
			out[outIdx++] = KISS_TFESC;
		}
		else
		{
			out[outIdx++] = in[i];
		}
	}

	/*	Closing FEND.						*/

	out[outIdx++] = KISS_FEND;

	return outIdx;
}

/*	kissUnframe: extract data from a KISS frame.
 *
 *	Strips FEND delimiters and command byte, unescapes
 *	FESC sequences.
 *
 *	Returns the length of the extracted data, or -1 on error.	*/

int	kissUnframe(unsigned char *in, int inLen,
		unsigned char *out, int outMax)
{
	int	outIdx = 0;
	int	i;
	int	dataStart;
	int	dataEnd;
	int	inEscape;

	if (in == NULL || out == NULL || inLen < 3)
	{
		return -1;
	}

	/*	Find the first FEND.					*/

	dataStart = -1;
	for (i = 0; i < inLen; i++)
	{
		if (in[i] == KISS_FEND)
		{
			dataStart = i + 1;
			break;
		}
	}

	if (dataStart < 0 || dataStart >= inLen)
	{
		return -1;
	}

	/*	Skip the command byte.					*/

	dataStart++;
	if (dataStart >= inLen)
	{
		return -1;
	}

	/*	Find the closing FEND.					*/

	dataEnd = -1;
	for (i = dataStart; i < inLen; i++)
	{
		if (in[i] == KISS_FEND)
		{
			dataEnd = i;
			break;
		}
	}

	if (dataEnd < 0)
	{
		return -1;
	}

	/*	Unescape the data.					*/

	inEscape = 0;
	for (i = dataStart; i < dataEnd; i++)
	{
		if (outIdx >= outMax)
		{
			return -1;
		}

		if (inEscape)
		{
			if (in[i] == KISS_TFEND)
			{
				out[outIdx++] = KISS_FEND;
			}
			else if (in[i] == KISS_TFESC)
			{
				out[outIdx++] = KISS_FESC;
			}
			else
			{
				/*	Invalid escape; pass through.	*/

				out[outIdx++] = in[i];
			}

			inEscape = 0;
		}
		else if (in[i] == KISS_FESC)
		{
			inEscape = 1;
		}
		else
		{
			out[outIdx++] = in[i];
		}
	}

	return outIdx;
}
