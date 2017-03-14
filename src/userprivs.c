/* This file is part of Smap.
   Copyright (C) 2007-2008, 2010, 2014 Sergey Poznyakoff

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
#include "smapd.h"

void
privinfo_set_user(struct privinfo *pi, const char *name)
{
	struct passwd *pw = getpwnam(name);
	if (!pw) {
		smap_error("%s: no such user", name);
		return;
	}
	pi->uid = pw->pw_uid;
	privinfo_add_grpgid(pi, pw->pw_gid);
}

void
privinfo_add_grpgid(struct privinfo *pi, gid_t gid)
{
	if (pi->gc == 0) {
		pi->gc = 16;
		pi->gids = ecalloc(pi->gc, sizeof(pi->gids[0]));
		pi->gi = 0;
	}
	if (pi->gi == pi->gc) {
		pi->gc *= 2;
		pi->gids = erealloc(pi->gids, pi->gc * sizeof(pi->gids[0]));
	}
	pi->gids[pi->gi++] = gid;
}

void
privinfo_add_grpnam(struct privinfo *pi, const char *name)
{
	struct group *gr = getgrnam(name);
	if (!gr) {
		smap_error("%s: no such group", name);
		return;
	}
	privinfo_add_grpgid(pi, gr->gr_gid);
}

void
get_user_groups(struct privinfo *pi)
{
	struct group *gr;
	struct passwd *pw;
	const char *user;

	pw = getpwuid(pi->uid);
	if (!pw)
		return;
	user = pw->pw_name;

	setgrent();
	while (gr = getgrent()) {
		char **p;
		for (p = gr->gr_mem; *p; p++)
			if (strcmp(*p, user) == 0) {
				privinfo_add_grpgid(pi, gr->gr_gid);
				break;
			}
	}
	endgrent();
}

void
privinfo_expand_user_groups(struct privinfo *pi)
{
	if (pi->allgroups) {
		get_user_groups(pi);
		pi->allgroups = 0;
	}
}

/* Switch to the given UID/GID */
int
switch_to_privs(struct privinfo *pi)
{
	int rc = 0;
	uid_t uid = pi->uid;
	gid_t gid;

	if (uid == 0)
		return 0;

	/* Reset group permissions */
	if (geteuid() == 0 && setgroups(pi->gi, pi->gids)) {
		smap_error("setgroups failed: %s", strerror(errno));
		return 1;
	}

	/* Switch to the user's gid. On some OSes the effective gid must
	   be reset first */

	gid = pi->gids[0];
#if defined(HAVE_SETEGID)
	if ((rc = setegid(gid)) < 0)
		smap_error("setegid(%lu) failed: %s",
			   (unsigned long) gid, strerror(errno));
#elif defined(HAVE_SETREGID)
	if ((rc = setregid(gid, gid)) < 0)
		smap_error("setregid(%lu,%lu) failed: %s",
			   (unsigned long) gid, (unsigned long) gid,
			   strerror(errno));
#elif defined(HAVE_SETRESGID)
	if ((rc = setresgid(gid, gid, gid)) < 0)
		smap_error("setresgid(%lu,%lu,%lu) failed: %s",
			   (unsigned long) gid,
			   (unsigned long) gid,
			   (unsigned long) gid,
			   strerror(errno));
#endif

	if (rc == 0 && gid != 0) {
		if ((rc = setgid(gid)) < 0 && getegid() != gid)
			smap_error("setgid(%lu) failed: %s",
				   (unsigned long) gid, strerror(errno));
		if (rc == 0 && getegid() != gid) {
			smap_error("cannot set effective gid to %lu",
				   (unsigned long) gid);
			rc = 1;
		}
	}

	/* Now reset uid */
	if (rc == 0 && uid != 0) {
		uid_t euid;

		if (setuid(uid)
		    || geteuid() != uid
		    || (getuid() != uid
			&& (geteuid() == 0 || getuid() == 0))) {

#if defined(HAVE_SETREUID)
			if (geteuid() != uid) {
				if (setreuid(uid, -1) < 0) {
					smap_error("setreuid(%lu,-1) failed: %s",
						   (unsigned long) uid,
						   strerror(errno));
					rc = 1;
				}
				if (setuid(uid) < 0) {
					smap_error("second setuid(%lu) failed: %s",
						   (unsigned long) uid,
						   strerror(errno));
					rc = 1;
				}
			} else
#endif
				{
					smap_error("setuid(%lu) failed: %s",
						   (unsigned long) uid,
						   strerror(errno));
					rc = 1;
				}
		}

		euid = geteuid();
		if (uid != 0 && setuid(0) == 0) {
			smap_error("seteuid(0) succeeded when it should not");
			rc = 1;
		} else if (uid != euid && setuid(euid) == 0) {
			smap_error("cannot drop non-root setuid privileges");
			rc = 1;
		}
	}
	return rc;
}
