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
#include "srvman.h"
#ifdef WITH_LIBWRAP
# include <tcpd.h>
#endif

struct smap_server {
	struct smap_server *prev;    /* Link to the previous server */
	struct smap_server *next;    /* Link to the next server */
	char *id;                    /* Server ID */
	char *url;                   /* Socket URL */
	struct sockaddr *sa;         /* Socket address */
	socklen_t salen;             /* Length of the sa */
	uid_t uid;                   /* Owner UID */
	gid_t gid;                   /* Owner GID */
	mode_t mode;                 /* Socket mode */
	int backlog;                 /* Backlog value for listen(2) */
	int fd;                      /* Socket descriptor */
	int flags;                   /* SRV_* flags */
	smap_server_prefork_hook_t prefork_hook;  /* Pre-fork function */
	smap_server_func_t conn;     /* Connection handler */
	smap_srvman_hook_t free_hook;
	void *data;                 /* Server-specific data */
	size_t max_children;     /* Maximum number of sub-processes to run. */
	size_t num_children;     /* Current number of running sub-processes. */
	pid_t *pidtab;           /* Array of child PIDs */
	size_t pidtab_size;      /* Number of elements in pidtab */
};

#define SRVMAN_BACKLOG(srv) \
	((srv)->backlog ? (srv)->backlog : srvman_param.backlog)
#define SRVMAN_REUSEADDR(srv) \
	(srvman_param.reuseaddr || !(srv->flags & SRV_KEEP_EXISTING))


typedef RETSIGTYPE (*sig_handler_t) (int);

union srvman_sockaddr {
	struct sockaddr sa;
	struct sockaddr_in s_in;
	struct sockaddr_un s_un;
};

struct srvman {
	struct smap_server *head, *tail; /* List of servers */
	size_t num_children;        /* Current number of running
				       sub-processes. */
	sigset_t sigmask;           /* A set of signals to handle by the
				       manager.  */
	sig_handler_t sigtab[NSIG]; /* Keeps old signal handlers. */
};

struct srvman_param srvman_param;
static struct srvman srvman;

static sig_handler_t
set_signal(int sig, sig_handler_t handler)
{
#ifdef HAVE_SIGACTION
	struct sigaction act, oldact;
	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(sig, &act, &oldact);
	return oldact.sa_handler;
#else
	return signal(sig, handler);
#endif
}

static int volatile need_cleanup;
static int volatile stop;

void
smap_srvman_stop()
{
	stop = 1;
}

static RETSIGTYPE
srvman_signal(int signo)
{
	switch (signo) {
	case SIGCHLD:
		need_cleanup = 1;
		break;

	default:
		/* FIXME: */
		stop = 1;
		break;
	}
#ifndef HAVE_SIGACTION
	signal(signo, srvman_signal);
#endif
}

static void
set_signal_handlers()
{
	int i;

	for (i = 0; i < NSIG; i++)
		if (sigismember (&srvman.sigmask, i))
			srvman.sigtab[i] = set_signal(i, srvman_signal);
}

static void
restore_signal_handlers()
{
	int i;

	for (i = 0; i < NSIG; i++)
		if (sigismember (&srvman.sigmask, i))
			set_signal(i, srvman.sigtab[i]);
}


/* Yield 1 if SRV has run out of the children limit */
#define SERVER_BUSY(srv)			\
	((srv)->max_children && (srv)->num_children >= (srv)->max_children)

static void
register_child(struct smap_server *srv, pid_t pid)
{
	size_t i;

	debug(DBG_SRVMAN, 20, ("registering child %lu", (unsigned long)pid));
	for (i = 0; i < srv->pidtab_size; i++)
		if (srv->pidtab[i] == 0)
			break;
	if (i == srv->pidtab_size) {
		int j;
		if (srv->pidtab_size == 0)
			srv->pidtab_size = DEFAULT_PIDTAB_SIZE;
		else
			srv->pidtab_size *= 2;
		srv->pidtab = erealloc(srv->pidtab,
				       srv->pidtab_size *
					 sizeof(srv->pidtab[0]));
		for (j = i; j < srv->pidtab_size; j++)
			srv->pidtab[j] = 0;

	}
	srv->pidtab[i] = pid;
	srv->num_children++;
	srvman.num_children++;
}

