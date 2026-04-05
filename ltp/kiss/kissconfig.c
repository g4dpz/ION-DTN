/*
	kissconfig.c:	Configuration file parser for KISS CLA.

			Parses kiss.ionconfig to load serial port
			and AX.25 parameters for a given LTP engine.

	Author: David Johnson

	Copyright (c) 2025, All rights reserved.

	Config file format (one line per remote engine):
	<engineId> <device> <baudRate> <mtu> <maxRate> <flowControl> [srcCall] [srcSSID] [dstCall] [dstSSID]

	If callsigns are present, AX.25 framing is enabled.
	Defaults: /dev/ttyUSB0, 9600, 512, 960, 0
									*/

#include "ltpkisslsa.h"

/*	loadKissConfig: parse kiss.ionconfig for the given engine ID.
 *
 *	Searches for a line matching the specified remote engine ID
 *	and populates the KissConfig structure.
 *
 *	Returns 0 on success, -1 on error.				*/

int	loadKissConfig(uvast engineId, KissConfig *config)
{
	FILE	*configFile;
	char	line[1024];
	uvast	lineEngineId;
	char	device[256];
	int	baudRate;
	int	mtu;
	int	maxRate;
	int	flowControl;
	char	srcCall[16];
	int	srcSSID;
	char	dstCall[16];
	int	dstSSID;
	int	fieldsRead;
	int	found;

	if (config == NULL)
	{
		return -1;
	}

	/*	Set defaults.						*/

	memset(config, 0, sizeof(KissConfig));
	istrcpy(config->devicePath, "/dev/ttyUSB0",
			sizeof(config->devicePath));
	config->baudRate = 9600;
	config->mtu = KISS_DEFAULT_MTU;
	config->maxRate = 960;
	config->useFlowControl = 0;
	config->useAX25 = 0;
	config->srcSSID = 0;
	config->dstSSID = 0;

	/*	Open the config file.					*/

	configFile = fopen("kiss.ionconfig", "r");
	if (configFile == NULL)
	{
		putErrmsg("Can't open kiss.ionconfig.", NULL);
		return -1;
	}

	/*	Search for a line matching the engine ID.		*/

	found = 0;
	while (fgets(line, sizeof(line), configFile) != NULL)
	{
		/*	Skip comments and blank lines.			*/

		if (line[0] == '#' || line[0] == '\n'
				|| line[0] == '\r')
		{
			continue;
		}

		/*	Try to parse with callsigns first.		*/

		memset(srcCall, 0, sizeof(srcCall));
		memset(dstCall, 0, sizeof(dstCall));
		srcSSID = 0;
		dstSSID = 0;

		fieldsRead = sscanf(line,
				UVAST_FIELDSPEC " %255s %d %d %d %d %15s %d %15s %d",
				&lineEngineId, device, &baudRate,
				&mtu, &maxRate, &flowControl,
				srcCall, &srcSSID, dstCall, &dstSSID);

		if (fieldsRead < 6)
		{
			continue;	/*	Malformed line.		*/
		}

		if (lineEngineId != engineId)
		{
			continue;	/*	Not our engine.		*/
		}

		/*	Found a matching line.				*/

		istrcpy(config->devicePath, device,
				sizeof(config->devicePath));
		config->baudRate = baudRate;
		config->mtu = mtu;
		config->maxRate = maxRate;
		config->useFlowControl = flowControl;

		if (fieldsRead >= 10 && strlen(srcCall) > 0
				&& strlen(dstCall) > 0)
		{
			istrcpy(config->srcCallsign, srcCall,
					sizeof(config->srcCallsign));
			config->srcSSID = srcSSID;
			istrcpy(config->dstCallsign, dstCall,
					sizeof(config->dstCallsign));
			config->dstSSID = dstSSID;
			config->useAX25 = 1;
		}

		found = 1;
		break;
	}

	fclose(configFile);

	if (!found)
	{
		putErrmsg("No kiss.ionconfig entry for engine.",
				itoa(engineId));
		return -1;
	}

	return 0;
}
