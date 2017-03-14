# This file is part of Smap.  -*- autoconf -*-
# Copyright (C) 2010, 2014, 2015 Sergey Poznyakoff
#
# Smap is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# Smap is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Smap.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ(2.63)
m4_define([SMAP_VERSION_MAJOR], 2)
m4_define([SMAP_VERSION_MINOR], 0)
dnl m4_define([SMAP_VERSION_PATCH], 90)
AC_INIT([smap],
	SMAP_VERSION_MAJOR.SMAP_VERSION_MINOR[]m4_ifdef([SMAP_VERSION_PATCH],.SMAP_VERSION_PATCH),
	[gray+smap@gnu.org.ua])
AC_CONFIG_SRCDIR([src/smapd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 gnits tar-ustar std-options silent-rules subdir-objects])

# Enable silent rules by default:
AM_SILENT_RULES([yes])

dnl Global subst variables
AC_SUBST(SMAP_MODDIR,'$(libdir)/$(PACKAGE)')

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S

dnl Checks for libraries.
LT_PREREQ(2.2.5a)
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT([recursive])
LT_WITH_LTDL
AC_CONFIG_FILES([libltdl/Makefile])

AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, gethostbyaddr)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([getopt.h sysexits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([getopt_long sysconf getdtablesize \
		setegid setregid setresgid setreuid])

AC_ARG_WITH([tcp-wrappers],
	AC_HELP_STRING([--with-tcp-wrappers],
		       [compile with TCP wrappers (libwrap) support]),
	[status_tcpwrap=${withval}],
	[status_tcpwrap=maybe])

case $status_tcpwrap in
 yes) AC_CHECK_LIB(wrap, main,,
		   [AC_MSG_ERROR([Required library libwrap not found])])
      AC_CHECK_LIB(nsl, main,
		   [TCPWRAP_LIBRARIES=-lnsl])
      AC_CHECK_HEADERS(tcpd.h,,
		       [AC_MSG_ERROR([Required header tcpd.h not found])])
      status_tcpwrap=yes
      ;;

 maybe)
      AC_CHECK_LIB(wrap, main,
		   [status_tcpwrap=yes],
		   [status_tcpwrap=no])
      AC_CHECK_LIB(nsl, main, [TCPWRAP_LIBRARIES=-lnsl])
      AC_CHECK_HEADERS(tcpd.h,
		       [status_tcpwrap=yes],
		       [status_tcpwrap=no])
      ;;
 no)  ;;
esac

if test "$status_tcpwrap" = "yes"; then
  AC_SUBST([TCPWRAP_LIBRARIES], "$TCPWRAP_LIBRARIES -lwrap")
  AC_DEFINE_UNQUOTED(WITH_LIBWRAP, 1,
		     [Define to 1 to use tcp wrappers.])
fi

AC_ARG_WITH([mailutils],
	      AC_HELP_STRING([--with-mailutils],
			     [use GNU Mailutils]),
	      [status_mailutils=${withval}],
	      [status_mailutils=maybe])

if test $status_mailutils != no; then
  AM_GNU_MAILUTILS(2.99.98, [all cfg auth],
		   [status_mailutils=yes],
		   [if test $status_mailutils = yes; then
			AC_MSG_ERROR([GNU Mailutils not found])
		    fi
		    status_mailutils=no])
fi

AM_CONDITIONAL([MAILUTILS_COND],[test $status_mailutils = yes])

# Guile
GINT_INIT([gint],[1.8 with-guile nodoc],
	  [status_guile=yes],
	  [status_guile=no])
AM_CONDITIONAL([GUILE_COND],[test $status_guile = yes])

# Mysql
AC_ARG_WITH(mysql,
 	    AC_HELP_STRING([--with-mysql],
	                   [use MySQL]),
            [status_mysql=${withval}],
	    [status_mysql=maybe])

AC_SUBST(MYSQL_LIBS)
if test $status_mysql != no; then
  MU_CHECK_LIB(mysqlclient, 
	       mysql_real_connect, 
	       [-lm],
               [ status_mysql=yes
	         MYSQL_LIBS=$mu_cv_lib_mysqlclient ],
	       [ if test $status_mysql = yes; then
	           AC_MSG_FAILURE([the requested library libmysqlclient is not found or is unusable])
                 fi
		 status_mysql=no ],
               [/usr/local/lib/mysql /usr/lib/mysql])
