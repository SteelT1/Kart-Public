// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Victor "SteelT" Fuentes.
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
#define HTTP_MULTI_DL // Undefine this to disable multiple downloads support

// Information of a curl transfer (one for each transfer)
typedef struct curlinfo_s
{
	char url[HTTP_MAX_URL_LENGTH]; // The url for this transfer
	time_t starttime; // The time when this transfer was started
	CURL *handle; // The easy handle for this transfer 
	char filename[MAX_WADPATH]; // Name of the file
	fileneeded_t *fileinfo; // The fileneeded_t for this transfer
} curlinfo_t;

typedef struct HTTP_login HTTP_login;

extern struct HTTP_login
{
	char       * url;
	char       * auth;
	HTTP_login * next;
}
*curl_logins;

extern UINT32 curl_active_transfers; // Number of currently ongoing transfers
extern UINT32 curl_total_transfers; // Number of total tranfeers
extern boolean curl_faileddownload; // Did a download fail?
extern curlinfo_t curli[MAX_WADFILES];
extern SINT8 curl_initstatus;
extern char http_source[HTTP_MAX_URL_LENGTH];

void CURL_Cleanup(curlinfo_t *curl);
boolean CURL_AddTransfer(curlinfo_t *curl, const char* url, int filenum);
void CURL_DownloadFiles(void);
extern void CURL_CheckDownloads(curlinfo_t *ti);
HTTP_login * CURLGetLogin (const char *url, HTTP_login ***return_prev_next);

#endif // _HTTPDL_H_
