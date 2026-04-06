/*
 *	ax25frame.c:	AX.25 UI frame construction and parsing for
 *			the LTP KISS CLA. Wraps LTP segments in AX.25
 *			frames with amateur radio callsign addressing.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team / Cislunar Amateur DTN Project
 */

#include "ltpkiss.h"
#include <string.h>
#include <ctype.h>

/*	Encode a callsign into a 7-byte AX.25 address field.
 *	Callsign is space-padded to 6 chars, each byte left-shifted by 1.
 *	SSID byte: 0b011SSSS0 with extension bit set if lastAddr.	*/

static void	encodeAddress(const char *callsign, int ssid, int lastAddr,
			unsigned char *out)
{
	int	i;
	int	len;
	unsigned char	ssidByte;

	len = strlen(callsign);
	if (len > AX25_CALLSIGN_LEN)
	{
		len = AX25_CALLSIGN_LEN;
	}

	/*	Encode callsign characters, left-shifted by 1.		*/

	for (i = 0; i < len; i++)
	{
		out[i] = (unsigned char)(toupper(callsign[i])) << 1;
	}

	/*	Pad remaining positions with space << 1.		*/

	for (i = len; i < AX25_CALLSIGN_LEN; i++)
	{
		out[i] = ' ' << 1;
	}

	/*	SSID byte: bits 6,5 = 1,1 (reserved), bits 4-1 = SSID,
	 *	bit 0 = extension bit (1 if last address field).	*/

	ssidByte = 0x60 | ((ssid & 0x0F) << 1);
	if (lastAddr)
	{
		ssidByte |= 0x01;
	}

	out[AX25_CALLSIGN_LEN] = ssidByte;
}

/*	Decode a 7-byte AX.25 address field into callsign and SSID.	*/

static void	decodeAddress(const unsigned char *addr, char *callsign,
			int *ssid)
{
	int	i;
	int	len;

	/*	Right-shift each byte by 1 to recover ASCII.		*/

	for (i = 0; i < AX25_CALLSIGN_LEN; i++)
	{
		callsign[i] = addr[i] >> 1;
	}

	callsign[AX25_CALLSIGN_LEN] = '\0';

	/*	Trim trailing spaces.					*/

	len = AX25_CALLSIGN_LEN;
	while (len > 0 && callsign[len - 1] == ' ')
	{
		callsign[--len] = '\0';
	}

	/*	Extract SSID from bits 4-1 of the SSID byte.		*/

	*ssid = (addr[AX25_CALLSIGN_LEN] >> 1) & 0x0F;
}

int	ax25BuildUIFrame(const KissConfig *config,
		const unsigned char *payload, int payloadLen,
		unsigned char *output, int *outputLen)
{
	int	totalLen;

	if (config == NULL || payload == NULL || output == NULL
		|| outputLen == NULL)
	{
		putErrmsg("Invalid parameters to ax25BuildUIFrame.", NULL);
		return -1;
	}

	if (payloadLen < 0)
	{
		putErrmsg("Invalid payload length.", itoa(payloadLen));
		return -1;
	}

	totalLen = AX25_HEADER_LEN + payloadLen;
	if (totalLen > MAX_KISS_FRAME_SIZE)
	{
		putErrmsg("AX.25 frame too large.", itoa(totalLen));
		return -1;
	}

	/*	Encode destination address (not last).			*/

	encodeAddress(config->dstCallsign, config->dstSSID, 0, output);

	/*	Encode source address (last address field).		*/

	encodeAddress(config->srcCallsign, config->srcSSID, 1,
			output + AX25_ADDR_LEN);

	/*	Control field: UI (Unnumbered Information).		*/

	output[2 * AX25_ADDR_LEN] = AX25_CONTROL_UI;

	/*	PID field: No layer 3 protocol.				*/

	output[2 * AX25_ADDR_LEN + 1] = AX25_PID_NOLAYER3;

	/*	Copy payload (LTP segment) into information field.	*/

	memcpy(output + AX25_HEADER_LEN, payload, payloadLen);

	*outputLen = totalLen;
	return 0;
}

int	ax25StripHeader(const unsigned char *frame, int frameLen,
		const unsigned char **payload, int *payloadLen)
{
	if (frame == NULL || payload == NULL || payloadLen == NULL)
	{
		putErrmsg("Invalid parameters to ax25StripHeader.", NULL);
		return -1;
	}

	if (frameLen < AX25_HEADER_LEN)
	{
		putErrmsg("Frame too short for AX.25 header.",
				itoa(frameLen));
		return -1;
	}

	/*	Verify control field is UI.				*/

	if (frame[2 * AX25_ADDR_LEN] != AX25_CONTROL_UI)
	{
		putErrmsg("Not a UI frame.", NULL);
		return -1;
	}

	/*	Point payload past the header.				*/

	*payload = frame + AX25_HEADER_LEN;
	*payloadLen = frameLen - AX25_HEADER_LEN;

	return 0;
}
