// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
// \brief HTTP based file downloader

#include <time.h>
#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "d_netfil.h"
#include "d_net.h"
#include "i_system.h"
#include "http_dl.h"

static int running_handles = 0;
static CURLM *curlm = NULL; // The multi handle used to keep track of all ongoing transfers
static CURL *curl = NULL; // Handle to set common opts with
static boolean httpdl_isinit = false;
UINT32 httpdl_active_jobs = 0; // Number of currently ongoing jobs
UINT32 httpdl_total_jobs = 0; // Number of total jobs
UINT32 httpdl_faileddownloads = 0; // Number of failed downloads
HTTP_login *httpdl_logins;

/*
 * Callback function to write received data to file
 * Required because on win32 not setting CURLOPT_WRITEFUNCTION can cause crashes if CURLOPT_WRITEDATA is set
 * And there's no way avoiding that
 * See: https://curl.se/libcurl/c/CURLOPT_WRITEDATA.html
 */
static size_t write_data_cb(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

/* Callback function to show progress information
 * Used to update the download screen meter */
static int progress_cb(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	httpdl_info_t di;
	INT32 dlspeed = 0;
	// Function prototype requires these but we won't use, so just discard
	(void)ultotal;
	(void)ulnow; 
	di = *(httpdl_info_t *)clientp;
	di.fileinfo->currentsize = (UINT32)dlnow;
	di.curtime = I_GetTime()/TICRATE;
	dlspeed = dlnow / (di.curtime - di.starttime);
	if (dlspeed > 0)
		getbytes = dlspeed;
	return 0;
}

static void set_common_opts(void)
{
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_cb);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_cb);
	
	// Only allow HTTP and HTTPS
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, va("%s/%s", SRB2APPLICATION, VERSIONSTRING)); // Set user agent as some servers won't accept invalid user agents.

	// Follow a redirect request, if sent by the server.
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

	// abort if slower than 1 bytes/sec during 10 seconds
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
}

static void cleanup_download(httpdl_info_t *download)
{
	httpdl_active_jobs--;
	httpdl_total_jobs--;
	curl_multi_remove_handle(curlm, download->handle);
	curl_easy_cleanup(download->handle);
	download->handle = NULL;
}

boolean HTTPDL_Init(void)
{
	// Should only be called once
	if (httpdl_isinit)
	{
		CONS_Alert(CONS_ERROR, "HTTPDL: Already initialized\n");
		return false;
	}
	
	if (curl_global_init(CURL_GLOBAL_ALL))
	{
		CONS_Alert(CONS_ERROR, "HTTPDL: Could not init cURL\n");
		return false;
	}
		
	curlm = curl_multi_init();
	
	if (!curlm)
	{
		CONS_Alert(CONS_ERROR, "HTTPDL: Could not create a multi handle");
		return false;
	}
	
	curl = curl_easy_init();
	
	if (!curl)
	{
		CONS_Alert(CONS_ERROR, "HTTPDL: Could not create easy handle");
		return false;
	}
	
	set_common_opts();
	httpdl_isinit = true;
	return true;
}

void HTTPDL_Cleanup(httpdl_info_t *download)
{
	UINT32 i;

	if (httpdl_isinit)
    {
    	for (i = 0; i < httpdl_total_jobs; i++)
    	{
    		if (download[i].handle != NULL)
				cleanup_download(download);
    	}

		curl_multi_cleanup(curlm);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
    }
    
    memset(download, 0, sizeof(*download));
	httpdl_isinit = false;
}

