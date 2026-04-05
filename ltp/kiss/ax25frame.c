/*
	ax25frame.c:	AX.25 UI frame construction and parsing.

			Builds AX.25 Unnumbered Information (UI)
			frames with amateur radio callsigns for
			wrapping LTP segments.  Also strips AX.25
			headers from received frames.

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.

	AX.25 UI frame format (16 bytes header):
	  Destination address:  7 bytes (6 callsign + 1 SSID)
	  Source address:       7 bytes (6 callsign + 1 SSID)
	  Control:              1 byte  (0x03 = UI)
	  PID:                  1 byte  (0xF0 = no layer 3)
	  Payload:              variable

	Callsign encoding:
	  - 6 characters, space-padded on the right
	  - Each byte is the ASCII value left-shifted by 1 bit
	  - SSID byte: 0b0SSSSS0 with extension bit set on
	    last address field (source)
									*/

#include "ltpkisslsa.h"

/*	encodeCallsign: encode a callsign into AX.25 address format.
 *
 *	Writes 7 bytes to 'out': 6 shifted callsign bytes + SSID byte.
 *	The 'last' flag sets the extension bit on the SSID byte to
 *	indicate this is the last address field.			*/

static void	encodeCallsign(const char *callsign, int ssid,
			int last, unsigned char *out)
{
	int	i;
	int	len;

	len = strlen(callsign);
	if (len > AX25_CALLSIGN_LEN)
	{
		len = AX25_CALLSIGN_LEN;
	}

	/*	Encode callsign characters, shifted left by 1.		*/

	for (i = 0; i < AX25_CALLSIGN_LEN; i++)
	{
		if (i < len)
		{
			out[i] = (unsigned char)(callsign[i]) << 1;
		}
		else
		{
			out[i] = ' ' << 1;	/*	Space-pad.	*/
		}
	}

	/*	SSID byte: 0b011SSSS0
	 *	Bits 7-6: reserved (0b01 per AX.25 2.2)
	 *	Bit 5: command/response (set to 1)
	 *	Bits 4-1: SSID (0-15)
	 *	Bit 0: extension bit (1 if last address field)		*/

	out[AX25_CALLSIGN_LEN] = (unsigned char)(0x60
			| ((ssid & 0x0F) << 1)
			| (last ? 0x01 : 0x00));
}

/*	ax25BuildUIFrame: build an AX.25 UI frame.
 *
 *	Constructs a 16-byte AX.25 header (destination address,
 *	source address, control byte, PID byte) followed by the
 *	payload (LTP segment).
 *
 *	Returns total frame length, or -1 on error.			*/

int	ax25BuildUIFrame(KissConfig *config,
		unsigned char *payload, int payloadLen,
		unsigned char *out, int outMax)
{
	int	totalLen;

	if (config == NULL || payload == NULL || out == NULL)
	{
		return -1;
	}

	totalLen = AX25_HEADER_LEN + payloadLen;
	if (totalLen > outMax)
	{
		return -1;
	}

	/*	Destination address (not last).				*/

	encodeCallsign(config->dstCallsign, config->dstSSID, 0, out);

	/*	Source address (last address field).			*/

	encodeCallsign(config->srcCallsign, config->srcSSID, 1,
			out + AX25_ADDR_LEN);

	/*	Control field: UI frame.				*/

	out[AX25_ADDR_LEN * 2] = AX25_CONTROL_UI;

	/*	PID field: no layer 3 protocol.				*/

	out[AX25_ADDR_LEN * 2 + 1] = AX25_PID_NOLAYER3;

	/*	Copy payload.						*/

	memcpy(out + AX25_HEADER_LEN, payload, payloadLen);

	return totalLen;
}

/*	ax25StripHeader: strip the AX.25 header from a received frame.
 *
 *	Sets *payload to point to the first byte after the 16-byte
 *	AX.25 header.
 *
 *	Returns the payload length, or -1 on error.			*/

int	ax25StripHeader(unsigned char *frame, int frameLen,
		unsigned char **payload)
{
	if (frame == NULL || payload == NULL)
	{
		return -1;
	}

	if (frameLen <= AX25_HEADER_LEN)
	{
		return -1;	/*	No payload.			*/
	}

	/*	Verify this looks like a UI frame.			*/

	if (frame[AX25_ADDR_LEN * 2] != AX25_CONTROL_UI)
	{
		return -1;	/*	Not a UI frame.			*/
	}

	*payload = frame + AX25_HEADER_LEN;
	return frameLen - AX25_HEADER_LEN;
}
