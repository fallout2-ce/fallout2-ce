/*******************************************************************************
* fpattern.c
*	Functions for matching filename patterns to filenames.
*
* Usage
*	(See "fpattern.h".)
*
* Notes
*	These pattern matching capabilities are modeled after those found in
*	the UNIX command shells.
*
*	`DELIM' must be defined to 1 if pathname separators are to be handled
*	explicitly.
*
* History
*	1.0, 1997-01-03, David Tribble.
*	First cut.
*
*	1.1, 1997-01-03, David Tribble.
*	Added the SUB pattern character.
*	Added function fpattern_matchn().
*
*	1.2, 1997-01-12, David Tribble.
*	Fixed missing lowercase matching cases.
*
*	1.3, 1997-01-13, David Tribble.
*	Pathname separator code is now controlled by DELIM macro.
*
*	1.4, 1997-01-14, David Tribble.
*	Added the QUOTE macro.
*
*	1.5, 1997-01-15, David Tribble.
*	Handles the special case of empty pattern and empty filename.
*
*	1.6, 1997-01-26, David Tribble.
*	Changed the range negation character from '^' to '!', ala Unix.
*
*	1.7, 1997-08-02, David Tribble.
*	Uses the 'FPAT_XXX' constants.
*
*	1.8, 1998-06-28, David Tribble.
*	Minor fixes for MS-VC++ (5.0).
*
*	1.9, 2001-11-21, David Tribble.
*	Minor fixes for Win32 compilations.
*
* Limitations
*	This code is copyrighted by the author, but permission is hereby granted
*	for its unlimited use provided that the original copyright and
*	authorship notices are retained intact.
*
*	Queries about this source code can be sent to: <david@tribble.com>
*
* Copyright �1997-2001 by David R. Tribble, all rights reserved.
*/


/* Identification */

static const char	id[] =
    "@(#)drt/src/lib/fpattern.c $Revision: 1.9 $ $Date: 2001/11/21 06:00:00 $";

static const char	copyright[] =
    "@(#)Portions are Copyright \2511997-2001 David R. Tribble, "
    "all rights reserved.\n";


/* System includes */

#include <ctype.h>
#include <stddef.h>

#if TEST
 #include <locale.h>
 #include <stdio.h>
 #include <string.h>
#endif

#if defined(FPAT_WINDOWS_PATHS)
 /* CE: force Windows path semantics even on Unix hosts. */
 #define UNIX	0
 #define DOS	1
#elif defined(unix) || defined(_unix) || defined(__unix)
 #define UNIX	1
 #define DOS	0
#elif defined(__MSDOS__) || defined(_WIN32)
 #define UNIX	0
 #define DOS	1
#else
 #error Cannot ascertain the O/S from predefined macros
#endif


/* Local includes */

#include "fpattern/debug.h"

#include "fpattern/fpattern.h"


/* Local constants */

#ifndef NULL
 #define NULL		((void *) 0)
#endif

#ifndef false
 #define false		0
#endif

#ifndef true
 #define true		1
#endif

#if TEST
 #define SUB		'~'
#else
 #define SUB		FPAT_CLOSP
#endif

#ifndef DELIM
 #define DELIM		0
#endif

#define DEL		FPAT_DEL

#if UNIX
 #define DEL2		FPAT_DEL
#else /*DOS*/
 #define DEL2		FPAT_DEL2
#endif

#if UNIX
 #define QUOTE		FPAT_QUOTE
#else /*DOS*/
 #define QUOTE		FPAT_QUOTE2
#endif


/* Local function macros */

#if UNIX
 #define lowercase(c)	(c)
#else /*DOS*/
 #define lowercase(c)	tolower(c)
#endif


/*------------------------------------------------------------------------------
* fpattern_isvalid()
*	Checks that filename pattern 'pat' is a well-formed pattern.
*
* Returns
*	True (1) if 'pat' is a valid filename pattern, otherwise false (0).
*
* Caveats
*	If 'pat' is null, false (0) is returned.
*
*	If 'pat' is empty (""), true (1) is returned, and it is considered a
*	valid (but degenerate) pattern (the only filename it matches is the
*	empty ("") string).
*/

