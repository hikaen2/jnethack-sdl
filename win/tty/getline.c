/*	SCCS Id: @(#)getline.c	3.2	96/01/27	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mbchar.h"

#ifdef TTY_GRAPHICS

#include "wintty.h"
#include "func_tab.h"

#ifdef OVL1
char morc = 0;	/* tell the outside world what char you chose */
#endif /* OVL1 */
#ifdef OVL2
static boolean FDECL(ext_cmd_getlin_hook, (char *));
#endif /* OVL2 */

typedef boolean FDECL((*getlin_hook_proc), (char *));

STATIC_DCL void FDECL(hooked_tty_getlin, (const char*,char*,getlin_hook_proc));

extern char erase_char, kill_char;	/* from appropriate tty.c file */

#ifdef OVL1

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */

void
tty_getlin(query, bufp)
const char *query;
register char *bufp;
{
    hooked_tty_getlin(query, bufp, (getlin_hook_proc) 0);
}

STATIC_OVL void
hooked_tty_getlin(query, bfp, hook)
const char *query;
register char *bfp;
getlin_hook_proc hook;
{
/*JP*/
	char tmp[BUFSZ];
	char *bufp = tmp;
	register char *obufp = bufp;
/*JP	register int c;*/
	int c;
	struct WinDesc *cw = wins[WIN_MESSAGE];
	boolean doprev = 0;
/*JP
Thu Jul 21 22:36:34 JST 1994 by ISSEI
*/
	unsigned int uc;

	if(ttyDisplay->toplin == 1 && !(cw->flags & WIN_STOP)) more();
	cw->flags &= ~WIN_STOP;
	ttyDisplay->toplin = 3; /* special prompt state */
	ttyDisplay->inread++;
	pline("%s ", query);
	for(;;) {
		(void) fflush(stdout);
		Sprintf(toplines, "%s ", query);
		Strcat(toplines, obufp);
		if((c = Getchar()) == EOF) {
			*bufp = 0;
			break;
		}
/*JP
Thu Jul 21 22:36:34 JST 1994 by ISSEI
*/
		uc = (*((unsigned int *)&c));
		uc &= 0377;
		if(c == '\033') {
			*obufp = c;
			obufp[1] = 0;
			break;
		}
		if (ttyDisplay->intr) {
		    ttyDisplay->intr--;
		    *bufp = 0;
		}
		if(c == '\020') { /* ctrl-P */
		    if(!doprev)
			(void) tty_doprev_message(); /* need two initially */
		    (void) tty_doprev_message();
		    doprev = 1;
		    continue;
		} else if(doprev) {
		    tty_clear_nhwindow(WIN_MESSAGE);
		    cw->maxcol = cw->maxrow;
		    doprev = 0;
		    addtopl(query);
		    addtopl(" ");
		    *bufp = 0;
		    addtopl(obufp);
		}
		if(c == erase_char || c == '\b') {
			if(bufp != obufp) {
/*JP
 *      One character back, and as many cells as it occupied.  This was a
 *      goto loop that stepped a single byte and erased a single cell each
 *      time round, repeating while the position was not a character
 *      boundary.  That is the same thing only while a kanji is two bytes
 *      and two columns; under UTF-8 it is three bytes and still two
 *      columns, and the loop would rub out one cell too many.
 */
                                char *prev = (char *) mb_prev(obufp, bufp);
                                char *q;
                                int w = 0;

                                for (q = prev; q < bufp; q += mb_seqlen(q))
                                        w += mb_width(q);
                                bufp = prev;
                                while (w-- > 0)
					putsyms("\b \b");/* putsym converts \b */
			} else	tty_nhbell();
#if defined(apollo)
		} else if(c == '\n' || c == '\r') {
#else
		} else if(c == '\n') {
#endif
			*bufp = 0;
			break;
/*JP
		} else if(' ' <= c && c < '\177' &&
*/
		} else if(' ' <= uc && uc < '\377' &&
			    (bufp-obufp < BUFSZ-1 && bufp-obufp < COLNO)) {
				/* avoid isprint() - some people don't have it
				   ' ' is not always a printing char */
			*bufp = c;
			bufp[1] = 0;
/*JP			putsyms(bufp);*/
			raw_putsyms(bufp);
			bufp++;
			if (hook && (*hook)(obufp)) {
/*JP			    putsyms(bufp);*/
			    raw_putsyms(bufp);
			    bufp = eos(bufp);
			}
		} else if(c == kill_char || c == '\177') { /* Robert Viduya */
				/* this test last - @ might be the kill_char */
/*JP*/
                        {       /* cells, not bytes -- as above */
                                char *q;
                                int w = 0;

                                for (q = obufp; q < bufp; q += mb_seqlen(q))
                                        w += mb_width(q);
				bufp = obufp;
                                while (w-- > 0)
					putsyms("\b \b");
			}
		} else
			tty_nhbell();
	}
	ttyDisplay->toplin = 2;		/* nonempty, no --More-- required */
	ttyDisplay->inread--;
	clear_nhwindow(WIN_MESSAGE);	/* clean up after ourselves */
/*JP
Fri Aug 26 19:30:56 JST 1994 by ISSEI
*/
	Strcpy(bfp, str2ic(tmp));
}

