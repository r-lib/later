#ifndef _LATER_FD_H_
#define _LATER_FD_H_

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // so R <= 4.1 can find WSAPoll() on Windows
#endif
#include <winsock2.h>
#else
#include <poll.h>
#endif

#ifdef _WIN32
#define LATER_POLL_FUNC WSAPoll
#else
#define LATER_POLL_FUNC poll
#endif

#endif // _LATER_FD_H_
