/* This file is part of Smap.
   Copyright (C) 2006-2007, 2010, 2014 Sergey Poznyakoff

   Smap is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Smap is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Smap.  If not, see <http://www.gnu.org/licenses/>. */

#include "smapd.h"

char *log_tag;
int log_facility = LOG_DAEMON;
int log_to_stderr = -1;
int smap_trace_option;

char **trace_patterns;
size_t trace_pattern_max;
size_t trace_pattern_count;

void
add_trace_pattern(const char *arg)
{
	if (trace_pattern_count == trace_pattern_max) {
		if (trace_pattern_max == 0) {
			trace_pattern_max = 4;
			trace_patterns = ecalloc(trace_pattern_max,
						 sizeof(trace_patterns[0]));
		} else {
			trace_pattern_max *= 2;
			trace_patterns = erealloc(trace_patterns,
						  trace_pattern_max *
						  sizeof(trace_patterns[0]));
		}
	}
	trace_patterns[trace_pattern_count++] = arg ? estrdup(arg) : NULL;
}



void
smap_log_init()
{
	if (!log_tag)
		log_tag = smap_progname;
	if (log_to_stderr)
		smap_openlog_stderr(smap_trace_option);
	else
		smap_openlog_syslog(log_tag, LOG_PID, log_facility,
				    smap_trace_option);
	if (smap_trace_option && trace_pattern_count) {
		add_trace_pattern(NULL);
		smap_stream_ioctl(smap_trace_str, SMAP_IOCTL_SET_ARGS,
				  trace_patterns);
	}
}

static struct smap_kwtab kw_facility[] = {
	{ "USER",    LOG_USER },
	{ "user",    LOG_USER },
	{ "DAEMON",  LOG_DAEMON },
	{ "daemon",  LOG_DAEMON },
	{ "AUTH",    LOG_AUTH },
	{ "auth",    LOG_AUTH },
	{ "AUTHPRIV",LOG_AUTHPRIV },
	{ "authpriv",LOG_AUTHPRIV },
	{ "MAIL",    LOG_MAIL },
	{ "mail",    LOG_MAIL },
	{ "CRON",    LOG_CRON },
	{ "cron",    LOG_CRON },
	{ "LOCAL0",  LOG_LOCAL0 },
	{ "local0",  LOG_LOCAL0 },
	{ "LOCAL1",  LOG_LOCAL1 },
	{ "local1",  LOG_LOCAL1 },
	{ "LOCAL2",  LOG_LOCAL2 },
	{ "local2",  LOG_LOCAL2 },
	{ "LOCAL3",  LOG_LOCAL3 },
	{ "local3",  LOG_LOCAL3 },
	{ "LOCAL4",  LOG_LOCAL4 },
	{ "local4",  LOG_LOCAL4 },
	{ "LOCAL5",  LOG_LOCAL5 },
	{ "local5",  LOG_LOCAL5 },
	{ "LOCAL6",  LOG_LOCAL6 },
	{ "local6",  LOG_LOCAL6 },
	{ "LOCAL7",  LOG_LOCAL7 },
	{ "local7",  LOG_LOCAL7 },
	{ NULL }
};

int
syslog_fname_to_n (const char *str, int *pint)
{
	return smap_kwtab_nametotok(kw_facility, str, pint);
}


