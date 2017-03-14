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

#define SMAP_URLE_NOMEM     1
#define SMAP_URLE_INVALID   2
#define SMAP_URLE_PORT      3
#define SMAP_URLE_NAME2LONG 4
#define SMAP_URLE_NOABS     5
#define SMAP_URLE_NOPORT    6
#define SMAP_URLE_BADPORT   7
#define SMAP_URLE_UNKPORT   8
#define SMAP_URLE_UNKHOST   9
#define SMAP_URLE_BADFAMILY 10
#define SMAP_URLE_BADPROTO  11

int smap_url_parse(const char *cstr, struct sockaddr **psa, socklen_t *plen);
const char *smap_url_strerror(int er);
