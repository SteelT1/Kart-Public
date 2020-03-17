// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  upnp.c
/// \brief UPnP initialization code and port mapping routines

#include "doomdef.h"
#include "i_system.h"
#include "miniupnpc/miniupnpc.h"
#include "miniupnpc/upnpcommands.h"
#include "miniupnpc/upnperrors.h"

boolean UPNP_support = false;
char upnp_portnum[8];

struct UPNPDev *upnp_dev;
int upnp_error;
// The internal (LAN) IP address
char lan_address[64];

// The external (WAN) IP address
char wan_address[64];

struct UPNPUrls upnp_urls;
struct IGDdatas upnp_data;

void ShutdownUPnP(void);
boolean InitUPnP(void);
boolean AddPortMapping(const char *addr, const char *port);
boolean DeletePortMapping(const char *port);
boolean CheckPortMapping(void);

void ShutdownUPnP(void)
{
	FreeUPNPUrls(&upnp_urls);
	freeUPNPDevlist(upnp_dev);
	CONS_Printf(M_GetText("InitUPnP(): shut down\n"));
	return;
}

boolean InitUPnP(void)
{
	int status;
	int r;
	CONS_Printf(M_GetText("InitUPnP()\n"));
	upnp_dev = upnpDiscover(2000, NULL, NULL, 0, 0, 2, &upnp_error); // Search for UPnP devices

	if (upnp_dev)
	{
		status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, sizeof(lan_address)); // Get a valid internet gateway device
		if (status == 1)
		{
			r = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);
			if (r != 0)
			{
				CONS_Alert(CONS_ERROR, M_GetText("Failed to get external IP address\n"));
				return false;
			}
			CONS_Printf(M_GetText("Local IP address: %s\n"), lan_address);
			CONS_Printf(M_GetText("External IP address: %s\n"), wan_address);
		}
		else
		{
			CONS_Alert(CONS_ERROR, M_GetText("No valid IGD was found\n"));
			return false;
		}
	}
	else
	{
		CONS_Alert(CONS_ERROR, M_GetText("No UPnP devices discovered\n"));
		return false;
	}

	return true;
}

boolean AddPortMapping(const char *addr, const char *port)
{
	int status;

	if (addr == NULL)
		addr = lan_address;

	if (!upnp_urls.controlURL || upnp_urls.controlURL[0] == '\0')
		return false;

	status = UPNP_AddPortMapping(upnp_urls.controlURL, upnp_data.first.servicetype, port, port, addr, "SRB2Kart", "UDP", NULL, 0);

	if (status != UPNPCOMMAND_SUCCESS)
	{
		CONS_Alert(CONS_ERROR, M_GetText("UPNP_AddPortMapping() failed with code %d (%s)\n"), status, strupnperror(status));
		return false;
	}
	return true;
}

boolean DeletePortMapping(const char *port)
{
	int status;

	if (!upnp_urls.controlURL || upnp_urls.controlURL[0] == '\0')
		return false;

	status = UPNP_DeletePortMapping(upnp_urls.controlURL, upnp_data.first.servicetype, port, "UDP", NULL);

	if (status != UPNPCOMMAND_SUCCESS)
	{
		CONS_Alert(CONS_ERROR, M_GetText("UPNP_DeletePortMapping() failed with code %d (%s)\n"), status, strupnperror(status));
		return false;
	}
	return true;
}

boolean CheckPortMapping(void)
{
	int status;
	char intPort[8];
	char intClient[16];

	if (!upnp_urls.controlURL || upnp_urls.controlURL[0] == '\0')
		return false;

	status = UPNP_GetSpecificPortMappingEntry(upnp_urls.controlURL, upnp_data.first.servicetype, upnp_portnum, "UDP", NULL, intClient, intPort, NULL, NULL, NULL);

	if (status != UPNPCOMMAND_SUCCESS)
		return false;
	return true;
}