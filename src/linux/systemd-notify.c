#ifdef HAVE_SD_NOTIFY

#define _GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <systemd/sd-daemon.h>  // sd_notify
#include "systemd-notify.h"

void send_sd_status_ready(void)
{
	sd_notify(0, "READY=1\n");
}
	
void send_sd_status_stop(void)
{
	sd_notify(0, "STOPPING=1");
}

void send_sd_status(const char *stformat, ...)
{
	char *str;
	int res = 0;
	va_list args;
	va_start(args, stformat);
	res = vasprintf(&str, stformat, args);
	if (res != -1)
		sd_pid_notify(getpid(), 0, str);
	va_end(args);
	if (str)
		free(str);
	str = NULL;
}

#endif // HAVE_SD_NOTIFY
