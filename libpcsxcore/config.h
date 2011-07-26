// some basic definitions

#ifndef CONFIG_H
#define CONFIG_H

#ifndef PREFIX
#define PREFIX "./"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0"
#endif

#ifndef inline
#ifdef _DEBUG
#define inline /* */
#else
#define inline __inline__
#endif
#endif

#endif