int fpattern_isvalid(const char *pat)
{
    int		len;

    DL(printf("fpattern_isvalid: pat=%04p:\"%s\"\n", pat, pat ? pat : ""));

    /* Check args */
    if (pat == NULL)
    {
        DL(printf("Null pattern\n"));
        return (false);
    }

    /* Verify that the pattern is valid */
    for (len = 0;  pat[len] != '\0';  len++)
    {
        switch (pat[len])
        {
        case FPAT_SET_L:
            /* Char set */
            len++;
            if (pat[len] == FPAT_SET_NOT)
                len++;			/* Set negation */

            while (pat[len] != FPAT_SET_R)
            {
                if (pat[len] == QUOTE)
                    len++;		/* Quoted char */
                if (pat[len] == '\0')
                {
                    DL(printf("Missing '%c'\n", FPAT_SET_R));
                    return (false);	/* Missing closing bracket */
                }
                len++;

                if (pat[len] == FPAT_SET_THRU)
                {
                    /* Char range */
                    len++;
                    if (pat[len] == QUOTE)
                        len++;		/* Quoted char */
                    if (pat[len] == '\0')
                    {
                        DL(printf("Missing '%c%c'\n",
                            FPAT_SET_THRU, FPAT_SET_R));
                        return (false);	/* Missing closing bracket */
                    }
                    len++;
                }

                if (pat[len] == '\0')
                {
                    DL(printf("Missing '%c'\n", FPAT_SET_R));
                    return (false);	/* Missing closing bracket */
                }
            }
            break;

        case QUOTE:
            /* Quoted char */
            len++;
            if (pat[len] == '\0')
            {
                DL(printf("Missing quoted char\n"));
                return (false);		/* Missing quoted char */
            }
            break;

        case FPAT_NOT:
            /* Negated pattern */
            len++;
            if (pat[len] == '\0')
            {
                DL(printf("Missing negated subpattern\n"));
                return (false);		/* Missing subpattern */
            }
            break;

        default:
            /* Valid character */
            break;
        }
    }

    DL(printf("fpattern_isvalid: return %d\n", len));
    return (true);
}


/*------------------------------------------------------------------------------
* fpattern_submatch()
*	Attempts to match subpattern 'pat' to subfilename 'fname'.
*
* Returns
*	True (1) if the subfilename matches, otherwise false (0).
*
* Caveats
*	This does not assume that 'pat' is well-formed.
*
*	If 'pat' is empty (""), the only filename it matches is the empty ("")
*	string.
*
*	Some non-empty patterns (e.g., "") will match an empty filename ("").
*/

static int fpattern_submatch(const char *pat, const char *fname)
{
    int		fch;
    int		pch;
    int		i;
    int		yes, match;
    int		lo, hi;

    DL(printf("fpattern_submatch: fname=\"%s\", pat=\"%s\"\n", fname, pat));

    /* Attempt to match subpattern against subfilename */
    while (*pat != '\0')
    {
        fch = *fname;
        pch = *pat;
        pat++;

        switch (pch)
        {
        case FPAT_ANY:
            /* Match a single char */
        #if DELIM
            if (fch == DEL  ||  fch == DEL2  ||  fch == '\0')
        #else
            if (fch == '\0')
        #endif
            {
                DL(printf("match=F\n"));
                return (false);
            }
            fname++;
            break;

        case FPAT_CLOS:
            /* Match zero or more chars */
            i = 0;
        #if DELIM
            while (fname[i] != '\0'  &&
                    fname[i] != DEL  &&  fname[i] != DEL2)
                i++;
        #else
            while (fname[i] != '\0')
                i++;
        #endif
            while (i >= 0)
            {
                if (fpattern_submatch(pat, fname+i))
                {
                    DL(printf("submatch=T for +%d\n", i));
                    return (true);
                }
                i--;
            }
            return (false);

        case SUB:
            /* Match zero or more chars */
            i = 0;
            while (fname[i] != '\0'  &&
        #if DELIM
                    fname[i] != DEL  &&  fname[i] != DEL2  &&
        #endif
                    fname[i] != '.')
                i++;
            while (i >= 0)
            {
                if (fpattern_submatch(pat, fname+i))
                    return (true);
                i--;
            }
            return (false);

        case QUOTE:
            /* Match a quoted char */
            pch = *pat;
            if (lowercase(fch) != lowercase(pch)  ||  pch == '\0')
                return (false);
            fname++;
            pat++;
            break;

        case FPAT_SET_L:
            /* Match char set/range */
            yes = true;
            if (*pat == FPAT_SET_NOT)
            {
               pat++;
               yes = false;	/* Set negation */
            }

            /* Look for [s], [-], [abc], [a-c] */
            match = !yes;
            while (*pat != FPAT_SET_R  &&  *pat != '\0')
            {
                if (*pat == QUOTE)
                    pat++;	/* Quoted char */

                if (*pat == '\0')
                    break;
                lo = *pat++;
                hi = lo;

                if (*pat == FPAT_SET_THRU)
                {
                    /* Range */
                    pat++;

                    if (*pat == QUOTE)
                        pat++;	/* Quoted char */

                    if (*pat == '\0')
                        break;
                    hi = *pat++;
                }

                if (*pat == '\0')
                    break;

                /* Compare character to set range */
                if (lowercase(fch) >= lowercase(lo)  &&
                    lowercase(fch) <= lowercase(hi))
                    match = yes;
            }

            if (!match)
            {
                DL(printf("match=F\n"));
                return (false);
            }

            if (*pat == '\0')
                return (false);		/* Missing closing bracket */

            fname++;
            pat++;
            break;

        case FPAT_NOT:
            /* Match only if rest of pattern does not match */
            if (*pat == '\0')
                return (false);		/* Missing subpattern */
            i = fpattern_submatch(pat, fname);
            DL(printf("submatch=%c\n", "FT"[!!i]));
            return !i;

#if DELIM
        case DEL:
    #if DEL2 != DEL
        case DEL2:
    #endif
            /* Match path delimiter char */
            if (fch != DEL  &&  fch != DEL2)
                return (false);
            fname++;
            break;
#endif

        default:
            /* Match a (non-null) char exactly */
            if (lowercase(fch) != lowercase(pch))
            {
                DL(printf("fch:'%c' != pch:'%c'\n", fch, pch));
                return (false);
            }
            fname++;
            break;
        }
    }

    /* Check for complete match */
    if (*fname != '\0')
        return (false);

    /* Successful match */
    DL(printf("fpattern_submatch: pass\n"));
    return (true);
}


