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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <syslog.h>
#include "smap/diag.h"

void
smap_openlog_syslog(char *tag, int opts, int facility, int trace)
{
	smap_stream_destroy(&smap_error_str);
	smap_stream_destroy(&smap_debug_str);
	smap_stream_destroy(&smap_trace_str);

	openlog(tag, opts, facility);
	smap_syslog_stream_create(&smap_error_str, LOG_ERR, NULL);
	smap_syslog_stream_create(&smap_debug_str, LOG_DEBUG, NULL);
	if (trace) {
		smap_stream_t s;
		if (smap_syslog_stream_create(&s, LOG_INFO, NULL))
			return;
		smap_trace_stream_create(&smap_trace_str, s);
	}
}
