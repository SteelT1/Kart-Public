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

CURLSH *curlshare; // Handle for sharing a single connection pool
UINT32 httpdl_active_transfers = 0; // Number of currently ongoing transfers
UINT32 httpdl_total_transfers = 0; // Number of total tranfeers
boolean httpdl_wasinit = false;
boolean httpdl_faileddownload = false; // Did a download fail?
static I_mutex httpdl_mutex;

static void ChangeFileExtension(char* filename, char* newExtension)
{
	static char* lastSlash;
	lastSlash = strstr(filename, ".");
    strlcpy(lastSlash, newExtension, strlen(lastSlash));
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
	ci.fileinfo->currentsize = (UINT32)dlnow;
	ci.fileinfo->totalsize = (UINT32)dltotal;
	//getbytes = dlnow / (time(NULL) - ci.starttime);
	return 0;
}

static void set_common_opts(curlinfo_t *ti)
{
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

	// abort if slower than 30 bytes/sec during 60 seconds
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_TIME, 60L);
	curl_easy_setopt(ti->handle, CURLOPT_LOW_SPEED_LIMIT, 30L);
	curl_easy_setopt(ti->share, CURLOPT_SHARE, curlshare);
}

static void cleanup_transfer(curlinfo_t *ti)
{
	httpdl_active_transfers--;
	httpdl_total_transfers--;
	curl_easy_cleanup(ti->handle);
}

static void download_file_thread(curlinfo_t *transfer)
{
	CURLcode cc;
	char errbuf[256];

	curl_easy_setopt(transfer->handle, CURLOPT_ERRORBUFFER, errbuf);
	cc = curl_easy_perform(transfer->handle);

	if (cc != CURLE_OK)
	{
		transfer->fileinfo->status = FS_FALLBACK;
		fclose(transfer->fileinfo->file);
		remove(transfer->fileinfo->filename);
		CONS_Printf(M_GetText("Failed to download %s (%s)\n"), transfer->filename, errbuf);
		httpdl_faileddownload = true;
	}
	else
	{
		CONS_Printf(M_GetText("Thread %d: finished downloading %s\n"), transfer->id, transfer->filename);
		downloadcompletednum++;
		transfer->fileinfo->status = FS_FOUND;
		fclose(transfer->fileinfo->file);
	}
	cleanup_transfer(transfer);
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

boolean HTTPDL_AddTransfer(curlinfo_t *download, const char* url, int filenum)
{
	static char transfername[64];
#ifdef PARANOIA
	if (M_CheckParm("-nodownload"))
		I_Error("Attempted to download files in -nodownload mode");
#endif

	download->handle = curl_easy_init();

	set_common_opts(download);
	I_mkdir(downloaddir, 0755);

	strlcpy(download->filename, download->fileinfo->filename, sizeof(download->filename));
	snprintf(download->url, sizeof(download->url), "%s/%s", url, download->filename);

	curl_easy_setopt(download->handle, CURLOPT_URL, download->url);
	curl_easy_setopt(download->handle, CURLOPT_SHARE, download->share);

	strcatbf(download->fileinfo->filename, downloaddir, "/");
	download->fileinfo->file = fopen(download->fileinfo->filename, "wb");

	curl_easy_setopt(download->handle, CURLOPT_WRITEDATA, download->fileinfo->file);

	//download->starttime = time(NULL);
	download->fileinfo->status = FS_DOWNLOADING;
	lastfilenum = filenum;
	snprintf(transfername, sizeof(transfername), "file-%s", download->filename);
	I_spawn_thread(transfername, (I_thread_fn)download_file_thread, download);
	return true;
}
