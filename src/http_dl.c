// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Victor "SteelT" Fuentes.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
// \brief HTTP based file downloader

#include <curl/curl.h>
#include <time.h>
#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "d_netfil.h"
#include "d_net.h"
#include "i_system.h"
#include "http_dl.h"

static int running_handles = 0;
static CURLM *multi_handle; // The multi handle used to keep track of all ongoing transfers
static int numfds;
static int repeats = 0;
SINT8 httpdl_initstatus = 0;
UINT32 httpdl_active_jobs = 0; // Number of currently ongoing jobs
UINT32 httpdl_total_jobs = 0; // Number of total jobs
boolean httpdl_faileddownload = false; // Did a download fail?
HTTP_login *httpdl_logins;

/* Callback function to write received data to file 
 * Required because on win32 not setting can cause crashes if CURLOPT_WRITEFUNCTION isn't set but CURLOPT_WRITEDATA is
 * And there's no way avoiding that
 * See: https://curl.se/libcurl/c/CURLOPT_WRITEDATA.html */
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
	di.fileinfo->totalsize = (UINT32)dltotal;
	di.curtime = I_GetTime()/TICRATE;
	dlspeed = dlnow / (di.curtime - di.starttime);
	if (dlspeed > 0)
		getbytes = dlspeed;
	return 0;
}

static void set_common_opts(httpdl_info_t *transfer)
{
	curl_easy_setopt(transfer->handle, CURLOPT_WRITEFUNCTION, write_data_cb);
	curl_easy_setopt(transfer->handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(transfer->handle, CURLOPT_PROGRESSFUNCTION, progress_cb);
	curl_easy_setopt(transfer->handle, CURLOPT_PROGRESSDATA, transfer);
	
	// Only allow HTTP and HTTPS
	curl_easy_setopt(transfer->handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	curl_easy_setopt(transfer->handle, CURLOPT_USERAGENT, va("SRB2Kart/v%d.%d", VERSION, SUBVERSION)); // Set user agent as some servers won't accept invalid user agents.

	// Follow a redirect request, if sent by the server.
	curl_easy_setopt(transfer->handle, CURLOPT_FOLLOWLOCATION, 1L);

	curl_easy_setopt(transfer->handle, CURLOPT_FAILONERROR, 1L);

	// abort if slower than 1 bytes/sec during 10 seconds
	curl_easy_setopt(transfer->handle, CURLOPT_LOW_SPEED_TIME, 10L);
	curl_easy_setopt(transfer->handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
	
	// provide a buffer to store errors in
	curl_easy_setopt(transfer->handle, CURLOPT_ERRORBUFFER, transfer->error_buffer);
}

static void cleanup_download(httpdl_info_t *download)
{
	httpdl_active_jobs--;
	httpdl_total_jobs--;
	curl_multi_remove_handle(multi_handle, download->handle);
	curl_easy_cleanup(download->handle);
	download->handle = NULL;
}

void HTTPDL_Cleanup(httpdl_info_t *download)
{
	UINT32 i;

	if (httpdl_initstatus == 1)
    {
    	for (i = 0; i < httpdl_total_jobs; i++)
    	{
    		if (download[i].handle != NULL)
				cleanup_download(download);
    	}

		curl_multi_cleanup(multi_handle);
		curl_global_cleanup();
    	httpdl_initstatus = 0;
    }
    
    httpdl_active_jobs = 0;
	httpdl_total_jobs = 0;
	httpdl_faileddownload = false;
	memset(download, 0, sizeof(*download));
}

boolean HTTPDL_AddDownload(httpdl_info_t *download, const char* url, int filenum)
{
	HTTP_login *login;
	char *output = NULL;
#ifdef PARANOIA
	if (M_CheckParm("-nodownload"))
		I_Error("Attempted to download files in -nodownload mode");
#endif

	if (httpdl_initstatus == 0)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		multi_handle = curl_multi_init();
		
		if (multi_handle)
			httpdl_initstatus = 1;
	}

	if (httpdl_initstatus == 1)
	{
		download->handle = curl_easy_init();

		if (download->handle)
		{
			set_common_opts(download);

			I_mkdir(downloaddir, 0755);

			strlcpy(download->filename, download->fileinfo->filename, sizeof(download->filename));
			snprintf(download->url, sizeof(download->url), "%s/%s", url, download->filename);
			
			// URL encode the string
			output = curl_easy_escape(download->handle, download->filename, 0);
			
			if (output)
			{
				// Copy the encoded URL 
				snprintf(download->url, sizeof(download->url), "%s/%s", url, output);
				curl_free(output);
				output = NULL;
			}
				
			curl_easy_setopt(download->handle, CURLOPT_URL, download->url);	
			
			// Authenticate if the user so wishes
			login = HTTPDL_GetLogin(url, NULL);

			if (login)
			{
				curl_easy_setopt(download->handle, CURLOPT_USERPWD, login->auth);
			}			

			strcatbf(download->fileinfo->filename, downloaddir, "/");
			download->fileinfo->file = fopen(download->fileinfo->filename, "wb");
			curl_easy_setopt(download->handle, CURLOPT_WRITEDATA, download->fileinfo->file);
		
			CONS_Printf(M_GetText("URL: %s; added to download queue\n"), download->url);
			curl_multi_add_handle(multi_handle, download->handle);
			download->starttime = I_GetTime()/TICRATE;
			download->fileinfo->status = FS_DOWNLOADING;
			lastfilenum = filenum;
			return true;
		}
	}
	return false;
}

void HTTPDL_DownloadFiles(void)
{
	CURLMcode mc;
	
    if (multi_handle)
    {
		mc = curl_multi_perform(multi_handle, &running_handles);
		
    	if (running_handles)
      	{	
			if (mc == CURLM_OK) 
			{
				// wait for activity, timeout or "nothing" 
				curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
			}
			
			if (mc != CURLM_OK)
			{
				CONS_Debug(DBG_NETPLAY, "curl_multi failed, code %d.\n", mc);
				return;
			}
			
			if (!numfds) 
			{
				repeats++; // count number of repeated zero numfds 
				
				if (repeats > 1) 
					I_Sleep();
			}
			else
				repeats = 0;
      	}
    }
}

void HTTPDL_CheckDownloads(httpdl_info_t *download)
{
	CURLMsg *m; // for picking up messages with the transfer status
	CURLcode easyres; // Result from easy handle for transfer
	int msg_left;

	// See how the downloads went
	while ((m = curl_multi_info_read(multi_handle, &msg_left)))
	{
		if (m && (m->msg == CURLMSG_DONE))
		{
			easyres = m->data.result;
			if (easyres != CURLE_OK)
			{
				if (download->error_buffer[0] == '\0')
					strlcpy(download->error_buffer, curl_easy_strerror(easyres), CURL_ERROR_SIZE);
				
				download->fileinfo->status = FS_FALLBACK;
				fclose(download->fileinfo->file);
				remove(download->fileinfo->filename);
				download->fileinfo->file = NULL;
				CONS_Alert(CONS_ERROR, M_GetText("Failed to download %s (%s)\n"), download->filename, download->error_buffer);
				httpdl_faileddownload = true;
			}
			else
			{
				CONS_Printf(M_GetText("Finished downloading %s\n"), download->filename);
				downloadcompletednum++;
				downloadcompletedsize += download->fileinfo->totalsize;
				download->fileinfo->status = FS_FOUND;
				fclose(download->fileinfo->file);
			}

			cleanup_download(download);
			if (!httpdl_total_jobs)
				break;
		}
	}
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
