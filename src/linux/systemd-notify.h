#ifndef __SYSTEMD_NOTIFY__
#define __SYSTEMD_NOTIFY__

#ifdef HAVE_SD_NOTIFY

void send_sd_status_ready(void);
void send_sd_status_stop(void);
void send_sd_status(const char *stformat, ...);

#endif

#endif // __SYSTEMD_NOTIFY__
