/* CMake config.h for Previous */

/* Define if you have a PNG compatible library */
#cmakedefine HAVE_LIBPNG 1

/* Define if you have a PCAP compatible library */
#cmakedefine HAVE_PCAP 1

/* Define if you have a readline compatible library */
#cmakedefine HAVE_LIBREADLINE 1

/* Define to 1 if you have the `z' library (-lz). */
#cmakedefine HAVE_LIBZ 1

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H 1

/* Define to 1 if you have the <limits.h> header file. */
#cmakedefine HAVE_LIMITS_H 1

/* Define to 1 if you have the <sys/syslimits.h> header file. */
#cmakedefine HAVE_SYS_SYSLIMITS_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/xattr.h> header file. */
#cmakedefine HAVE_SYS_XATTR_H 1

/* Define to 1 if you have the <tchar.h> header file. */
#cmakedefine HAVE_TCHAR_H 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#cmakedefine HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#cmakedefine HAVE_NETINET_IN_H 1

/* Define to 1 if you have the 'setenv' function. */
#cmakedefine HAVE_SETENV 1

/* Define to 1 if you have the `select' function. */
#cmakedefine HAVE_SELECT 1

/* Define to 1 if you have the 'gettimeofday' function. */
#cmakedefine HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the 'nanosleep' function. */
#cmakedefine HAVE_NANOSLEEP 1

/* Define to 1 if you have the 'alphasort' function. */
#cmakedefine HAVE_ALPHASORT 1

/* Define to 1 if you have the 'scandir' function. */
#cmakedefine HAVE_SCANDIR 1

/* Define to 1 if you have the 'fseeko' function. */
#cmakedefine HAVE_FSEEKO 1

/* Define to 1 if you have the 'ftello' function. */
#cmakedefine HAVE_FTELLO 1

/* Define to 1 if you have the 'flock' function. */
#cmakedefine HAVE_FLOCK 1

/* Define to 1 if you have the 'strlcpy' function. */
#cmakedefine HAVE_LIBC_STRLCPY 1

/* Define to 1 if you have the 'strdup' function */
#cmakedefine HAVE_STRDUP 1

/* Define to 1 if you have the 'lsetxattr' and 'lgetxattr' functions */
#cmakedefine HAVE_LXETXATTR 1

/* Define to 1 if you have the member 'st_atimespec.tv_nsec' in struct 'stat' */
#cmakedefine HAVE_STRUCT_STAT_ST_ATIMESPEC 1

/* Define to 1 if you have the member 'st_atimespec.tv_nsec' in struct 'stat' */
#cmakedefine HAVE_STRUCT_STAT_ST_MTIMESPEC 1

/* Define to 1 if you have the member 'd_namelen' in struct 'dirent' */
#cmakedefine HAVE_STRUCT_DIRENT_D_NAMELEN 1

/* Relative path from bindir to datadir */
#define BIN2DATADIR "@BIN2DATADIR@"

/* Define to 1 to enable trace logs - undefine to slightly increase speed */
#cmakedefine ENABLE_TRACING 1

/* Define to 1 if you have the 'posix_memalign' function */
#cmakedefine HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the 'aligned_alloc' function */
#cmakedefine HAVE_ALIGNED_ALLOC 1

/* Define to 1 if you have the '_aligned_alloc' function */
#cmakedefine HAVE__ALIGNED_ALLOC 1