boolean HTTPDL_AddDownload(httpdl_info_t *download, const char* url)
{
	HTTP_login *login;
	char *output = NULL;
#ifdef PARANOIA
	if (M_CheckParm("-nodownload"))
		I_Error("Attempted to download files in -nodownload mode");
#endif

	download->handle = curl_easy_duphandle(curl);

	if (download->handle)
	{
		strlcpy(download->basename, download->fileinfo->filename, sizeof(download->basename));
		snprintf(download->url, sizeof(download->url), "%s/%s", url, download->basename);
			
		// URL encode the string
		output = curl_easy_escape(download->handle, download->basename, 0);
			
		if (output)
		{
			// Copy the encoded URL 
			snprintf(download->url, sizeof(download->url), "%s/%s", url, output);
			curl_free(output);
			output = NULL;
		}
			
		// Set transfer specific options
		curl_easy_setopt(download->handle, CURLOPT_URL, download->url);	
			
		strcatbf(download->fileinfo->filename, downloaddir, "/");
		download->fileinfo->file = fopen(download->fileinfo->filename, "wb");
		curl_easy_setopt(download->handle, CURLOPT_WRITEDATA, download->fileinfo->file);
			
		// Authenticate if the user so wishes
		login = HTTPDL_GetLogin(url, NULL);

		if (login)
		{
			curl_easy_setopt(download->handle, CURLOPT_USERPWD, login->auth);
		}
		
		// provide a buffer to store errors in
		curl_easy_setopt(download->handle, CURLOPT_ERRORBUFFER, download->error_buffer);		
		curl_easy_setopt(download->handle, CURLOPT_PROGRESSDATA, download);

		// Store a pointer to download info
		curl_easy_setopt(download->handle, CURLOPT_PRIVATE, download);
		
		CONS_Printf(M_GetText("HTTPDL: Downloading %s\n"), download->url);
		curl_multi_add_handle(curlm, download->handle);
		lastfilenum = download->filenum;
		download->starttime = I_GetTime()/TICRATE;
		download->fileinfo->status = FS_DOWNLOADING;
		return true;
	}
	return false;
}

boolean HTTPDL_DownloadFiles(void)
{
	CURLMcode mc;
	if (!curlm)
		return false;

	mc = curl_multi_perform(curlm, &running_handles);

    if (running_handles)
    {
		if (mc == CURLM_OK)
		{
			// wait for activity, timeout or "nothing"
			mc = curl_multi_wait(curlm, NULL, 0, 1000, NULL);
		}
			
		if (mc != CURLM_OK)
		{
			CONS_Alert(CONS_ERROR, "HTTPDL: curl_multi_perform error %d\n", mc);
			return false;
		}
    }
    return true;
}

void HTTPDL_CheckDownloads(void)
{
	CURLMsg *m; // for picking up messages with the transfer status
	CURLcode easyres; // Result from easy handle for a transfer
	int msg_left;
	httpdl_info_t *download;

	// Check if any transfers are done, and if so, report the status
	do 
	{
		m = curl_multi_info_read(curlm, &msg_left);
		if (m && (m->msg == CURLMSG_DONE))
		{
			easyres = m->data.result;

			// Get download info for this transfer
			curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &download);
			if (easyres != CURLE_OK)
			{
				if (download->error_buffer[0] == '\0')
					strlcpy(download->error_buffer, curl_easy_strerror(easyres), CURL_ERROR_SIZE);
				
				download->fileinfo->status = FS_FALLBACK;
				fclose(download->fileinfo->file);
				remove(download->fileinfo->filename);
				download->fileinfo->file = NULL;
				CONS_Alert(CONS_ERROR, M_GetText("HTTPDL: Failed to download %s (%s)\n"), download->url, download->error_buffer);
				httpdl_faileddownloads++;
			}
			else
			{
				CONS_Printf(M_GetText("HTTPDL: Finished downloading %s\n"), download->url);
				downloadcompletednum++;
				downloadcompletedsize += download->fileinfo->totalsize;
				download->fileinfo->status = FS_FOUND;
				fclose(download->fileinfo->file);
			}

			cleanup_download(download);
			if (!httpdl_total_jobs)
				break;
		}
	} while (m);
}

HTTP_login *
HTTPDL_GetLogin (const char *url, HTTP_login ***return_prev_next)
{
	HTTP_login  * login;
	HTTP_login ** prev_next;

	for (
			prev_next = &httpdl_logins;
			( login = (*prev_next));
			prev_next = &login->next
	){
		if (strcmp(login->url, url) == 0)
		{
			if (return_prev_next)
				(*return_prev_next) = prev_next;

			return login;
		}
	}

	return NULL;
}