AC_INIT([YAM],[0.0.92],[https://github.com/v1cont/yam/issues/],[yam],[https://github.com/v1cont/yam/])
AC_CONFIG_AUX_DIR([build])

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects dist-xz no-dist-gzip])
AM_SILENT_RULES([yes])

AC_PREREQ([2.71])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIRS([m4])

dnl libtool versioning
LT_RELEASE=1.0
LT_CURRENT=1
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PATH_XTRA
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
AM_PROG_LEX
AC_PROG_YACC
LT_INIT

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

AC_CHECK_SIZEOF(unsigned short, 2)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(unsigned long, 4)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(u32)

if test "x$ac_cv_sizeof_time_t" = "x8"; then
	AC_DEFINE(HAVE_64BIT_TIME_T, 1, Define if time_t is 64-bit)
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(gethostname mkdir mktime socket strstr strchr \
	       uname flock lockf inet_aton inet_addr \
	       fchmod truncate getuid regcomp mlock fsync)

dnl Check for d_type member in struct dirent
AC_MSG_CHECKING([whether struct dirent has d_type member])
AC_CACHE_VAL(ac_cv_dirent_d_type,[
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <dirent.h>]], [[struct dirent d; d.d_type = DT_REG;]])],[ac_cv_dirent_d_type=yes],[ac_cv_dirent_d_type=no])
])
AC_MSG_RESULT($ac_cv_dirent_d_type)
if test $ac_cv_dirent_d_type = yes; then
	AC_DEFINE(HAVE_DIRENT_D_TYPE, 1,
		  Define if `struct dirent' has `d_type' member.)
fi

dnl Checks for math
AC_CHECK_HEADERS(math.h)
AC_CHECK_LIB(m, round)

dnl Checks for liblockfile
AC_CHECK_HEADERS(lockfile.h)
AC_CHECK_LIB(lockfile, lockfile_create)

dnl Checks for header files.
AC_HEADER_DIRENT

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h unistd.h paths.h \
		 sys/param.h sys/utsname.h sys/select.h \
		 netdb.h regex.h sys/mman.h)

dnl Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.58.0 gobject-2.0 gmodule-2.0])
AC_SUBST([GLIB_CFLAGS])
AC_SUBST([GLIB_LIBS])

PKG_CHECK_VAR([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal])
PKG_CHECK_VAR([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.22.0 gtk+-unix-print-3.0])
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])

dnl GPGME is used to support OpenPGP
AC_ARG_ENABLE(gpgme,
	[  --disable-gpgme         Disable GnuPG support using GPGME],
	[ac_cv_enable_gpgme=$enableval], [ac_cv_enable_gpgme=yes])
AC_MSG_CHECKING([whether to use GPGME])
if test $ac_cv_enable_gpgme = yes; then
	AC_MSG_RESULT(yes)
	AM_PATH_GPGME(1.0.0, AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.),
		      [use_gpgme=no
		       ac_cv_enable_gpgme=no])
	if test $ac_cv_enable_gpgme = yes; then
		AC_CHECK_LIB(gpgme, gpgme_io_writen, AC_DEFINE(HAVE_GPGME_IO_WRITEN, 1, Define if GPGME has gpgme_io_writen function.))
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for LDAP support in addressbook
dnl no check for libraries; dynamically loaded
AC_ARG_ENABLE(ldap,
	[AS_HELP_STRING([--enable-ldap],[Enable LDAP support (default no)])],
	[ac_cv_enable_ldap=$enableval], [ac_cv_enable_ldap=no])
