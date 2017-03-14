# This file is part of Smap. 
# Copyright (C) 2010, 2014 Sergey Poznyakoff
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

if MAILUTILS_COND
  MAILUTILS_DIR=mailutils
endif
if GUILE_COND
  GUILE_DIR=guile
endif
if MYSQL_COND
  MYSQL_DIR=mysql
endif
if POSTGRES_COND
  POSTGRES_DIR=postgres
endif
if LDAP_COND
  LDAP_DIR=ldap
endif
SUBDIRS =\
 echo\
 sed\
 $(MAILUTILS_DIR)\
 $(GUILE_DIR)\
 $(MYSQL_DIR)\
 $(POSTGRES_DIR)\
 $(LDAP_DIR)