static void
report_exit_status(const char *tag, pid_t pid, int status, int expect_term)
{
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0)
			debug(DBG_SRVMAN, 1,
			      ("%s (%lu) exited successfully",
			       tag, (unsigned long) pid));
		else
			smap_error(_("%s (%lu) failed with status %d"),
				   tag, (unsigned long) pid,
				   WEXITSTATUS(status));
	} else if (WIFSIGNALED (status)) {
		if (expect_term && WTERMSIG(status) == SIGTERM)
			debug(DBG_SRVMAN, 1,
			      ("%s (%lu) terminated on signal %d",
			       tag, (unsigned long) pid,
			       WTERMSIG(status)));
		else
			smap_error(_("%s (%lu) terminated on signal %d"),
				   tag, (unsigned long) pid,
				   WTERMSIG(status));
	} else if (WIFSTOPPED(status))
		smap_error(_("%s (%lu) stopped on signal %d"),
			   tag, (unsigned long) pid,
			   WSTOPSIG(status));
	else
		smap_error(_("%s (%lu) terminated with unrecognized status"),
			   tag, (unsigned long) pid);
}

/* Remove (unregister) PID from the list of running instances and
   log its exit STATUS.
   Return 1 to command main loop to recompute the set of active
   descriptors. */
static int
unregister_child(pid_t pid, int status)
{
	struct smap_server *srv;

	debug(DBG_SRVMAN, 20,
	      ("unregistering child %lu (status %#x)",
	       (unsigned long)pid, status));
	for (srv = srvman.head; srv; srv = srv->next) {
		size_t i;

		for (i = 0; i < srv->pidtab_size; i++)
			if (srv->pidtab[i] == pid) {
				int rc = SERVER_BUSY(srv);
				srv->pidtab[i] = 0;
				srv->num_children--;
				srvman.num_children--;
				/* FIXME: expect_term? */
				report_exit_status(srv->id, pid, status, 0);
				return rc;
			}
	}
	/* FIXME */
	return 0;
}

static int
children_cleanup()
{
	int rc = 0;
	pid_t pid;
	int status;

	debug(DBG_SRVMAN, 2, ("cleaning up subprocesses"));
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		rc |= unregister_child(pid, status);
	return rc;
}


static void
server_remove(struct smap_server *srv)
{
	struct smap_server *p;

	debug(DBG_SRVMAN, 10, ("removing server %s", srv->id));
	if ((p = srv->prev) != NULL)
		p->next = srv->next;
	else
		srvman.head = srv->next;

	if ((p = srv->next) != NULL)
		p->prev = srv->prev;
	else
		srvman.tail = srv->prev;
}

static void
server_signal_children(struct smap_server *srv, int sig)
{
	int i;

	debug(DBG_SRVMAN, 10,
	      ("server %s: sending children signal %d", srv->id, sig));
	for (i = 0; i < srv->pidtab_size; i++)
		if (srv->pidtab[i])
			kill(srv->pidtab[i], sig);
}

void
smap_server_shutdown(struct smap_server *srv)
{
	if (srv->fd == -1)
		return;

	debug(DBG_SRVMAN, 2, ("shutting down %s", srv->id));
	close(srv->fd);
	srv->fd = -1;
}

struct smap_server *
smap_server_new(const char *id, const char *url,
		smap_server_func_t conn, int flags)
{
	struct smap_server *srv;
	struct sockaddr *sa;
	socklen_t salen;
	int rc;

	rc = smap_url_parse(url, &sa, &salen);
	if (rc) {
		smap_error("cannot parse url `%s': %s",
			   url, smap_url_strerror(rc));
		return NULL;
	}
	srv = ecalloc(1, sizeof(*srv));
	srv->id = estrdup(id);
	srv->url = estrdup(url);
	srv->sa = sa;
	srv->salen = salen;
	srv->backlog = 0;
	srv->conn = conn;
	srv->flags = flags;
	srv->uid = (uid_t)-1;
	srv->gid = (gid_t)-1;
	srv->mode = (mode_t)-1;
	return srv;
}

void
smap_server_free(struct smap_server *srv)
{
	server_remove(srv);
	if (srv->free_hook)
		srv->free_hook(srv->data);
	free(srv->id);
	free(srv->sa);
	free(srv->pidtab);
	free(srv);
}