AC_MSG_CHECKING([whether to use LDAP])
if test "$ac_cv_enable_ldap" = yes; then
	AC_MSG_RESULT(yes)

	dnl check for available libraries, and pull them in
	AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="$LDAP_LIBS -lresolv")
	AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
	AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
	AC_CHECK_LIB(lber, ber_get_tag, LDAP_LIBS="$LDAP_LIBS -llber",,
		     $LDAP_LIBS)

	dnl we need libpthread for YAM ldap, until we find
	dnl a better way to handle ldap requests asynchronously...
	AC_CHECK_LIB(pthread, pthread_create, LDAP_LIBS="$LDAP_LIBS -lpthread")

	AC_CHECK_HEADERS(ldap.h lber.h pthread.h,
			 [ ac_cv_enable_ldap=yes ],
			 [ ac_cv_enable_ldap=no ])

	if test "$ac_cv_enable_ldap" = yes; then
		AC_CHECK_LIB(ldap, ldap_open,
			     [ ac_cv_enable_ldap=yes ],
			     [ ac_cv_enable_ldap=no ],
			     $LDAP_LIBS)
	fi

	AC_MSG_CHECKING([whether ldap library is available])
	AC_MSG_RESULT($ac_cv_enable_ldap)

	if test "$ac_cv_enable_ldap" = yes; then
		CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gthread-2.0`"
		LDAP_LIBS="$LDAP_LIBS -lldap `$PKG_CONFIG --libs gthread-2.0`"
		AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
		AC_DEFINE(USE_THREADS, 1, Whether to use multithread or not)
		AC_SUBST(LDAP_LIBS)
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for OpenSSL
AC_ARG_ENABLE(ssl,
	[AS_HELP_STRING([--disable-ssl],[Disable SSL support using OpenSSL])],
	[ac_cv_enable_ssl=$enableval], [ac_cv_enable_ssl=yes])
AC_MSG_CHECKING([whether to use OpenSSL])
if test $ac_cv_enable_ssl = yes; then
	AC_MSG_RESULT(yes)
	dnl First check with pkg-config
	PKG_CHECK_MODULES(OPENSSL, openssl, [:], [:])
	if test "x$OPENSSL_LIBS" != "x"; then
		CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
		LIBS="$LIBS $OPENSSL_LIBS"
		AC_MSG_CHECKING([if openssl is available])
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_SSL, 1, Define if you use OpenSSL to support SSL.)
	else
		dnl If pkg-config fails, run compile/link test.
		AC_MSG_CHECKING([if openssl is available])
		ac_save_LIBS="$LIBS"
		if test "$native_win32" = yes; then
			LIBS="$LIBS -lssl32 -leay32"
		else
			LIBS="$LIBS -lssl -lcrypto"
		fi
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <openssl/opensslv.h>
]], [[ return OPENSSL_VERSION_NUMBER; ]])],[ AC_MSG_RESULT(yes)
		  AC_DEFINE(USE_SSL, 1, Define if you use OpenSSL to support SSL.) ],[ AC_MSG_RESULT(no)
		  LIBS="$ac_save_LIBS"
		  ac_cv_enable_ssl=no ])
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for X-Face support
AC_ARG_ENABLE(compface,
	[AS_HELP_STRING([--disable-compface],[Do not use compface (X-Face)])],
	[ac_cv_enable_compface=$enableval], [ac_cv_enable_compface=yes])
if test "$ac_cv_enable_compface" = yes; then
	AC_CHECK_LIB(compface, uncompface,,[ac_cv_enable_compface=no])
fi

dnl Check for GSpell support
AC_MSG_CHECKING([whether to use GSpell])
AC_ARG_ENABLE(gspell,
	[AS_HELP_STRING([--disable-gspell],[Do not use GSpell in editor])],
	[ac_cv_enable_gspell=$enableval], [ac_cv_enable_gspell=yes])
if test "$ac_cv_enable_gspell" = yes; then
    AC_MSG_RESULT(yes)
    PKG_CHECK_MODULES([SPELL], [gspell-1], [ac_cv_enable_gspell=yes], [ac_cv_enable_gspell=no])
    if test "$ac_cv_enable_gspell" = yes; then
       AC_DEFINE(USE_GSPELL, 1, Use GSpell in editor)
    fi
else
	AC_MSG_RESULT(no)
fi
AC_SUBST([SPELL_CFLAGS])
AC_SUBST([SPELL_LIBS])

dnl check for Tray option
AC_ARG_ENABLE(tray,
	[AS_HELP_STRING([--disable-tray],[Disable system tray support])],
	[ac_cv_enable_tray=$enableval], [ac_cv_enable_tray=yes])
if test x"$ac_cv_enable_tray" = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_TRAY, 1, Define if you want system tray support.)
else
	AC_MSG_RESULT(no)
fi

dnl check additional libraries
AC_CHECK_LIB(resolv, res_init)
AC_CHECK_LIB(socket, bind)
AC_CHECK_LIB(nsl, gethostbyname)

dnl check for IPv6 option
AC_ARG_ENABLE(ipv6,
	[AS_HELP_STRING([--disable-ipv6],[Disable IPv6 support])],
	[ac_cv_enable_ipv6=$enableval], [ac_cv_enable_ipv6=yes])

dnl automated checks for IPv6 support.
AC_MSG_CHECKING([whether to use IPv6])
if test x"$ac_cv_enable_ipv6" = xyes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([for IPv6 support])
	AC_CACHE_VAL(ac_cv_ipv6,[
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#define INET6
				#include <sys/types.h>
				#include <netinet/in.h>]], [[int x = IPPROTO_IPV6; struct in6_addr a;]])],[ac_cv_ipv6=yes],[ac_cv_ipv6=no])
	])
	AC_MSG_RESULT($ac_cv_ipv6)
	if test $ac_cv_ipv6 = yes; then
		AC_DEFINE(INET6, 1, Define if you want IPv6 support.)
	else
		AC_MSG_WARN(*** IPv6 will not be supported ***)
		ac_cv_enable_ipv6=no
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for gettext
IT_PROG_INTLTOOL

GETTEXT_PACKAGE=yam
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define text domain.])

AM_GLIB_GNU_GETTEXT

AC_CONFIG_FILES([
Makefile
lib/Makefile
src/Makefile
src/icons/Makefile
plugin/Makefile
plugin/attachment_tool/Makefile
plugin/test/Makefile
po/Makefile.in
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/24x24/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
data/icons/64x64/Makefile
data/icons/scalable/Makefile
data/yam.pc
])
AC_OUTPUT

dnl Output the configuration summary
echo ""
echo "GnuPG         : $ac_cv_enable_gpgme"
echo "LDAP          : $ac_cv_enable_ldap"
echo "OpenSSL       : $ac_cv_enable_ssl"
echo "compface      : $ac_cv_enable_compface"
echo "IPv6          : $ac_cv_enable_ipv6"
echo "GSpell        : $ac_cv_enable_gspell"
echo "Tray          : $ac_cv_enable_tray"
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
