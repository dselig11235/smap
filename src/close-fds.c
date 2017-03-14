/* This file is part of Smap.
   Copyright (C) 2006-2007, 2010, 2013-2014 Sergey Poznyakoff

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

void
close_fds_above(int fd)
{
	int i;

	for (i = getmaxfd() - 1; i > fd; i--)
		close(i);
}

void
close_fds_except(fd_set *exfd)
{
	int i;

	for (i = FD_SETSIZE - 1; i >= 0; i--)
		if (!FD_ISSET(i, exfd))
			close(i);
}
