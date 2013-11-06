/*
**  SNIPTYPE.H - Include file for SNIPPETS data types and commonly used macros
*/

#ifndef SNIPTYPE__H
#define SNIPTYPE__H

#include <stdlib.h>                             /* For free()           */
#include <string.h>                             /* For NULL & strlen()  */

typedef enum {Error_ = -1, Success_, False_ = 0, True_} Boolean_T;

#if defined(__unix__)
 typedef unsigned char  BYTE;
 typedef unsigned long  DWORD;
 typedef unsigned short WORD;
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
#elif !defined(WIN32) && !defined(_WIN32) && !defined(__NT__) \
      && !defined(_WINDOWS)
 #if !defined(OS2)
  typedef unsigned char  BYTE;
  typedef unsigned long  DWORD;
 #endif
 typedef unsigned short WORD;
#else
 #define WIN32_LEAN_AND_MEAN
 #define NOGDI
 #define NOSERVICE
 #undef INC_OLE1
 #undef INC_OLE2
 #include <windows.h>
 #define HUGE
#endif

typedef union {
      signed char       c;
      BYTE              b;
} VAR8_;

typedef union {
      VAR8_             v8[2];
      signed short      s;
      WORD              w;
} VAR16_;

typedef union {
      VAR16_            v16[2];
      signed long       l;
      DWORD             dw;
      float             f;
      void              *p;
} VAR32_;

typedef union {
      VAR32_            v32[2];
      double            d;
} VAR64_;

#define NUL '\0'
#define LAST_CHAR(s) (((char *)s)[strlen(s) - 1])
#define TOBOOL(x) (!(!(x)))
#define FREE(p) (free(p),(p)=NULL)

#endif /* SNIPTYPE__H */
