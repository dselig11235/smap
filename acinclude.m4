dnl This file is part of Smap.  -*- autoconf -*-
dnl Copyright (C) 2010, 2014 Sergey Poznyakoff
dnl
dnl Smap is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3, or (at your option)
dnl any later version.
dnl
dnl Smap is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with Smap.  If not, see <http://www.gnu.org/licenses/>.

dnl Arguments:
dnl   $1     --    Library to look for
dnl   $2     --    Function to check in the library
dnl   $3     --    Any additional libraries that might be needed
dnl   $4     --    Action to be taken when test succeeds
dnl   $5     --    Action to be taken when test fails
dnl   $6     --    Directories where the library may reside
AC_DEFUN([MU_CHECK_LIB],
[m4_ifval([$4], , [AH_CHECK_LIB([$1])])dnl
AS_VAR_PUSHDEF([mu_Lib], [mu_cv_lib_$1])dnl
AC_CACHE_CHECK([for $2 in -l$1], [mu_Lib],
[AS_VAR_SET([mu_Lib], [no])
 mu_check_lib_save_LIBS=$LIBS
 for path in "" $6
 do
   if test -n "$path"; then
     mu_ldflags="-L$path -l$1 $3"
   else
     mu_ldflags="-l$1 $3"
   fi
   LIBS="$mu_ldflags $mu_check_lib_save_LIBS"
   AC_LINK_IFELSE([AC_LANG_CALL([], [$2])],
                  [AS_VAR_SET([mu_Lib], ["$mu_ldflags"])
		   break])
 done		  
 LIBS=$mu_check_lib_save_LIBS])
AS_IF([test "AS_VAR_GET([mu_Lib])" != no],
      [m4_default([$4], [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1))
  LIBS="-l$1 $LIBS"
])],
      [$5])dnl
AS_VAR_POPDEF([mu_Lib])dnl
])# MU_CHECK_LIB