void
smap_server_set_prefork_hook(struct smap_server *srv,
			     smap_server_prefork_hook_t hook)
{
	srv->prefork_hook = hook;
}

void
smap_server_set_data(struct smap_server *srv,
		     void *data,
		     smap_srvman_hook_t free_hook)
{
	srv->data = data;
	srv->free_hook = free_hook;
}

void *
smap_server_get_data_ptr(struct smap_server *srv)
{
	return srv->data;
}

void
smap_server_set_max_children(struct smap_server *srv, size_t n)
{
	srv->max_children = n;
}

void
smap_server_set_backlog(struct smap_server *srv, int n)
{
	srv->backlog = n;
}

void
smap_server_set_flags(struct smap_server *srv, int bit, enum srvman_bitop op)
{
	switch (op) {
	case srvman_bitop_asg:
		srv->flags = bit;
		break;

	case srvman_bitop_set:
		srv->flags |= bit;
		break;

	case srvman_bitop_clr:
		srv->flags &= ~bit;
	}
}

void
smap_server_set_mode(struct smap_server *srv, mode_t mode)
{
	srv->mode = mode;
}

void
smap_server_set_owner(struct smap_server *srv, uid_t uid, gid_t gid)
{
	if (uid != (uid_t)-1)
		srv->uid = uid;
	if (gid != (gid_t)-1)
		srv->gid = gid;
}

void
smap_server_get_owner(struct smap_server *srv, uid_t *uid, gid_t *gid)
{
	if (uid)
		*uid = srv->uid;
	if (gid)
		*gid = srv->gid;
}


void
smap_srvman_attach_server(struct smap_server *srv)
{
	srv->next = NULL;
	srv->prev = srvman.tail;
	if (srvman.tail)
		srvman.tail->next = srv;
	else
		srvman.head = srv;
	srvman.tail = srv;
}

struct smap_server *
smap_srvman_find_server(const char *id)
{
	struct smap_server *srv;

	for (srv = srvman.head; srv; srv = srv->next)
		if (strcmp(srv->id, id) == 0)
			break;
	return srv;
}

int
smap_srvman_get_sockaddr(const char *id, struct sockaddr const **psa,
			 int *plen)
{
	struct smap_server *srv = smap_srvman_find_server(id);
	if (!srv)
		return 1;
	*psa = srv->sa;
	*plen = srv->salen;
	return 0;
}


static int
check_acl(char *id, int fd)
{
#ifdef WITH_LIBWRAP
	int rc;
	struct request_info req;

	request_init(&req, RQ_DAEMON, id, RQ_FILE, fd, NULL);
	fromhost(&req);
	rc = hosts_access(&req);
	if (!rc)
		smap_error("connect from %s denied by tcpwrapper rule %s",
			   eval_client(&req), id);
	else
		debug(DBG_SRVMAN, 1, ("connection from %s allowed by %s",
				      eval_client(&req), id));
	return rc;
#else
	return 1;
#endif
}

static void
server_run(int connfd, struct smap_server *srv,
	   struct sockaddr *sa, socklen_t salen)
{
	if (sa->sa_family == AF_INET
	    && (!check_acl(srv->id, connfd)
		|| !check_acl(smap_progname, connfd)))
		return;

	if (srvman_param.single_process
	    || (srv->flags & SRV_SINGLE_PROCESS)) {
		if ((!srvman_param.prefork_hook
		     || srvman_param.prefork_hook(sa, salen,
						  srvman_param.data) == 0)
		    && (!srv->prefork_hook
			|| srv->prefork_hook(srv->id,
					     sa, salen,
					     srv->data,
					     srvman_param.data) == 0))
			srv->conn(srv->id,
				  connfd, sa, salen,
				  srv->data,
				  srvman_param.data);
	} else {
		pid_t pid;

		if (srv->prefork_hook
		    && srv->prefork_hook(srv->id,
					 sa, salen,
					 srv->data, srvman_param.data))
			return;

		pid = fork();
		if (pid == -1)
			smap_error("fork: %s", strerror(errno));
		else if (pid == 0) {
			/* Child.  */
			fd_set fdset;

			FD_ZERO(&fdset);
			FD_SET(connfd, &fdset);
			if (log_to_stderr) {
				FD_SET(1, &fdset);
				FD_SET(2, &fdset);
			}
			close_fds_except(&fdset);
			restore_signal_handlers();
			exit(srv->conn(srv->id,
				       connfd,
				       sa, salen,
				       srv->data, srvman_param.data));
		} else
			register_child(srv, pid);
	}
}

