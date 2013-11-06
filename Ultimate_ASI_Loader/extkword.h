/*====================================================================

    _MSC_VER        Microsoft C 6.0 and later
    _QC             Microsoft Quick C 2.51 and later
    __TURBOC__      Borland Turbo C, Turbo C++ and BC++
    __BORLANDC__    Borland C++
    __ZTC__         Zortech C and C++
    __SC__          Symantec C++
    __WATCOMC__     WATCOM C
    __POWERC        Mix Power C
    __GNUC__        Gnu C

    Revised:

    25-Sep-95  Bob Stout      Original from PC-PORT.H
    30-Mar-96  Ed Blackman  OS/2 mods for OS/2 ver 2.0 and up
    30-May-96  Andrew Clarke  Added support for WATCOM C/C++ __NT__ macro.
    17-Jun-96  Bob Stout      Added __FLAT__ macros support
    20-Aug-96  Bob Stout      Eliminate Win32 conflicts
======================================================================*/


/* prevent multiple inclusions of this header file */

#ifndef EXTKWORD__H
#define EXTKWORD__H

#include <limits.h>                       /* For INT_MAX, LONG_MAX      */

/*
**  Watcom defines __FLAT__ for 32-bit environments and so will we
*/

#if !defined(__FLAT__) && !defined(__WATCOMC__) && !defined(_MSC_VER)
 #if defined(__GNUC__)
  #define __FLAT__ 1
 #elif defined (_WIN32) || defined(WIN32) || defined(__NT__)
  #define __FLAT__ 1
 #elif defined(__INTSIZE)
  #if (4 == __INTSIZE)
   #define __FLAT__ 1
  #endif
 #elif (defined(__ZTC__) && !defined(__SC__)) || defined(__TURBOC__)
  #if ((INT_MAX != SHRT_MAX) && (SHRT_MAX == 32767))
   #define __FLAT__ 1
  #endif
 #endif
#endif

/*
**  Correct extended keywords syntax
*/

#if defined(__unix__)
 #if !defined(FAR)
  #define FAR
 #endif
 #if !defined(NEAR)
  #define NEAR
 #endif
 #if !defined(HUGE)
  #define HUGE
 #endif
 #if !defined(PASCAL)
  #define PASCAL
 #endif
 #if !defined(CDECL)
  #define CDECL
 #endif
 #if !defined(INTERRUPT)
  #define INTERRUPT
 #endif
#elif defined(__OS2__)      /* EBB: not sure this works for OS/2 1.x */
 #include <os2def.h>
 #define INTERRUPT
 #ifndef HUGE
  #define HUGE
 #endif
#elif defined(_WIN32) || defined(WIN32) || defined(__NT__)
 #define WIN32_LEAN_AND_MEAN
 #define NOGDI
 #define NOSERVICE
 #undef INC_OLE1
 #undef INC_OLE2
 #include <windows.h>
 #define INTERRUPT
 #ifndef HUGE
  #define HUGE
 #endif
#else /* ! Win 32 or OS/2 */
 #if (defined(__POWERC) || (defined(__TURBOC__) && !defined(__BORLANDC__)) \
       || (defined(__ZTC__) && !defined(__SC__))) && !defined(__FLAT__)
  #define FAR far
  #define NEAR near
  #define PASCAL pascal
  #define CDECL cdecl
  #if (defined(__ZTC__) && !defined(__SC__)) || (defined(__SC__) && \
        (__SC__ < 0x700))
   #ifndef HUGE
    #define HUGE far
   #endif
   #define INTERRUPT
  #else
   #ifndef HUGE
    #define HUGE huge
   #endif
   #define INTERRUPT interrupt
  #endif
 #else
  #if (defined(__MSDOS__) || defined(MSDOS)) && !defined(__FLAT__)
   #define FAR _far
   #define NEAR _near
   #ifndef HUGE
    #define HUGE _huge
   #endif
   #define PASCAL _pascal
   #define CDECL _cdecl
   #define INTERRUPT _interrupt
  #else
   #define FAR
   #define NEAR
   #ifndef HUGE
    #define HUGE
   #endif
   #define PASCAL
   #define CDECL
  #endif
 #endif
#endif

#endif /* EXTKWORD__H */
