// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2021 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
// \brief HTTP based file downloader

#ifndef _HTTPDL_H_
#define _HTTPDL_H_

#include "doomtype.h"
#include "w_wad.h"
#include "d_clisrv.h"
#include <curl/curl.h>
//#define HTTP_MULTI_DL // Undefine this to disable multiple downloads support

// Information of a curl transfer (one for each transfer)
typedef struct curlinfo_s
{
	char url[HTTP_MAX_URL_LENGTH]; // The url for this transfer
	//time_t starttime; // The time when this transfer was started
	CURL *handle; // The easy handle for this transfer
	CURLSH *share; // Copy of the share handle
	char filename[MAX_WADPATH]; // Name of the file
	fileneeded_t *fileinfo; // The fileneeded_t for this transfer
	UINT32 id; // the id of the transafrr
} curlinfo_t;

extern CURLSH *curlshare; // Handle for sharing a single connection pool
extern UINT32 httpdl_active_transfers; // Number of currently ongoing transfers
extern UINT32 httpdl_total_transfers; // Number of total tranfeers
extern boolean httpdl_faileddownload; // Did a download fail?
extern boolean httpdl_wasinit;
extern curlinfo_t curl[MAX_WADFILES];
//extern SINT8 curl_initstatus;
extern char http_source[HTTP_MAX_URL_LENGTH];

boolean HTTPDL_Init(void);
void HTTPDL_Quit(void);
boolean HTTPDL_AddTransfer(curlinfo_t *curl, const char* url, int filenum);

#endif // _HTTPDL_H_