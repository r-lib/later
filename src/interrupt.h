#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#ifdef _WIN32

#include <windows.h>
#undef TRUE
#undef FALSE

#endif // _WIN32


// Borrowed from https://github.com/wch/r-source/blob/a0a6b159/src/include/R_ext/GraphicsDevice.h#L843-L858
#ifndef BEGIN_SUSPEND_INTERRUPTS
/* Macros for suspending interrupts */
#define BEGIN_SUSPEND_INTERRUPTS do { \
    Rboolean __oldsusp__ = R_interrupts_suspended; \
    R_interrupts_suspended = TRUE;
#define END_SUSPEND_INTERRUPTS R_interrupts_suspended = __oldsusp__; \
    if (R_interrupts_pending && ! R_interrupts_suspended) \
        Rf_onintr(); \
} while(0)

#include <R_ext/libextern.h>
LibExtern Rboolean R_interrupts_suspended;
LibExtern int R_interrupts_pending;
extern void Rf_onintr(void);
LibExtern Rboolean mbcslocale;
#endif


#endif