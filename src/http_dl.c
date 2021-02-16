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
#include "m_argv.h"
#include "m_misc.h"
#include "d_netfil.h"
#include "d_net.h"
#include "i_system.h"
#include "i_threads.h"
#include "http_dl.h"

CURLSH *curlshare;
UINT32 httpdl_active_transfers = 0;
UINT32 httpdl_total_transfers = 0;
boolean httpdl_wasinit = false;
boolean httpdl_faileddownload = false;
HTTP_login *httpdl_logins;
static I_mutex httpdl_mutex;

void terminateConnectionNow(CURL* curlHandle)
{
    curl_socket_t sockfd;
    curl_easy_getinfo(curlHandle, CURLINFO_ACTIVESOCKET, &sockfd);
    closesocket(sockfd);
}

static void lock_cb(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr)
{
  (void)access; /* unused */
  (void)userptr; /* unused */
  (void)handle; /* unused */
  (void)data; /* unused */
  I_lock_mutex(&httpdl_mutex);
}

static void unlock_cb(CURL *handle, curl_lock_data data, void *userptr)
{
  (void)userptr; /* unused */
  (void)handle;  /* unused */
  (void)data;    /* unused */
  I_unlock_mutex(&httpdl_mutex);
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

static int progress_cb(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	curlinfo_t ci;
	// Function prototype requires these but we won't use, so just discard
	(void)ultotal;
	(void)ulnow;
	ci = *(curlinfo_t *)clientp;
	fileneeded[ci.num].currentsize = (UINT32)dlnow;
	fileneeded[ci.num].totalsize = (UINT32)dltotal;
	//getbytes = dlnow / (time(NULL) - ci.starttime);
	return 0;
}

static void set_common_opts(curlinfo_t *ti)
{
	curl_easy_setopt(ti->share, CURLOPT_SHARE, curlshare);

	curl_easy_setopt(ti->handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(ti->handle, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(ti->handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(ti->handle, CURLOPT_PROGRESSFUNCTION, progress_cb);
	curl_easy_setopt(ti->handle, CURLOPT_PROGRESSDATA, ti);

	// Only allow HTTP and HTTPS
	curl_easy_setopt(ti->handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
	curl_easy_setopt(ti->handle, CURLOPT_USERAGENT, va("SRB2Kart/v%d.%d", VERSION, SUBVERSION)); // Set user agent as some servers won't accept invalid user agents.

	// Follow a redirect request, if sent by the server.
	curl_easy_setopt(ti->handle, CURLOPT_FOLLOWLOCATION, 1L);

	curl_easy_setopt(ti->handle, CURLOPT_FAILONERROR, 1L);

	// abort if slower than 5 bytes/sec during 10 seconds
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_TIME, 10L);
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_LIMIT, 5L);
}

static void cleanup_transfer(curlinfo_t *transfer)
{
	httpdl_active_transfers--;
	httpdl_total_transfers--;
	curl_easy_cleanup(transfer->handle);
	transfer->handle = NULL;
	transfer->active = false;
}

static void download_file_thread(curlinfo_t *transfer)
{
	CURLcode cc;

	if (I_thread_is_stopped())
		return;

	if (!transfer->handle)
		return;

	fileneeded[transfer->num].status = FS_DOWNLOADING;
	transfer->active = true;

	cc = curl_easy_perform(transfer->handle);

	if (cc != CURLE_OK)
	{
		CONS_Printf(M_GetText("Failed to download %s (%s)\n"), transfer->filename, curl_easy_strerror(cc));
		httpdl_faileddownload = true;
		fclose(transfer->file);
		remove(transfer->filename);
		transfer->file = NULL;
	}
	else
	{
		CONS_Printf(M_GetText("Finished downloading %s\n"), transfer->filename);
		downloadcompletednum++;
		fileneeded[transfer->num].status = FS_FOUND;
		fclose(transfer->file);
		transfer->file = NULL;
	}
	cleanup_transfer(transfer);
	return;
}

/*
 Initialize a new curl session,
 returns true if successful, false if not.
*/
boolean HTTPDL_Init(void)
{
	if (!httpdl_wasinit)
	{
		if (!curl_global_init(CURL_GLOBAL_ALL))
		{
			curlshare = curl_share_init();
			if (curlshare)
			{
				curl_share_setopt(curlshare, CURLSHOPT_LOCKFUNC, lock_cb);
	  			curl_share_setopt(curlshare, CURLSHOPT_UNLOCKFUNC, unlock_cb);
	  			curl_share_setopt(curlshare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
	  			httpdl_wasinit = true;
	  			CONS_Printf("http dl init\n");
	  			return true;
			}
		}
	}
	CONS_Printf("http dl init failed\n");
	return false;
}

// Cleans up the share handle and all of the resources used by this curl session.
void HTTPDL_Quit(void)
{
	if (!httpdl_wasinit)
		return;

	curl_share_cleanup(curlshare);
 	curl_global_cleanup();
 	httpdl_wasinit = false;
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


boolean HTTPDL_AddTransfer(curlinfo_t *download, const char* url, int filenum)
{
	static char transfername[64];
	HTTP_login *login;
#ifdef PARANOIA
	if (M_CheckParm("-nodownload"))
		I_Error("Attempted to download files in -nodownload mode");
#endif

	download->handle = curl_easy_init();

	set_common_opts(download);

	I_mkdir(downloaddir, 0755);

	strlcpy(download->filename, fileneeded[filenum].filename, sizeof(download->filename));
	snprintf(download->url, sizeof(download->url), "%s/%s", url, download->filename);

	curl_easy_setopt(download->handle, CURLOPT_URL, download->url);

	// Authenticate if the user so wishes
	login = HTTPDL_GetLogin(url, NULL);

	if (login)
	{
		curl_easy_setopt(download->handle, CURLOPT_USERPWD, login->auth);
	}

	strcatbf(download->filename, downloaddir, "/");
	download->file = fopen(download->filename, "wb");
	download->num = filenum;

	curl_easy_setopt(download->handle, CURLOPT_WRITEDATA, download->file);

	//download->starttime = time(NULL);
	lastfilenum = filenum;
	snprintf(transfername, sizeof(transfername), "file-%s", download->filename);
	I_spawn_thread(transfername, (I_thread_fn)download_file_thread, download);

	return true;
}