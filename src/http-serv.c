// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2022 by "SteelT".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  http-serv.c
/// \brief HTTP server, used for dynamic file index of added wadfiles

#ifdef HAVE_HTTPSERV

#include <microhttpd.h>
#include "http-serv.h"
#include "doomdef.h"

struct MHD_Daemon *http_sv_daemon;
const char *page  = "<html><body>Hello, browser!</body></html>";

const char *get_ip(struct sockaddr)
{

}

static int answer_to_connection(void *cls, struct MHD_Connection *connection,
								const char *url,
								const char *method, const char *version,
								const char *upload_data,
								size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response;
	MHD_ConnectionInfo *coninfo;
	int ret;
	const char *ipaddr;

	coninfo = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	ipaddr = get_ip()

	CONS_Printf("HSV: New %s request for %s from %s using version %s\n", method, url, ipaddr, version);

	response = MHD_create_response_from_buffer(strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);

	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  	MHD_destroy_response(response);

  	return ret;
}

void HTTPSV_StartServer(void)
{
	CONS_Printf("HSV: Starting HTTP server...\n");
	http_sv_daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, HTTP_SV_PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_END);

	if (!http_sv_daemon)
	{
		CONS_Alert(CONS_ERROR, "HSV: Failed to start HTTP server");
		return;
	}

	CONS_Printf("HSV: listening on port %d\n", HTTP_SV_PORT);;
}

void HTTPSV_StopServer(void)
{
	if (http_sv_daemon)
		MHD_stop_daemon(http_sv_daemon);
}

#endif // HAVE_HTTPSERV