void
xwaitforspace(s)
register const char *s;	/* chars allowed besides return */
{
    register int c, x = ttyDisplay ? (int) ttyDisplay->dismiss_more : '\n';

    morc = 0;

    while((c = tty_nhgetch()) != '\n') {
	if(iflags.cbreak) {
	    if ((s && index(s,c)) || c == x) {
		morc = (char) c;
		break;
	    }
	    tty_nhbell();
	}
    }

}

#endif /* OVL1 */
#ifdef OVL2

/*
 * Implement extended command completion by using this hook into
 * tty_getlin.  Check the characters already typed, if they uniquely
 * identify an extended command, expand the string to the whole
 * command.
 *
 * Return TRUE if we've extended the string at base.  Otherwise return FALSE.
 * Assumptions:
 *
 *	+ we don't change the characters that are already in base
 *	+ base has enough room to hold our string
 */
static boolean
ext_cmd_getlin_hook(base)
	char *base;
{
	int oindex, com_index;

	com_index = -1;
	for (oindex = 0; extcmdlist[oindex].ef_txt != (char *)0; oindex++) {
	    if (!strncmpi(base, extcmdlist[oindex].ef_txt, strlen(base))){
			if (com_index == -1)	/* no matches yet */
			    com_index = oindex;
			else			/* more than 1 match */
			    return FALSE;
	    }
	}
	if (com_index >= 0) {
		Strcpy(base, extcmdlist[com_index].ef_txt);
		return TRUE;
	}

	return FALSE;	/* didn't match anything */
}

/*
 * Read in an extended command, doing command line completion.  We
 * stop when we have found enough characters to make a unique command.
 */
int
tty_get_ext_cmd()
{
	int i;
	char buf[BUFSZ];

	/* maybe a runtime option? */
	/* hooked_tty_getlin("#", buf, flags.cmd_comp ? ext_cmd_getlin_hook : (getlin_hook_proc) 0); */
	hooked_tty_getlin("#", buf, ext_cmd_getlin_hook);
	if (buf[0] == 0 || buf[0] == '\033') return -1;

	for (i = 0; extcmdlist[i].ef_txt != (char *)0; i++)
		if (!strcmpi(buf, extcmdlist[i].ef_txt)) break;

	if (extcmdlist[i].ef_txt == (char *)0) {
/*JP
		pline("%s: unknown extended command.", buf);
*/
		pline("%s:拡張コマンドエラー", buf);
		i = -1;
	}

	return i;
}

#endif /* OVL2 */

#endif /* TTY_GRAPHICS */

/*getline.c*/
