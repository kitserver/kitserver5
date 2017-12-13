#ifndef _DIAG_MAIN_
#define _DIAG_MAIN_

#ifdef MYDLL_RELEASE_BUILD
#define DIAG_WINDOW_TITLE "KitServer 5 Diagnostics"
#else
#define DIAG_WINDOW_TITLE "KitServer 5 Diagnostics (debug build)"
#endif
#define CREDITS "About: v5.2.1 (6/2006) by Robbie."

#define LOG(f,x) if (f != NULL) fprintf x

#endif