/*------------------------------------------------------------------------------
* fpattern_match()
*	Attempts to match pattern 'pat' to filename 'fname'.
*
* Returns
*	True (1) if the filename matches, otherwise false (0).
*
* Caveats
*	If 'fname' is null, false (0) is returned.
*
*	If 'pat' is null, false (0) is returned.
*
*	If 'pat' is empty (""), the only filename it matches is the empty string
*	("").
*
*	If 'fname' is empty, the only pattern that will match it is the empty
*	string ("").
*
*	If 'pat' is not a well-formed pattern, false (0) is returned.
*
*	Upper and lower case letters are treated the same; alphabetic characters
*	are converted to lower case before matching occurs.  Conversion to lower
*	case is dependent upon the current locale setting.
*/

int fpattern_match(const char *pat, const char *fname)
{
    int		rc;

    DL(printf("fpattern_match: fname=%04p:\"%s\", pat=%04p:\"%s\"\n",
        fname, fname ? fname : "", pat, pat ? pat : ""));

    /* Check args */
    if (fname == NULL)
        return (false);

    if (pat == NULL)
        return (false);

    /* Verify that the pattern is valid, and get its length */
    if (!fpattern_isvalid(pat))
        return (false);

    /* Attempt to match pattern against filename */
    if (fname[0] == '\0')
        return (pat[0] == '\0');	/* Special case */
    rc = fpattern_submatch(pat, fname);

    DL(printf("fpattern_match: return %c\n", "FT"[!!rc]));
    return (rc);
}


/*------------------------------------------------------------------------------
* fpattern_matchn()
*	Attempts to match pattern 'pat' to filename 'fname'.
*	This operates like fpattern_match() except that it does not verify that
*	pattern 'pat' is well-formed, assuming that it has been checked by a
*	prior call to fpattern_isvalid().
*
* Returns
*	True (1) if the filename matches, otherwise false (0).
*
* Caveats
*	If 'fname' is null, false (0) is returned.
*
*	If 'pat' is null, false (0) is returned.
*
*	If 'pat' is empty (""), the only filename it matches is the empty ("")
*	string.
*
*	If 'pat' is not a well-formed pattern, unpredictable results may occur.
*
*	Upper and lower case letters are treated the same; alphabetic characters
*	are converted to lower case before matching occurs.  Conversion to lower
*	case is dependent upon the current locale setting.
*
* See also
*	fpattern_match().
*/

int fpattern_matchn(const char *pat, const char *fname)
{
    int		rc;

    DL(printf("fpattern_matchn: fname=%04p:\"%s\", pat=%04p:\"%s\"\n",
        fname, fname ? fname : "", pat, pat ? pat : ""));

    /* Check args */
    if (fname == NULL)
        return (false);

    if (pat == NULL)
        return (false);

    /* Assume that pattern is well-formed */

    /* Attempt to match pattern against filename */
    rc = fpattern_submatch(pat, fname);

    DL(printf("fpattern_matchn: return %c\n", "FT"[!!rc]));
    return (rc);
}


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

#if TEST


/* Local variables */

