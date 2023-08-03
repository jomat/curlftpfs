AC_INIT([curlftpfs],[0.9.2])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

PKG_CHECK_MODULES(GLIB, [glib-2.0])
PKG_CHECK_MODULES(FUSE, [fuse >= 2.2])

LIBCURL_CHECK_CONFIG([yes], [7.17.0], [], [AC_MSG_ERROR(["libcurl not found"])])
if test "$libcurl_protocol_FTP" != yes; then
	AC_MSG_ERROR(["We need libcurl with support for FTP protocol."])
fi

case $host_os in
	darwin*)
	CFLAGS="$CFLAGS -D__off_t=off_t"
	;;
	*musl*)
	CFLAGS="$CFLAGS -D__off_t=off_t"
	;;
esac
CFLAGS="$CFLAGS -Wall -W -Wno-sign-compare -D_REENTRANT $GLIB_CFLAGS $FUSE_CFLAGS $LIBCURL_CPPFLAGS"
LIBS="$GLIB_LIBS $FUSE_LIBS $LIBCURL"

have_fuse_opt_parse=no
AC_CHECK_FUNC([fuse_opt_parse], [have_fuse_opt_parse=yes])

# Checks for header files.
m4_warn([obsolete],
[The preprocessor macro `STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  ISO C90 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_CHECK_HEADERS([fcntl.h netinet/in.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
m4_warn([obsolete],
[Update your code to rely only on HAVE_SYS_TIME_H,
then remove this warning and the obsolete code below it.
All current systems provide time.h; it need not be checked for.
Not all systems provide sys/time.h, but those that do, all allow
you to include it and time.h simultaneously.])dnl
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.

AC_STRUCT_TM
#AC_TYPE_UINT32_T
#AC_TYPE_UINT64_T
#AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([ftruncate getpass memmove memset mkdir realpath rmdir select strchr strdup strrchr strstr strtoull utime])

# Check for iconv
AM_ICONV

if test "$have_fuse_opt_parse" = no; then
	CFLAGS="$CFLAGS -Icompat -I../compat"
fi
AM_CONDITIONAL(FUSE_OPT_COMPAT, test "$have_fuse_opt_parse" = no)

AC_CONFIG_FILES([Makefile compat/Makefile tests/Makefile doc/Makefile])
AC_OUTPUT
