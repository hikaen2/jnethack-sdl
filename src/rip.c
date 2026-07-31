/*	SCCS Id: @(#)rip.c	3.2	95/05/01	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"
#include "mbchar.h"

static void FDECL(center, (int, char *));

extern const char *killed_by_prefix[];

#if defined(TTY_GRAPHICS) || defined(X11_GRAPHICS) || defined(mac) || defined(__BEOS__)

static const char *rip_txt[] = {
"              ----------                      ----------",
"             /          \\                    /          \\",
"            /    REST    \\                  /    This    \\",
"           /      IN      \\                /  release of  \\",
"          /     PEACE      \\              /   NetHack is   \\",
"         /                  \\            /   dedicated to   \\",
"         |                  |            |  the memory of   |",
"         |                  |            |                  |",
"         |                  |            |  Izchak Miller   |",
"         |                  |            |   1935 - 1994    |",
"         |                  |            |                  |",
"         |                  |            |     Ascended     |",
"         |       1001       |            |                  |",
"      *  |     *  *  *      | *        * |      *  *  *     | *",
" _____)/\\|\\__//(\\/(/\\)/\\//\\/|_)________)/|\\\\_/_/(\\/(/\\)/\\/\\/|_)____",
0
};

#define STONE_LINE_CENT 19	/* char[] element of center of stone face */
#define STONE_LINE_LEN 16	/* # chars that fit on one line
				 * (note 1 ' ' border)
				 */
#define NAME_LINE 6		/* *char[] line # for player name */
#define GOLD_LINE 7		/* *char[] line # for amount of gold */
#define DEATH_LINE 8		/* *char[] line # for death description */
#define YEAR_LINE 12		/* *char[] line # for year */

char **rip;

static void
center(line, text)
int line;
char *text;
{
	register char *ip,*op;
	ip = text;
	op = &rip[line][STONE_LINE_CENT - ((strlen(text)+1)>>1)];
	while(*ip) *op++ = *ip++;
}


void
genl_outrip(tmpwin, how)
winid tmpwin;
int how;
{
	register char **dp;
	register char *dpx;
	char buf[BUFSZ];
	register int x;
	int line;

	rip = dp = (char **) alloc(sizeof(rip_txt));
	for (x = 0; rip_txt[x]; x++) {
		dp[x] = (char *) alloc((unsigned int)(strlen(rip_txt[x]) + 1));
		Strcpy(dp[x], rip_txt[x]);
	}
	dp[x] = (char *)0;

	/* Put name on stone */
	Sprintf(buf, "%s", plname);
	buf[STONE_LINE_LEN] = 0;
	center(NAME_LINE, buf);

	/* Put $ on stone */
	Sprintf(buf, "%ld Au", u.ugold);
	buf[STONE_LINE_LEN] = 0; /* It could be a *lot* of gold :-) */
	center(GOLD_LINE, buf);

	/* Put together death description */
	switch (killer_format) {
		default: impossible("bad killer format?");
		case KILLED_BY_AN:
			Strcpy(buf, an(killer));
			Strcat(buf, killed_by_prefix[how]);
			break;
		case KILLED_BY:
			Strcpy(buf, killer);
			Strcat(buf, killed_by_prefix[how]);
			break;
		case NO_KILLER_PREFIX:
			Strcpy(buf, killer);
			break;
	}

	/* Put death type on stone */
	for (line=DEATH_LINE, dpx = buf; line<YEAR_LINE; line++) {
		register int i,i0;
		char tmpchar;
/*JP
 *      The stone is STONE_LINE_LEN *columns* wide, and jstone_line divides
 *      a long epitaph evenly over the lines available rather than filling
 *      the first and leaving a stub.  Both were counted in bytes, which was
 *      the same number while a kanji was two of each; under UTF-8 a kanji
 *      is three bytes and two columns, so the lines came out two thirds the
 *      width they should be -- and the loop that advanced to a character
 *      boundary stepped two bytes at a time, so it could stop inside one.
 */
		int jstone_line, w = mb_colwidth(dpx);

		if (w <= STONE_LINE_LEN)
		  jstone_line = STONE_LINE_LEN;
		else if (w/2 <= STONE_LINE_LEN )
		  jstone_line = ((w+3)/4)*2;
		else if (w/3 <= STONE_LINE_LEN )
		  jstone_line = ((w+5)/6)*2;
		else
		  jstone_line = ((w+7)/8)*2;

                /* the most that fits, on a character boundary */
                i0 = mb_trunc_cols(dpx, jstone_line);
                if (!i0 && *dpx)
                  i0 = mb_seqlen(dpx);  /* one character, however wide */

                if (dpx[i0]) {
                        /* prefer the last space at or before that point */
			for(i = i0; i > 0; i--)
				if(dpx[i] == ' ') { i0 = i; break; }
		}
		tmpchar = dpx[i0];
		dpx[i0] = 0;
		center(line, dpx);
		if (tmpchar != ' ') {
			dpx[i0] = tmpchar;
			dpx= &dpx[i0];
		} else  dpx= &dpx[i0+1];
	}

	/* Put year on stone */
	Sprintf(buf, "%4d", getyear());
	center(YEAR_LINE, buf);

	putstr(tmpwin, 0, "");
	for(; *dp; dp++)
		putstr(tmpwin, 0, *dp);

	putstr(tmpwin, 0, "");
	putstr(tmpwin, 0, "");

	for (x = 0; rip_txt[x]; x++) {
		free((genericptr_t)rip[x]);
	}
	free((genericptr_t)rip);
}

#endif

/*rip.c*/
