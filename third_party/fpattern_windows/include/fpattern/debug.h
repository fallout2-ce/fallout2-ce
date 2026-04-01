/******************************************************************************
* debug.h
*
* Copyright ©1997-2015 by David R. Tribble, all rights reserved.
*/


#ifndef drt_debug_h
#define drt_debug_h	1

#ifdef __cplusplus
extern "C"
{
#endif


/* Identification */

#ifndef NO_H_IDENT
static const char	drt_debug_h_id[] =
    "@(#)drt/src/lib/debug.h $Revision: 1.4 $ $Date: 2001/11/12 06:00:00 $";
#endif


/*==============================================================================
* Debug macros
*/

#ifndef DEBUG
 #define DEBUG	0
#endif

#if DEBUG-0 <= 0
 #undef  DEBUG
 #define DEBUG	0
#endif

#if DEBUG
 #define DL(e)	(opt_debug ? (void)(e) : (void)0)
#else
 #define DL(e)	((void)0)
#endif


#ifdef __cplusplus
}
#endif

#endif /* drt_debug_h */

/* End debug.h */
