/* Full copyright information is available in the file ../doc/CREDITS */

#ifndef cdc_config_h
#define cdc_config_h

#cmakedefine RESTRICTIVE_FILES
#cmakedefine CACHE_WIDTH @CACHE_WIDTH@
#cmakedefine CACHE_DEPTH @CACHE_DEPTH@

#cmakedefine VERSION_MAJOR @VERSION_MAJOR@
#cmakedefine VERSION_MINOR @VERSION_MINOR@
#cmakedefine VERSION_PATCH @VERSION_PATCH@
#ifndef VERSION_PATCH
// Work around VERSION_PATCH not being defined when 0.
#define VERSION_PATCH 0
#endif
#cmakedefine VERSION_RELEASE "@VERSION_RELEASE@"

#cmakedefine SYSTEM_TYPE "@SYSTEM_TYPE@"

#cmakedefine HAVE_UNISTD_H

#cmakedefine SIZEOF_LONG @SIZEOF_LONG@
#cmakedefine SIZEOF_FLOAT @SIZEOF_FLOAT@

#cmakedefine HAVE_GETRUSAGE
#cmakedefine HAVE_GETTIMEOFDAY
#cmakedefine HAVE_RINT
#cmakedefine HAVE_STRCSPN
#cmakedefine HAVE_STRERROR
#cmakedefine HAVE_STRFTIME

#cmakedefine __UNIX__
#cmakedefine __Win32__

#cmakedefine USE_CLEANER_THREAD
#cmakedefine DEBUG_DB_LOCK
#cmakedefine DEBUG_LOOKUP_LOCK
#cmakedefine DEBUG_BUCKET_LOCK
#cmakedefine DEBUG_CLEANER_LOCK

#cmakedefine USE_PARENT_OBJS

#endif
