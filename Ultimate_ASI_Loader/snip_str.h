/*
**  Header file for SNIPPETS string manipulation functions
*/

#ifndef SNIP_STR__H
#define SNIP_STR__H

#include <stddef.h>                       /* For size_t and NULL        */
#include <string.h>                       /* For strncpy() & memmove()  */
#include "sniptype.h"                     /* For LAST_CHAR() & NUL      */
#include "extkword.h"                     /* For FAR                    */

/*
**  Macros to print proper plurals by Bob Stout
*/

#define plural_text(n) &"s"[(1 == (n))]
#define plural_text2(n) &"es"[(1 == (n)) << 1]
#define plural_text3(n) &"y\0ies"[(1 != (n)) << 1]

/*
**  Safe string macros by Keiichi Nakasato
*/

/* strncpy() variants that are guaranteed to append NUL                 */

#define strn1cpy(d,s,n) (strncpy(d,s,n),(d)[n]=0,d)
#define strn0cpy(d,s,n) strn1cpy(d,s,(n)-1)

/* like strcpy, except guaranteed to work with overlapping strings      */

#define strMove(d,s) memmove(d,s,strlen(s)+1)

/*
**  Popular macros
*/

#define STREQ(s1,s2) (Success_==strcmp((s1),(s2)))
#define STRNEQ(s1,s2,N) (Success_==strncmp((s1),(s2),N))

/*
**  Prototypes
**
**  Note: If compiling strictly conforming ANSI/ISO standard C code, the
**        function names are modified to be compliant.
*/

#if defined(__STDC__) && __STDC__
 #define memmem   memMem
 #define strchcat strChcat
 #define strdel   strDel
 #define strdelch strDelch
 #define strdup   strDup
 #define strecpy  strEcpy
 #define stristr  strIstr
 #define strrepl  strRepl
 #define strrev   strRev
 #define strrpbrk strRpbrk
 #define strupr   strUpr
 #define strlwr   strLwr
#endif

#if defined(__cplusplus) && __cplusplus
 extern "C" {
#endif

void *memmem(const void *buf, const void *pattern,    /* Memmem.C       */
      size_t buflen, size_t len);
char *sstrcpy(char *to, char *from);                  /* Sstrcpy.C      */
char *sstrcat(char *to, char *from);                  /* Sstrcpy.C      */
char *sstrdel(char *s, ...);                          /* Sstrdel.C      */
char *stptok(const char *s, char *tok, size_t toklen,
      char *brk);                                     /* Stptok.C       */
char *strchcat(char *string, int ch, size_t buflen);  /* Strchcat.C     */
char *strdel(char *string, size_t first, size_t len); /* Strdel.C       */
char *strdelch(char *string, const char *lose);       /* Strdelch.C     */
char *strdup(const char *string);                     /* Strdup.C       */
char *strecpy(char *target, const char *src);         /* Strecpy.C/Asm  */
char *stristr(const char *String,                     /* Stristr.C      */
              const char *Pattern);
char *strrepl(char *Str, size_t BufSiz,
      char *OldStr, char *NewStr);                    /* Strrepl.C      */
char *strrev(char *str);                              /* Strrev.C       */
char *strrpbrk(const char *szString,
      const char *szChars);                           /* Strrpbrk.C     */
char *strupr(char *string);                           /* Strupr.C       */
char *strlwr(char *string);                           /* Strupr.C       */
char *translate(char *string);                        /* Translat.C     */
char *xstrcat(char *des, char *src, ...);             /* Xstrcat.C      */
char *rule_line(char * s, unsigned short len,
      short units, char * digits, char filler);       /* Ruleline.C     */
char *rmallws(char *str);                             /* Rmallws.C      */
char *rmlead(char *str);                              /* Rmlead.C       */
char *rmtrail(char *str);                             /* Rmtrail.C      */
char *trim (char *str);                               /* Trim.C         */
void lv1ws(char *str);                                /* Lv1Ws.C        */

#if defined(MSDOS) || defined(__MSDOS__)
 void FAR *fmemmem(const void FAR *buf,               /* Fmemmem.C      */
       const void FAR *pattern, long buflen, long len);
#endif

#if defined(__cplusplus) && __cplusplus
 }
#endif

#endif /*  SNIP_STR__H */
