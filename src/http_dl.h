// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
// \brief HTTP based file downloader

#ifndef _HTTPDL_H_
#define _HTTPDL_H_

#include <curl/curl.h>
#include "doomtype.h"
#include "w_wad.h"
#include "d_clisrv.h"

// Information of a download (one for each download)
typedef struct httpdl_info_s
{
	INT32 filenum; 
	char url[HTTPDL_MAX_URL_LENGTH]; // The url for this download
	tic_t starttime;
	tic_t curtime;
	CURL *handle; // The easy handle for this download 
	char basename[MAX_WADPATH]; // Name of the file, without full path
	fileneeded_t *fileinfo;
	char error_buffer[CURL_ERROR_SIZE]; // Buffer to store error messages
} httpdl_info_t;

typedef struct HTTP_login HTTP_login;

extern struct HTTP_login
{
	char       * url;
	char       * auth;
	HTTP_login * next;
}
*httpdl_logins;

extern UINT32 httpdl_active_jobs; // Number of currently ongoing download
extern UINT32 httpdl_total_jobs; // Number of total download
extern UINT32 httpdl_faileddownloads; // Number of failed downloads
extern httpdl_info_t httpdl_downloads[MAX_WADFILES];
extern char http_source[HTTPDL_MAX_URL_LENGTH];

boolean HTTPDL_Init(void);
void HTTPDL_Cleanup(httpdl_info_t *download);
boolean HTTPDL_AddDownload(httpdl_info_t *curl, const char* url);
boolean HTTPDL_DownloadFiles(void);
extern void HTTPDL_CheckDownloads(void);
HTTP_login * HTTPDL_GetLogin (const char *url, HTTP_login ***return_prev_next);

#endif // _HTTPDL_H_
