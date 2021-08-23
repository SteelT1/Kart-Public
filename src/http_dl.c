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
SINT8 curl_initstatus = 0;
UINT32 curl_active_transfers = 0; // Number of currently ongoing transfers
UINT32 curl_total_transfers = 0; // Number of total tranfeers
boolean curl_faileddownload = false; // Did a download fail?
HTTP_login *curl_logins;

static void ChangeFileExtension(char* filename, char* newExtension)
{
	static char* lastSlash;
	lastSlash = strstr(filename, ".");
    strlcpy(lastSlash, newExtension, strlen(lastSlash));
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

static int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	curlinfo_t ci;
	// Function prototype requires these but we won't use, so just discard
	(void)ultotal;
	(void)ulnow; 
	ci = *(curlinfo_t *)clientp;
	ci.fileinfo->currentsize = (UINT32)dlnow;
	ci.fileinfo->totalsize = (UINT32)dltotal;
	getbytes = dlnow / (time(NULL) - ci.starttime);
	return 0;
}

static void set_common_opts(curlinfo_t *ti)
{
	curl_easy_setopt(ti->handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(ti->handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(ti->handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt(ti->handle, CURLOPT_PROGRESSDATA, ti);
	
	// Only allow HTTP and HTTPS
	curl_easy_setopt(ti->handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	curl_easy_setopt(ti->handle, CURLOPT_USERAGENT, va("SRB2Kart/v%d.%d", VERSION, SUBVERSION)); // Set user agent as some servers won't accept invalid user agents.

	// Follow a redirect request, if sent by the server.
	curl_easy_setopt(ti->handle, CURLOPT_FOLLOWLOCATION, 1L);

	curl_easy_setopt(ti->handle, CURLOPT_FAILONERROR, 1L);

	// abort if slower than 30 bytes/sec during 60 seconds
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_TIME, 60L);
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_LIMIT, 30L);
}

void CURL_Cleanup(curlinfo_t *curlc)
{
	UINT32 i;

	if (curl_initstatus == 1)
    {
    	for (i = 0; i < curl_total_transfers; i++)
    	{
    		if (curlc[i].handle != NULL)
    		{
	    		curl_multi_remove_handle(multi_handle, curlc[i].handle);
				curl_easy_cleanup(curlc[i].handle);
    		}
    	}

		curl_multi_cleanup(multi_handle);
		curl_global_cleanup();
    	curl_initstatus = 0;
    }
}

boolean CURL_AddTransfer(curlinfo_t *curl, const char* url, int filenum)
{
	HTTP_login *login;	
#ifdef PARANOIA
	if (M_CheckParm("-nodownload"))
		I_Error("Attempted to download files in -nodownload mode");
#endif

	if (curl_initstatus == 0)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		multi_handle = curl_multi_init();
		
		if (multi_handle)
		{
			// only keep 10 connections in the cache
			curl_multi_setopt(multi_handle, CURLMOPT_MAXCONNECTS, 10L);
			curl_initstatus = 1;
		}
	}

	if (curl_initstatus == 1)
	{
		curl->handle = curl_easy_init();

		if (curl->handle)
		{
			set_common_opts(curl);

			I_mkdir(downloaddir, 0755);

			strlcpy(curl->filename, curl->fileinfo->filename, sizeof(curl->filename));
			snprintf(curl->url, sizeof(curl->url), "%s/%s", url, curl->filename);

			curl_easy_setopt(curl->handle, CURLOPT_URL, curl->url);
			
			// Authenticate if the user so wishes
			login = CURLGetLogin(url, NULL);

			if (login)
			{
				curl_easy_setopt(curl->handle, CURLOPT_USERPWD, login->auth);
			}			

			strcatbf(curl->fileinfo->filename, downloaddir, "/");
			curl->fileinfo->file = fopen(curl->fileinfo->filename, "wb");
			curl_easy_setopt(curl->handle, CURLOPT_WRITEDATA, curl->fileinfo->file);
		
			CONS_Printf(M_GetText("[File]: %s [URL]: %s - added to download queue\n"), curl->filename, curl->url);
			curl_multi_add_handle(multi_handle, curl->handle);
			curl->starttime = time(NULL);
			curl->fileinfo->status = FS_DOWNLOADING;
			lastfilenum = filenum;
			return true;
		}
	}
	curl_faileddownload = true;
	return false;
}

void CURL_DownloadFiles(void)
{
	int numfds;

    if (multi_handle)
    {
    	curl_multi_perform(multi_handle, &running_handles);

    	if (running_handles)
      	{
      		// wait for activity, timeout or "nothing" 
			curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
      	}
    }
}

static void cleanup_transfer(curlinfo_t *ti)
{
	curl_active_transfers--;
	curl_total_transfers--;
	curl_multi_remove_handle(multi_handle, ti->handle);
	curl_easy_cleanup(ti->handle);
	ti->handle = NULL;
}

void CURL_CheckDownloads(curlinfo_t *ti)
{
	CURLMsg *m; // for picking up messages with the transfer status
	long response_code = 0;
	const char *easy_handle_error;
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
				if (easyres == CURLE_HTTP_RETURNED_ERROR)
					curl_easy_getinfo(ti->handle, CURLINFO_RESPONSE_CODE, &response_code);

				easy_handle_error = (response_code) ? va("HTTP reponse code %ld", response_code) : curl_easy_strerror(easyres);
				ti->fileinfo->status = FS_FALLBACK;
				fclose(ti->fileinfo->file);
				remove(ti->fileinfo->filename);
				CONS_Printf(M_GetText("Failed to download %s (%s)\n"), ti->filename, easy_handle_error);
				curl_faileddownload = true;
			}
			else
			{
				CONS_Printf(M_GetText("Finished downloading %s\n"), ti->filename);
				downloadcompletednum++;
				ti->fileinfo->status = FS_FOUND;
				fclose(ti->fileinfo->file);
			}

			cleanup_transfer(ti);
			if (!curl_total_transfers)
				break;
		}
	}
}

HTTP_login *
CURLGetLogin (const char *url, HTTP_login ***return_prev_next)
{
	HTTP_login  * login;
	HTTP_login ** prev_next;

	for (
			prev_next = &curl_logins;
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
