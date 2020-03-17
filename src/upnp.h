// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  upnp.h
/// \brief UPnP initialization code and port mapping routines

extern boolean UPNP_support;
extern char upnp_portnum[8];

/**	\brief ShutdownUPnP function

	\return	none
*/
void ShutdownUPnP(void);

/**	\brief InitUPnP function

	\return	true if successful, false if not
*/
boolean InitUPnP(void);

/**	\brief AddPortMapping function

	\return	true if successful, false if not
*/
boolean AddPortMapping(const char *addr, const char *port);

/**	\brief DeletePortMapping function

	\return	true if successful, false if not
*/
boolean DeletePortMapping(const char *port);

/**	\brief DeletePortMapping function

	\return	true if successful, false if not
*/
boolean CheckPortMapping(void);