/* Accept incoming connection for server SRV.
   Return 1 to command main loop to recompute the set of active
   descriptors. */
static int
server_accept(struct smap_server *srv)
{
	int connfd;
	union srvman_sockaddr client;
	socklen_t size = sizeof(client);

	if (srv->fd == -1) {
		smap_error(_("removing shut down server %s"),
			   srv->id);
		smap_server_free(srv);
		return 1;
	}

	if (SERVER_BUSY(srv)) {
		smap_error(_("server %s: too many children (%lu)"),
			   srv->id, (unsigned long) srv->num_children);
		return 1;
	}

	connfd = accept(srv->fd, &client.sa, &size);
	if (connfd == -1) {
		if (errno != EINTR) {
			smap_error(_("server %s: accept failed: %s"),
				   srv->id, strerror(errno));
			smap_server_shutdown(srv);
			smap_server_free(srv);
			return 1;
		} /* FIXME: Call srv->intr otherwise? */
	} else {
		server_run(connfd, srv, &client.sa, size);
		close(connfd);
	}
	return 0;
}

static int
connection_loop(fd_set *fdset)
{
	struct smap_server *srv;
	int rc = 0;
	for (srv = srvman.head; srv; ) {
		struct smap_server *next = srv->next;
		if (FD_ISSET(srv->fd, fdset))
			rc |= server_accept(srv);
		srv = next;
	}
	return rc;
}

int
compute_fdset(fd_set *fdset)
{
	struct smap_server *p;
	int maxfd = 0;
	FD_ZERO(fdset);
	for (p = srvman.head; p; p = p->next) {
		if (SERVER_BUSY(p))
			continue;
		FD_SET(p->fd, fdset);
		if (p->fd > maxfd)
			maxfd = p->fd;
	}
	debug(DBG_SRVMAN, 10, ("recomputed fdset: %d fds", maxfd));
	return maxfd;
}

void
smap_srvman_run(sigset_t *set)
{
	int recompute_fd = 1;
	int maxfd;
	fd_set fdset;

	if (!srvman.head)
		return;

	debug(DBG_SRVMAN, 2, ("server manager starting"));
	if (set)
		srvman.sigmask = *set;
	else
		sigemptyset(&srvman.sigmask);
	sigaddset(&srvman.sigmask, SIGCHLD);
	set_signal_handlers();
	if (srvman_param.shutdown_timeout == 0)
		srvman_param.shutdown_timeout = DEFAULT_SHUTDOWN_TIMEOUT;
	if (srvman_param.backlog == 0)
		srvman_param.backlog = DEFAULT_BACKLOG;

	for (stop = 0; srvman.head && !stop;) {
		int rc;
		struct timeval *to;
		fd_set rdset;

		if (need_cleanup) {
			need_cleanup = 0;
			recompute_fd = children_cleanup();
		}

		if (recompute_fd) {
			maxfd = compute_fdset(&fdset);
			recompute_fd = 0;
		}

		if (!maxfd) {
			debug(DBG_SRVMAN, 2, ("no active fds, pausing"));
			pause();
			recompute_fd = 1;
			continue;
		}

		if (srvman_param.max_children
		    && srvman.num_children >= srvman_param.max_children) {
			smap_error(_("too many children (%lu)"),
				   (unsigned long) srvman.num_children);
			pause();
			continue;
		}

		if (srvman_param.idle_hook
		    && srvman_param.idle_hook(srvman_param.data)) {
			debug(DBG_SRVMAN, 2, ("break requested by idle hook"));
			break;
		}

		rdset = fdset;
		to = NULL; /* FIXME */
		rc = select(maxfd + 1, &rdset, NULL, NULL, to);
		if (rc == -1 && errno == EINTR)
			continue;
		if (rc < 0) {
			smap_error(_("select failed: %s"), strerror(errno));
			break;
		}
		recompute_fd = connection_loop(&rdset);
	}

	restore_signal_handlers();
	debug(DBG_SRVMAN, 2, ("server manager finishing"));
}