static int	count =	0;
static int	fails =	0;
static int	stop_on_fail = false;


/*------------------------------------------------------------------------------
* test()
*/

static void test(int expect, const char *fname, const char *pat)
{
    int		failed;
    int		result;
    char	fbuf[80+1];
    char	pbuf[80+1];

    count++;
    printf("%3d. ", count);

    if (fname == NULL)
    {
        printf("<null>\n");
    }
    else
    {
        strcpy(fbuf, fname);
        printf("\"%s\"\n", fbuf);
    }

    if (pat == NULL)
    {
        printf("     <null>\n");
    }
    else
    {
        strcpy(pbuf, pat);
        printf("     \"%s\"\n", pbuf);
    }

    result = fpattern_match(pat == NULL ? NULL : pbuf,
                            fname == NULL ? NULL : fbuf);

    failed = (result != expect);
    printf("    -> %c, expected %c: %s\n",
        "FT"[!!result], "FT"[!!expect], failed ? "FAIL ***" : "pass");

    if (failed)
    {
        fails++;

        if (stop_on_fail)
            exit(1);
        sleep(1);
    }

    printf("\n");
}


/*------------------------------------------------------------------------------
* main()
*	Test driver.
*/

int main(int argc, char **argv)
{
    (void) argc;	/* Shut up lint */
    (void) argv;	/* Shut up lint */

#if DEBUG
    dbg_f = stdout;
#endif

    printf("==========================================\n");

    setlocale(LC_CTYPE, "");

#if UNIX
    printf("[O/S is UNIX]\n");
#elif DOS
    printf("[O/S is DOS]\n");
#else
    printf("[O/S is unknown]\n");
#endif

#if 1	/* Set to nonzero to stop on first failure */
    stop_on_fail = true;
#endif

    test(0,	NULL,	NULL);
    test(0,	NULL,	"");
    test(0,	NULL,	"abc");
    test(0,	"",	NULL);
    test(0,	"abc",	NULL);

    test(1,	"abc",		"abc");
    test(0,	"ab",		"abc");
    test(0,	"abcd",		"abc");
    test(0,	"Foo.txt",	"Foo.x");
    test(1,	"Foo.txt",	"Foo.txt");
    test(1,	"Foo.txt",	"foo.txt");
    test(1,	"FOO.txt",	"foo.TXT");

    test(1,	"a",		"?");
    test(1,	"foo.txt",	"f??.txt");
    test(1,	"foo.txt",	"???????");
    test(0,	"foo.txt",	"??????");
    test(0,	"foo.txt",	"????????");

    test(1,	"a",		"`a");
    test(1,	"AB",		"a`b");
    test(0,	"aa",		"a`b");
    test(1,	"a`x",		"a``x");
    test(1,	"a`x",		"`a```x");
    test(1,	"a*x",		"a`*x");

#if DELIM
    test(0,	"",		"/");
    test(0,	"",		"\\");
    test(1,	"/",		"/");
    test(1,	"/",		"\\");
    test(1,	"\\",		"/");
    test(1,	"\\",		"\\");

    test(1,	"a/b",		"a/b");
    test(1,	"a/b",		"a\\b");

    test(1,	"/",		"*/*");
    test(1,	"foo/a.c",	"f*/*.?");
    test(1,	"foo/a.c",	"*/*");
    test(0,	"foo/a.c",	"/*/*");
    test(0,	"foo/a.c",	"*/*/");

    test(1,	"/",		"~/~");
    test(1,	"foo/a.c",	"f~/~.?");
    test(0,	"foo/a.c",	"~/~");
    test(1,	"foo/abc",	"~/~");
    test(0,	"foo/a.c",	"/~/~");
    test(0,	"foo/a.c",	"~/~/");
#endif

    test(0,	"",		"*");
    test(1,	"a",		"*");
    test(1,	"ab",		"*");
    test(1,	"abc",		"**");
    test(1,	"ab.c",		"*.?");
    test(1,	"ab.c",		"*.*");
    test(1,	"ab.c",		"*?");
    test(1,	"ab.c",		"?*");
    test(1,	"ab.c",		"?*?");
    test(1,	"ab.c",		"?*?*");
    test(1,	"ac",		"a*c");
    test(1,	"axc",		"a*c");
    test(1,	"ax-yyy.c",	"a*c");
    test(1,	"ax-yyy.c",	"a*x-yyy.c");
    test(1,	"axx/yyy.c",	"a*x/*c");

    test(0,	"",		"~");
    test(1,	"a",		"~");
    test(1,	"ab",		"~");
    test(1,	"abc",		"~~");
    test(1,	"ab.c",		"~.?");
    test(1,	"ab.c",		"~.~");
    test(0,	"ab.c",		"~?");
    test(0,	"ab.c",		"?~");
    test(0,	"ab.c",		"?~?");
    test(1,	"ab.c",		"?~.?");
    test(1,	"ab.c",		"?~?~");
    test(1,	"ac",		"a~c");
    test(1,	"axc",		"a~c");
    test(0,	"ax-yyy.c",	"a~c");
    test(1,	"ax-yyyvc",	"a~c");
    test(1,	"ax-yyy.c",	"a~x-yyy.c");
    test(0,	"axx/yyy.c",	"a~x/~c");
    test(1,	"axx/yyyvc",	"a~x/~c");

    test(0,	"a",		"!");
    test(0,	"a",		"!a");
    test(1,	"a",		"!b");
    test(1,	"abc",		"!abb");
    test(0,	"a",		"!*");
    test(1,	"abc",		"!*.?");
    test(1,	"abc",		"!*.*");
    test(0,	"",		"!*");		/*!*/
    test(0,	"",		"!*?");		/*!*/
    test(0,	"a",		"!*?");
    test(0,	"a",		"a!*");
    test(1,	"a",		"a!?");
    test(1,	"a",		"a!*?");
    test(1,	"ab",		"*!?");
    test(1,	"abc",		"*!?");
    test(0,	"ab",		"?!?");
    test(1,	"abc",		"?!?");
    test(0,	"a-b",		"!a[-]b");
    test(0,	"a-b",		"!a[x-]b");
    test(0,	"a=b",		"!a[x-]b");
    test(0,	"a-b",		"!a[x`-]b");
    test(1,	"a=b",		"!a[x`-]b");
    test(0,	"a-b",		"!a[x---]b");
    test(1,	"a=b",		"!a[x---]b");

    test(1,	"abc",		"a[b]c");
    test(1,	"aBc",		"a[b]c");
    test(1,	"abc",		"a[bB]c");
    test(1,	"abc",		"a[bcz]c");
    test(1,	"azc",		"a[bcz]c");
    test(0,	"ab",		"a[b]c");
    test(0,	"ac",		"a[b]c");
    test(0,	"axc",		"a[b]c");

    test(0,	"abc",		"a[!b]c");
    test(0,	"abc",		"a[!bcz]c");
    test(0,	"azc",		"a[!bcz]c");
    test(0,	"ab",		"a[!b]c");
    test(0,	"ac",		"a[!b]c");
    test(1,	"axc",		"a[!b]c");
    test(1,	"axc",		"a[!bcz]c");

    test(1,	"a1z",		"a[0-9]z");
    test(0,	"a1",		"a[0-9]z");
    test(0,	"az",		"a[0-9]z");
    test(0,	"axz",		"a[0-9]z");
    test(1,	"a2z",		"a[-0-9]z");
    test(1,	"a-z",		"a[-0-9]z");
    test(1,	"a-b",		"a[-]b");
    test(0,	"a-b",		"a[x-]b");
    test(0,	"a=b",		"a[x-]b");
    test(1,	"a-b",		"a[x`-]b");
    test(0,	"a=b",		"a[x`-]b");
    test(1,	"a-b",		"a[x---]b");
    test(0,	"a=b",		"a[x---]b");

    test(0,	"a0z",		"a[!0-9]z");
    test(1,	"aoz",		"a[!0-9]z");
    test(0,	"a1",		"a[!0-9]z");
    test(0,	"az",		"a[!0-9]z");
    test(0,	"a9Z",		"a[!0-9]z");
    test(1,	"acz",		"a[!-0-9]z");
    test(0,	"a7z",		"a[!-0-9]z");
    test(0,	"a-z",		"a[!-0-9]z");
    test(0,	"a-b",		"a[!-]b");
    test(0,	"a-b",		"a[!x-]b");
    test(0,	"a=b",		"a[!x-]b");
    test(0,	"a-b",		"a[!x`-]b");
    test(1,	"a=b",		"a[!x`-]b");
    test(0,	"a-b",		"a[!x---]b");
    test(1,	"a=b",		"a[!x---]b");

    test(1,	"a!z",		"a[`!0-9]z");
    test(1,	"a3Z",		"a[`!0-9]z");
    test(0,	"A3Z",		"a[`!0`-9]z");
    test(1,	"a9z",		"a[`!0`-9]z");
    test(1,	"a-z",		"a[`!0`-9]z");

done:
    printf("%d tests, %d failures\n", count, fails);
    return (fails == 0 ? 0 : 1);
}


#endif /* TEST */

/* End fpattern.c */
