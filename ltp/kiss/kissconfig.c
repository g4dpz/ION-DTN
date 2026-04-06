/*
 *	kissconfig.c:	KISS CLA configuration management.
 *
 *	Copyright (c) 2024, California Institute of Technology.
 *	ALL RIGHTS RESERVED.  U.S. Government Sponsorship acknowledged.
 *
 *	Author: ION Development Team
 */

#include "ltpkissP.h"
#include <string.h>

/*	*	*	Configuration loading	*	*	*	*/

int	loadKissConfig(uvast engineId, KissConfig *config)
{
	char	engineIdStr[32];
	char	configLine[512];
	int	configFile;
	int	lineLen;
	char	*token;
	int	found = 0;

	CHKERR(config);

	/*	Set default values.					*/

	memset(config, 0, sizeof(KissConfig));
	config->baudRate = DEFAULT_BAUD_RATE;
	config->mtu = DEFAULT_MTU;
	config->maxRate = DEFAULT_MAX_RATE;
	config->useFlowControl = 0;
	config->reconnectDelay = 5;
	config->frameTimeout = 5000;

	/*	Try to load from .ionconfig file.			*/

	isprintf(engineIdStr, sizeof engineIdStr, UVAST_FIELDSPEC, engineId);
	configFile = iopen("kiss.ionconfig", O_RDONLY, 0);
	if (configFile < 0)
	{
		/*	No config file, use defaults.			*/

		writeMemo("[i] No kiss.ionconfig found, using defaults.");
		istrcpy(config->devicePath, "/dev/ttyUSB0",
				sizeof config->devicePath);
		return 0;
	}

	/*	Parse configuration file.				*/

	while (igets(configFile, configLine, sizeof configLine, &lineLen)
			!= NULL)
	{
		/*	Skip comments and blank lines.			*/

		if (lineLen == 0 || configLine[0] == '#')
		{
			continue;
		}

		/*	Look for: <engineId> <device> <baud> <mtu> <maxRate>	*/

		token = strtok(configLine, " \t\n");
		if (token == NULL)
		{
			continue;
		}

		if (strcmp(token, engineIdStr) != 0)
		{
			continue;	/*	Not for this engine.	*/
		}

		/*	Found configuration for this engine.		*/

		found = 1;

		/*	Parse device path.				*/

		token = strtok(NULL, " \t\n");
		if (token == NULL)
		{
			putErrmsg("Missing device path in config.", NULL);
			close(configFile);
			return -1;
		}

		istrcpy(config->devicePath, token, sizeof config->devicePath);

		/*	Parse baud rate (optional).			*/

		token = strtok(NULL, " \t\n");
		if (token != NULL)
		{
			config->baudRate = atoi(token);
		}

		/*	Parse MTU (optional).				*/

		token = strtok(NULL, " \t\n");
		if (token != NULL)
		{
			config->mtu = atoi(token);
		}

		/*	Parse max rate (optional).			*/

		token = strtok(NULL, " \t\n");
		if (token != NULL)
		{
			config->maxRate = atoi(token);
		}

		/*	Parse flow control flag (optional).		*/

		token = strtok(NULL, " \t\n");
		if (token != NULL)
		{
			config->useFlowControl = atoi(token);
		}

		/*	Parse source callsign (optional).		*/

		token = strtok(NULL, " \t\n");
		if (token != NULL)
		{
			istrcpy(config->srcCallsign, token,
					sizeof config->srcCallsign);
			config->useAX25 = 1;

			/*	Parse source SSID (optional).		*/

			token = strtok(NULL, " \t\n");
			if (token != NULL)
			{
				config->srcSSID = atoi(token);
			}

			/*	Parse destination callsign (optional).	*/

			token = strtok(NULL, " \t\n");
			if (token != NULL)
			{
				istrcpy(config->dstCallsign, token,
					sizeof config->dstCallsign);

				/*	Parse dest SSID (optional).	*/

				token = strtok(NULL, " \t\n");
				if (token != NULL)
				{
					config->dstSSID = atoi(token);
				}
			}
		}

		break;
	}

	close(configFile);

	if (!found)
	{
		putErrmsg("No KISS config found for engine.", engineIdStr);
		return -1;
	}

	/*	Validate configuration.					*/

	if (config->devicePath[0] == '\0')
	{
		putErrmsg("Device path is empty.", NULL);
		return -1;
	}

	if (config->baudRate <= 0)
	{
		putErrmsg("Invalid baud rate.", itoa(config->baudRate));
		return -1;
	}

	if (config->mtu <= 0 || config->mtu > MAX_KISS_FRAME_SIZE)
	{
		putErrmsg("Invalid MTU.", itoa(config->mtu));
		return -1;
	}

	if (config->maxRate <= 0)
	{
		putErrmsg("Invalid max rate.", itoa(config->maxRate));
		return -1;
	}

	return 0;
}