fi
AM_CONDITIONAL([MYSQL_COND],[test $status_mysql = yes])

# Postgres
AC_ARG_WITH([postgres],
	      AC_HELP_STRING([--with-postgres],
			     [build postgres module]),
	      [status_postgres=${withval}],
	      [status_postgres=maybe])

AC_SUBST(POSTGRES_LIBS)
if test $status_postgres != no; then
  MU_CHECK_LIB(pq, PQconnectStart, [],
	       [ POSTGRES_LIBS=$mu_cv_lib_pq
                 status_postgres=yes ],
	       [ if test $status_postgres = yes; then
	           AC_MSG_FAILURE([the requested library libpq is not found or is unusable])
		 fi
		 status_postgres=no ],
               [/usr/local/pgsql/lib /usr/pgsql/lib])
fi
AM_CONDITIONAL([POSTGRES_COND],[test $status_postgres = yes])

# LDAP
AC_ARG_WITH([ldap],
            AC_HELP_STRING([--with-ldap],
	                   [build LDAP module]),
            [status_ldap=${withval}],
            [status_ldap=maybe])
								       
if test $status_ldap != no; then
  AC_CHECK_LIB(ldap, ldap_bind,
               [status_ldap=yes],
    	       [if test $status_ldap = yes; then
	          AC_MSG_ERROR([the required library libldap is not found])
                else
                  status_ldap=no
                fi])
fi
AM_CONDITIONAL([LDAP_COND],[test $status_ldap = yes])

# Readline
AC_ARG_WITH([readline],
	    AC_HELP_STRING([--with-readline],
			   [use readline]),
	    [status_readline=${withval}],
	    [status_readline=maybe])

# Test for GNU Readline
AC_SUBST(READLINE_LIBS)

if test "$status_readline" != "no"; then

  dnl FIXME This should only link in the curses libraries if it's
  dnl really needed!

  dnl Check for Curses libs.
  CURSES_LIBS=
  for lib in ncurses curses termcap
  do
    AC_CHECK_LIB($lib, tputs, [CURSES_LIBS="-l$lib"; break])
  done

  saved_LIBS=$LIBS
  LIBS="$LIBS $CURSES_LIBS"
  AC_CHECK_LIB(readline, readline, smap_have_readline=yes)
  LIBS=$saved_LIBS

  if test "$smap_have_readline" = "yes"; then
    AC_CHECK_HEADERS(readline/readline.h,
		     AC_DEFINE(WITH_READLINE,1,[Enable use of readline]))
    READLINE_LIBS="-lreadline $CURSES_LIBS"
    saved_LIBS=$LIBS
    LIBS="$LIBS $READLINE_LIBS"
    AC_CHECK_FUNCS(rl_completion_matches)
    LIBS=$saved_LIBS
    status_readline="yes"
  else
    if test "$status_readline" = "yes"; then
      AC_MSG_WARN(GNU readline requested but does not seem to be installed)
    fi
    status_readline="no"
  fi
fi


AC_CONFIG_COMMANDS([status],[
cat <<EOF

*******************************************************************
Smap configured with the following settings:

TCP wrappers ............................. $status_tcpwrap
Mailutils ................................ $status_mailutils
Guile .................................... $status_guile
MySQL .................................... $status_mysql
Postgres ................................. $status_postgres
LDAP ..................................... $status_ldap
Readline ................................. $status_readline
*******************************************************************

EOF
],[
status_tcpwrap=$status_tcpwrap
if test $status_mailutils = yes; then
  status_mailutils="version $MAILUTILS_VERSION"
else
  status_mailutils=$status_mailutils
fi
status_guile=$status_guile
status_mysql=$status_mysql
status_postgres=$status_postgres
status_ldap=$status_ldap
status_readline=$status_readline
])

AC_CONFIG_FILES([Makefile
		 include/Makefile
		 include/smap/Makefile
		 lib/Makefile
		 src/Makefile
		 gint/Makefile
		 modules/Makefile
		 modules/echo/Makefile
		 modules/sed/Makefile
		 modules/mailutils/Makefile
		 modules/guile/Makefile
		 modules/mysql/Makefile
		 modules/postgres/Makefile
		 modules/ldap/Makefile
		 doc/Makefile])
AC_OUTPUT
