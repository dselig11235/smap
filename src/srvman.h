/* This file is part of Smap.
   Copyright (C) 2005-2010, 2014 Sergey Poznyakoff

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

#ifndef __SRVMAN_H
#define __SRVMAN_H

typedef struct smap_server *smap_server_t;

typedef	int (*smap_server_func_t) (const char *id,
				  int fd,
				  struct sockaddr const *sa, socklen_t len,
				  void *server_data, void *srvman_data);
typedef	int (*smap_server_prefork_hook_t) (const char *id,
					  struct sockaddr const *sa,
					  socklen_t len,
					  void *server_data,
					  void *srvman_data);
typedef	int (*smap_srvman_hook_t) (void *data);
typedef int (*smap_srvman_prefork_hook_t) (struct sockaddr const *sa,
					  socklen_t len,
					  void *data);

#define DEFAULT_PIDTAB_SIZE 64
#define DEFAULT_SHUTDOWN_TIMEOUT 5
#define DEFAULT_BACKLOG 8

#define SRV_SINGLE_PROCESS 0x01
#define SRV_KEEP_EXISTING  0x02

struct srvman_param {
	void *data;                 /* Server manager data */
	int single_process;
	int reuseaddr;
	unsigned shutdown_timeout;
	smap_srvman_hook_t idle_hook;             /* Idle function */
	smap_srvman_prefork_hook_t prefork_hook;  /* Pre-fork function */
	smap_srvman_hook_t free_hook;             /* Free function */
	size_t max_children;        /* Maximum number of sub-processes
				       to run. */
	int backlog;
	mode_t socket_mode;
};

enum srvman_bitop {
	srvman_bitop_asg,
	srvman_bitop_set,
	srvman_bitop_clr
};

extern struct srvman_param srvman_param;

smap_server_t smap_srvman_find_server(const char *id);

void smap_server_shutdown(smap_server_t srv);
smap_server_t smap_server_new(const char *id, const char *url,
			      smap_server_func_t conn, int flags);
void smap_server_free(smap_server_t srv);
void smap_server_set_prefork_hook(smap_server_t srv,
				 smap_server_prefork_hook_t hook);
void *smap_server_get_data_ptr(struct smap_server *srv);
void smap_server_set_data(smap_server_t srv,
			 void *data,
			 smap_srvman_hook_t free_hook);
void smap_server_set_max_children(smap_server_t srv, size_t n);
void smap_server_set_backlog(struct smap_server *srv, int n);
void smap_server_set_flags(struct smap_server *srv, int bit,
			   enum srvman_bitop op);
void smap_server_set_owner(struct smap_server *srv, uid_t uid, gid_t gid);
void smap_server_get_owner(struct smap_server *srv, uid_t *uid, gid_t *gid);
void smap_server_set_mode(struct smap_server *srv, mode_t mode);

int smap_srvman_get_sockaddr(const char *id, struct sockaddr const **psa,
			     int *plen);

void smap_srvman_attach_server(smap_server_t srv);
void smap_srvman_run(sigset_t *);
int smap_srvman_open(void);
void smap_srvman_shutdown(void);
void smap_srvman_free(void);
size_t smap_srvman_count_servers(void);
void smap_srvman_stop(void);

void smap_srvman_iterate_data(int (*fun)(struct smap_server *, void *));

#endif