static int
server_prep(struct smap_server *srv, int fd)
{
	struct stat st;
	struct sockaddr_un *s_un;
	int t, rc;
	mode_t old_mask;

	switch (srv->sa->sa_family) {
	case AF_UNIX:
		s_un = (struct sockaddr_un *) srv->sa;

		if (stat(s_un->sun_path, &st)) {
			if (errno != ENOENT) {
				smap_error(_("%s: file %s exists but cannot be stat'd: %s"),
					   srv->id,
					   s_un->sun_path,
					   strerror(errno));
				return 1;
			}
		} else if (!S_ISSOCK(st.st_mode)) {
			smap_error(_("%s: file %s is not a socket"),
				   srv->id, s_un->sun_path);
			return 1;
		} else if (SRVMAN_REUSEADDR(srv)) {
			if (unlink(s_un->sun_path)) {
				smap_error(_("%s: cannot unlink file %s: %s"),
					   srv->id, s_un->sun_path,
					   strerror(errno));
				return 1;
			}
		} else {
			smap_error(_("socket `%s' already exists"),
				   s_un->sun_path);
			return 1;
		}
		break;

	case AF_INET:
		if (SRVMAN_REUSEADDR(srv)) {
			t = 1;
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
				   &t, sizeof(t));
		}
	}

	
	old_mask = umask(0777 & ~((srv->mode == (mode_t)-1) ?
				   srvman_param.socket_mode : srv->mode));
	rc = bind(fd, srv->sa, srv->salen);
	umask(old_mask);

	if (rc == -1) {
		smap_error(_("%s: cannot bind to %s: %s"),
			   srv->id, srv->url, strerror(errno));
		return 1;
	}

	if (srv->sa->sa_family == AF_UNIX
	    && (srv->uid != (uid_t)-1 || srv->gid != (gid_t)-1)) {
		if (geteuid())
			debug(DBG_SRVMAN, 1,
			      ("ignoring ownership directives for %s: "
			       "not a superuser",
			       s_un->sun_path));
		else if (chown(s_un->sun_path, srv->uid, srv->gid))
				smap_error(_("cannot set ownership of %s: %s"),
					   s_un->sun_path, strerror(errno));
	}
	
	if (listen(fd, SRVMAN_BACKLOG(srv)) == -1) {
		smap_error(_("%s: listen on %s failed: %s"),
			   srv->id, srv->url, strerror(errno));
		return 1;
	}
	return 0;
}

static int
server_open(struct smap_server *srv)
{
	int fd = socket(srv->sa->sa_family, SOCK_STREAM, 0);
	if (fd == -1) {
		smap_error("%s: socket: %s", srv->id, strerror(errno));
		return 1;
	}

	if (server_prep(srv, fd)) {
		close(fd);
		return 1;
	}
	srv->fd = fd;
	return 0;
}


int
smap_srvman_open()
{
	struct smap_server *p;

	debug(DBG_SRVMAN, 2, ("opening servers"));
	for (p = srvman.head; p; ) {
		struct smap_server *next = p->next;
		if (server_open(p))
			smap_server_free(p);
		p = next;
	}
	return srvman.head == NULL;
}

void
smap_srvman_shutdown()
{
	struct smap_server *p;
	time_t start = time(NULL);

	for (p = srvman.head; p; p = p->next)
		server_signal_children(p, SIGTERM);

	do {
		children_cleanup();
		if (srvman.num_children == 0)
			break;
		sleep(1);
	} while (time(NULL) - start < srvman_param.shutdown_timeout);

	debug(DBG_SRVMAN, 2, ("shutting down servers"));
	for (p = srvman.head; p; p = p->next) {
		server_signal_children(p, SIGKILL);
		smap_server_shutdown(p);
	}
}

void
smap_srvman_free()
{
	struct smap_server *p;

	for (p = srvman.head; p; ) {
		struct smap_server *next = p->next;
		smap_server_free(p);
		p = next;
	}
	if (srvman_param.free_hook)
		srvman_param.free_hook(srvman_param.data);
}

size_t
smap_srvman_count_servers()
{
	size_t count = 0;
	struct smap_server *p;

	for (p = srvman.head; p; p = p->next)
		count++;
	return count;
}

void
smap_srvman_iterate_data(int (*fun)(smap_server_t, void *))
{
	struct smap_server *p;
	for (p = srvman.head; p;) {
		struct smap_server *next = p->next;
		if (fun(p, p->data))
			smap_server_free(p);
		p = next;
	}
}
