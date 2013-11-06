//**************************************
// Name: Case-insensitive strstr() work-alike
// Description:StriStr--This function is an ANSI version of strstr() with case insensitivity.
// By: Bob Stout (republished under Open Content License)
//
//
// Inputs:None
//
// Returns:None
//
//Assumes:None
//
//Side Effects:None
//This code is copyrighted and has limited warranties.
//Please see http://www.Planet-Source-Code.com/xq/ASP/txtCodeId.596/lngWId.3/qx/vb/scripts/ShowCode.htm
//for details.
//**************************************

/* +++Date last modified: 05-Jul-1997 */
/*
** Designation: StriStr
**
** Call syntax: char *stristr(char *String, char *Pattern)
**
** Description: This function is an ANSI version of strstr() with
**case insensitivity.
**
** Return item: char *pointer if Pattern is found in String, else
**pointer to 0
**
** Rev History: 07/04/95 Bob Stout ANSI-fy
**02/03/94 Fred Cole Original
**
** Hereby donated to public domain.
*/
#include "StdAfx.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "snip_str.h"
typedef unsigned int uint;
#if defined(__cplusplus) && __cplusplus


    extern "C" {
    #endif
    char *stristr(const char *String, const char *Pattern)


        {
        char *pptr, *sptr, *start;
        uint slen, plen;
        for (start = (char *)String,
        pptr = (char *)Pattern,
        slen = strlen(String),
        plen = strlen(Pattern);
        /* while string length not shorter than pattern length */
        slen >= plen;
        start++, slen--)


            {
            /* find start of pattern in string */
            while (toupper(*start) != toupper(*Pattern))


                {
                start++;
                slen--;
                /* if pattern longer than string */
                if (slen < plen)
                return(NULL);
            }
            sptr = start;
            pptr = (char *)Pattern;
            while (toupper(*sptr) == toupper(*pptr))


                {
                sptr++;
                pptr++;
                /* if end of pattern then pattern was found */
                if ('\0' == *pptr)
                return (start);
            }
        }
        return(NULL);
    }
    #if defined(__cplusplus) && __cplusplus
}
#endif
#ifdef TEST
int main(void)


    {
    char buffer[80] = "heLLo, HELLO, hello, hELLo, HellO";
    char *sptr = buffer;
    while (0 != (sptr = stristr(sptr, "hello")))
    printf("Found %5.5s!\n", sptr++);
    return(0);
}
#endif /* TEST */

		