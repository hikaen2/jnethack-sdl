/*	SCCS Id: @(#)engrave.c	3.2	96/04/21	*/
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
#include "lev.h"
#include <ctype.h>

STATIC_VAR NEARDATA struct engr *head_engr;

#ifdef OVLB
/* random engravings */
static const char *random_mesg[] = {
	"Elbereth",
	/* trap engravings */
	"Vlad was here", "ad aerarium",
	/* take-offs and other famous engravings */
	"Owlbreath", "Galadriel",
	"Kilroy was here",
	"A.S. ->", "<- A.S.", /* Journey to the Center of the Earth */
	"You won't get it up the steps", /* Adventure */
	"Lasciate ogni speranza o voi ch'entrate.", /* Inferno */
	"Well Come", /* Prisoner */
	"We apologize for the inconvenience.", /* So Long... */
	"See you next Wednesday", /* Thriller */
	"notary sojak", /* Smokey Stover */
	"For a good time call 8?7-5309",
	"Moon prism power make up!", /* Sailor Moon */
	"Lizard's Tail!", /* Mahoujin Guru Guru */
	"It's a show time!" /* St. Tail */ 
};

char *
random_engraving(outbuf)
char *outbuf;
{
	const char *rumor;

	/* a random engraving may come from the "rumors" file,
	   or from the list above */
	if (!rn2(4) || !(rumor = getrumor(0, outbuf)) || !*rumor)
	    Strcpy(outbuf, random_mesg[rn2(SIZE(random_mesg))]);

	wipeout_text(outbuf, (int)(strlen(outbuf) / 4), 0);
	return outbuf;
}

/* Partial rubouts for engraving characters. -3. */
static const struct {
	char		wipefrom;
	const char *	wipeto;
} rubouts[] = {
	{'A', "^"},     {'B', "Pb["},   {'C', "("},     {'D', "|)["},
	{'E', "|FL[_"}, {'F', "|-"},    {'G', "C("},    {'H', "|-"},
	{'I', "|"},     {'K', "|<"},    {'L', "|_"},    {'M', "|"},
	{'N', "|\\"},   {'O', "C("},    {'P', "F"},     {'Q', "C("},
	{'R', "PF"},    {'T', "|"},     {'U', "J"},     {'V', "/\\"},
	{'W', "V/\\"},  {'Z', "/"},
	{'b', "|"},     {'d', "c|"},    {'e', "c"},     {'g', "c"},
	{'h', "n"},     {'j', "i"},     {'k', "|"},     {'l', "|"},
	{'m', "nr"},    {'n', "r"},     {'o', "c"},     {'q', "c"},
	{'w', "v"},     {'y', "v"},
	{':', "."},     {';', ","},
	{'0', "C("},    {'1', "|"},     {'6', "o"},     {'7', "/"},
	{'8', "3o"}
};
/*JP*/

/*JP
**      Replace the character at engr[nxt] with rep, shifting the rest of the
**      string if the two are not the same number of bytes.  Under EUC-JP a
**      kanji and the fullwidth "？" that rubs it out are both two bytes and
**      the move never happens; under UTF-8 they are both three, but the two
**      spaces that blank an already-rubbed "？" are not.
*/
static void
jrub_replace(engr, nxt, rep)
        char *engr;
        int nxt;
        const char *rep;
{
        int n = mb_seqlen(engr + nxt);
        int r = (int) strlen(rep);

        if (n != r)
                (void) memmove(engr + nxt + r, engr + nxt + n,
                               strlen(engr + nxt + n) + 1);
        (void) memcpy(engr + nxt, rep, r);
}

void
wipeout_text(engr, cnt, seed)
char *engr;
int cnt;
unsigned seed;		/* for semi-controlled randomization */
{
	unsigned char *s;
	int i, j, nxt, use_rubout, lth = (int)strlen(engr);

	if (lth && cnt > 0) {
	    while (cnt--) {
		/* pick next character */
		if (!seed) {
		    /* random */
		    nxt = rn2(lth);
		    use_rubout = rn2(4);
		} else {
		    /* predictable; caller can reproduce the same sequence by
		       supplying the same arguments later, or a pseudo-random
		       sequence by varying any of them (but see the "pick one"
		       comment below for a caveat about "the same sequence") */
		    nxt = seed % lth;
		    seed *= 31,  seed %= (BUFSZ-1);
		    use_rubout = seed & 3;
		}
/*JP
**	2BYTE文字を間違わずに消す
**
**	英語では，'E' -> 'F' など断片的に文字が消えていくようなエンコーディングを
**	しているが，日本語ではとりあえず保留．
*/

                /* nxt is a random byte offset; move it to the start of
                   whichever character it landed in.  This was "--nxt",
                   which is one byte short of the start of a three-byte
                   character. */
		if(!mb_is_boundary(engr, nxt))
                  nxt = (int)((char *)mb_prev(engr, engr + nxt) - engr);

		s = (unsigned char *)&engr[nxt];
		if (*s == ' ') continue;

                if(mb_seqlen(&engr[nxt]) > 1){  /* nxt is on a boundary */

                  if(!strncmp(&engr[nxt], "？", sizeof("？")-1)){
                    /* already rubbed out once: blank it, one space per
                       column rather than one per byte */
                    jrub_replace(engr, nxt, "  ");
		    continue;
		  }
		  else{
                    jrub_replace(engr, nxt, "？");
		    continue;
		  }
		}
		/* rub out unreadable & small punctuation marks */
		if (index("?.,'`-|_", *s)) {
		    *s = ' ';
		    continue;
		}
		if (!use_rubout)
		    i = SIZE(rubouts);
		else
		    for (i = 0; i < SIZE(rubouts); i++)
			if (*s == rubouts[i].wipefrom) {
			    /*
			     * Pick one of the substitutes at random.
			     * For the `seed' pseudo-random case, this can
			     * produce minor variations each time when/if the
			     * caller tries to regenerate the scrambled text.
			     * That's deemed acceptable for intended usage....
			     */
			    j = rn2(strlen(rubouts[i].wipeto));
			    *s = rubouts[i].wipeto[j];
			    break;
			}

		/* didn't pick rubout; use '?' for unreadable character */
		if (i == SIZE(rubouts)) *s = '?';
	    }
	}

	/* trim trailing spaces */
	while (lth && engr[lth-1] == ' ') engr[--lth] = 0;
}

boolean
can_reach_floor()
{
	return (boolean)(!u.uswallow &&
			 (!Levitation ||
			  Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)));
}
#endif /* OVLB */
#ifdef OVL0

const char *
surface(x, y)
register int x, y;
{
	register struct rm *lev = &levl[x][y];

	if ((x == u.ux) && (y == u.uy) && u.uswallow &&
		is_animal(u.ustuck->data))
/*JP	    return "maw";*/
	    return "胃袋";
	else if (IS_AIR(lev->typ))
/*JP	    return "air";*/
	    return "空中";
	else if (is_pool(x,y))
/*JP	    return "water";*/
	    return "水中";
	else if (is_ice(x,y))
/*JP	    return "ice";*/
	    return "氷";
	else if (is_lava(x,y))
/*JP	    return "lava";*/
	    return "溶岩";
	else if (lev->typ == DRAWBRIDGE_DOWN)
/*JP	    return "bridge";*/
	    return "橋";
	else if(IS_ALTAR(levl[x][y].typ))
/*JP	    return "altar";*/
	    return "祭壇";
	else if ((IS_ROOM(lev->typ) && !Is_earthlevel(&u.uz)) ||
		 IS_WALL(lev->typ) || IS_DOOR(lev->typ) || lev->typ == SDOOR)
/*JP	    return "floor";*/
	    return "床";
	else
/*JP	    return "ground";*/
	    return "地面";
}

const char *
ceiling(x, y)
register int x, y;
{
	register struct rm *lev = &levl[x][y];
	const char *what;

	/* other room types will no longer exist when we're interested --
	 * see check_special_room()
	 */
	if (*in_rooms(x,y,VAULT))
/*JP	    what = "vault's ceiling";*/
	    what = "倉庫の天井";
	else if (*in_rooms(x,y,TEMPLE))
/*JP	    what = "temple's ceiling";*/
	    what = "寺院の天井";
	else if (*in_rooms(x,y,SHOPBASE))
/*JP	    what = "shop's ceiling";*/
	    what = "店の天井";
	else if (IS_AIR(lev->typ))
/*JP	    what = "sky";*/
	    what = "空";
	else if (Underwater)
/*JP	    what = "water's surface";*/
	    what = "水面";
	else if ((IS_ROOM(lev->typ) && !Is_earthlevel(&u.uz)) ||
		 IS_WALL(lev->typ) || IS_DOOR(lev->typ) || lev->typ == SDOOR)
/*JP	    what = "ceiling";*/
	    what = "天井";
	else
/*JP	    what = "rock above";*/
	    what = "上方の岩";

	return what;
}

struct engr *
engr_at(x, y)
xchar x, y;
{
	register struct engr *ep = head_engr;

	while(ep) {
		if(x == ep->engr_x && y == ep->engr_y)
			return(ep);
		ep = ep->nxt_engr;
	}
	return((struct engr *) 0);
}

#ifdef ELBERETH
/* decide whether a particular string is engraved at a specified location;
   a case-insensitive substring match used */
int
sengr_at(s, x, y)
	const char *s;
	xchar x, y;
{
	register struct engr *ep = engr_at(x,y);

	return (ep && ep->engr_time <= moves && strstri(ep->engr_txt, s) != 0);
}
#endif /* ELBERETH */

#endif /* OVL0 */
#ifdef OVL2

void
u_wipe_engr(cnt)
register int cnt;
{
	if (can_reach_floor())
		wipe_engr_at(u.ux, u.uy, cnt);
}

#endif /* OVL2 */
#ifdef OVL1

void
wipe_engr_at(x,y,cnt)
register xchar x,y,cnt;
{
	register struct engr *ep = engr_at(x,y);

	if(ep){
	    if(ep->engr_type != BURN || is_ice(x,y)) {
		if(ep->engr_type != DUST && ep->engr_type != BLOOD) {
			cnt = rn2(1 + 50/(cnt+1)) ? 0 : 1;
		}
		wipeout_text(ep->engr_txt, (int)cnt, 0);
		while(ep->engr_txt[0] == ' ')
			ep->engr_txt++;
		if(!ep->engr_txt[0]) del_engr(ep);
	    }
	}
}

#endif /* OVL1 */
#ifdef OVL2

void
read_engr_at(x,y)
register int x,y;
{
	register struct engr *ep = engr_at(x,y);
	register int	sensed = 0;

	if(ep && ep->engr_txt[0]) {
	    switch(ep->engr_type) {
	    case DUST:
		if(!Blind) {
			sensed = 1;
/*JP			pline("%s is written here in the %s.", Something,
				is_ice(x,y) ? "frost" : "dust");*/
			pline("何かの文字が%sに書いてある．",
				is_ice(x,y) ? "氷" : "ほこり");

		}
		break;
	    case ENGRAVE:
		if (!Blind || can_reach_floor()) {
			sensed = 1;
/*JP			pline("%s is engraved here on the %s.",
				Something,
				surface(x,y));*/
			pline("何かの文字が%sに刻まれている．",
  				surface(x,y));

		}
		break;
	    case BURN:
		if (!Blind || can_reach_floor()) {
			sensed = 1;
/*JP			pline("Some text has been %s into the %s here.",
				is_ice(x,y) ? "melted" : "burned",
				surface(x,y));*/
			pline("何かの文字が%sに%sいる．",
				surface(x,y),
				is_ice(x,y) ? "刻まれて" : "燃えて");
		}
		break;
	    case MARK:
		if(!Blind) {
			sensed = 1;
/*JP			pline("There's some graffiti on the %s here.",*/
			pline("何かの落書が%sにある．",
				surface(x,y));
		}
		break;
	    case BLOOD:
		/* "It's a message!  Scrawled in blood!"
		 * "What's it say?"
		 * "It says... `See you next Wednesday.'" -- Thriller
		 */
		if(!Blind) {
			sensed = 1;
/*JP			You("see a message scrawled in blood here.");*/
			You("血文字がなぐり書きされているのを見つけた．");
		}
		break;
	    default:
		impossible("%s is written in a very strange way.",
				Something);
		sensed = 1;
	    }
	    if (sensed) {
/*JP		You("%s: \"%s\".",
		      (Blind) ? "feel the words" : "read",  ep->engr_txt);*/
		You("%s：「%s」",
		      (Blind) ? "次のように感じた" : "読んだ",  ep->engr_txt);
		if(flags.run > 1) nomul(0);
	    }
	}
}

#endif /* OVL2 */
#ifdef OVLB

void
make_engr_at(x,y,s,e_time,e_type)
register int x,y;
register const char *s;
register long e_time;
register xchar e_type;
{
	register struct engr *ep;

	if ((ep = engr_at(x,y)) != 0)
	    del_engr(ep);
	ep = newengr(strlen(s) + 1);
	ep->nxt_engr = head_engr;
	head_engr = ep;
	ep->engr_x = x;
	ep->engr_y = y;
	ep->engr_txt = (char *)(ep + 1);
	Strcpy(ep->engr_txt, s);
	if(strcmp(s, "Elbereth")) exercise(A_WIS, TRUE);
	ep->engr_time = e_time;
	ep->engr_type = e_type > 0 ? e_type : rnd(N_ENGRAVE);
	ep->engr_lth = strlen(s) + 1;
}

/* delete any engraving at location <x,y> */
void
del_engr_at(x, y)
int x, y;
{
	register struct engr *ep = engr_at(x, y);

	if (ep) del_engr(ep);
}

/*
 *	freehand - returns true if player has a free hand
 */
int
freehand()
{

	return(!uwep || !welded(uwep) ||
	   (!bimanual(uwep) && (!uarms || !uarms->cursed)));
/*	if ((uwep && bimanual(uwep)) ||
	    (uwep && uarms))
		return(0);
	else
		return(1);*/
}

static NEARDATA const char styluses[] =
	{ ALL_CLASSES, ALLOW_NONE, TOOL_CLASS, WEAPON_CLASS, WAND_CLASS,
	  GEM_CLASS, RING_CLASS, 0 };

/* Mohs' Hardness Scale:
 *  1 - Talc		 6 - Orthoclase
 *  2 - Gypsum		 7 - Quartz
 *  3 - Calcite		 8 - Topaz
 *  4 - Fluorite	 9 - Corundum
 *  5 - Apatite		10 - Diamond
 *
 * Since granite is a igneous rock hardness ~ 7, anything >= 8 should
 * probably be able to scratch the rock.
 * Devaluation of less hard gems is not easily possible because obj struct
 * does not contain individual oc_cost currently. 7/91
 *
 * steel     -	5-8.5	(usu. weapon)
 * diamond    - 10			* jade	     -	5-6	 (nephrite)
 * ruby       -  9	(corundum)	* turquoise  -	5-6
 * sapphire   -  9	(corundum)	* opal	     -	5-6
 * topaz      -  8			* glass      - ~5.5
 * emerald    -  7.5-8	(beryl)		* dilithium  -	4-5??
 * aquamarine -  7.5-8	(beryl)		* iron	     -	4-5
 * garnet     -  7.25	(var. 6.5-8)	* fluorite   -	4
 * agate      -  7	(quartz)	* brass      -	3-4
 * amethyst   -  7	(quartz)	* gold	     -	2.5-3
 * jasper     -  7	(quartz)	* silver     -	2.5-3
 * onyx       -  7	(quartz)	* copper     -	2.5-3
 * moonstone  -  6	(orthoclase)	* amber      -	2-2.5
 */

/* return 1 if action took 1 (or more) moves, 0 if error or aborted */
int
doengrave()
{
	boolean dengr = FALSE;	/* TRUE if we wipe out the current engraving */
	boolean doblind = FALSE;/* TRUE if engraving blinds the player */
	boolean doknown = FALSE;/* TRUE if we identify the stylus */
	boolean eow = FALSE;	/* TRUE if we are overwriting oep */
	boolean jello = FALSE;	/* TRUE if we are engraving in slime */
	boolean ptext = TRUE;	/* TRUE if we must prompt for engrave text */
	boolean teleengr =FALSE;/* TRUE if we move the old engraving */
	boolean zapwand = FALSE;/* TRUE if we remove a wand charge */
	xchar type = DUST;	/* Type of engraving made */
	char buf[BUFSZ];	/* Buffer for final/poly engraving text */
	char ebuf[BUFSZ];	/* Buffer for initial engraving text */
	char qbuf[QBUFSZ];	/* Buffer for query text */
	char post_engr_text[BUFSZ]; /* Text displayed after engraving prompt */
	const char *everb;	/* Present tense of engraving type */
	const char *eloc;	/* Where the engraving is (ie dust/floor/...) */
	register char *sp;	/* Place holder for space count of engr text */
	register int len;	/* # of nonspace chars of new engraving text */
	register int maxelen;	/* Max allowable length of new engraving text */
	register int spct;	/* # of spaces in new engraving text */
	register struct engr *oep = engr_at(u.ux,u.uy);
				/* The current engraving */
	register struct obj *otmp; /* Object selected with which to engrave */
	char *writer;


	multi = 0;		/* moves consumed */
	nomovemsg = (char *)0;	/* occupation end message */

	buf[0] = (char)0;
	ebuf[0] = (char)0;
	post_engr_text[0] = (char)0;
	maxelen = BUFSZ - 1;

	/* Can the adventurer engrave at all? */

	if(u.uswallow) {
		if (is_animal(u.ustuck->data)) {
/*JP			pline("What would you write?  \"Jonah was here\"?");*/
			pline("何を書くんだい？「ヨナはここにいる」？");
			return(0);
		} else if (is_whirly(u.ustuck->data)) {
/*JP			You_cant("reach the %s.", surface(u.ux,u.uy));*/
			You("%sに届かない．", surface(u.ux,u.uy));
			return(0);
		} else
			jello = TRUE;
	} else if (is_lava(u.ux, u.uy)) {
/*JP		You_cant("write on the lava!");*/
		You("溶岩には書けない！");
		return(0);
	} else if (is_pool(u.ux,u.uy) || IS_FOUNTAIN(levl[u.ux][u.uy].typ)) {
/*JP		You_cant("write on the water!");*/
		You("水には書けない！");
		return(0);
	}
	if(Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)/* in bubble */) {
/*JP		You_cant("write in thin air!");*/
		You("空中には書けない！");
		return(0);
	}
	if (cantwield(uasmon)) {
/*JP		You_cant("even hold anything!");*/
		You("何も持てない！");
		return(0);
	}
	if (check_capacity((char *)0)) return (0);

	/* One may write with finger, or weapon, or wand, or..., or...
	 * Edited by GAN 10/20/86 so as not to change weapon wielded.
	 */

/*JP	otmp = getobj(styluses, "write with");*/
	otmp = getobj(styluses, "使って書く");
	if(!otmp) return(0);		/* otmp == zeroobj if fingers */

	if (otmp == &zeroobj) writer = makeplural(body_part(FINGER));
	else writer = xname(otmp);

	/* There's no reason you should be able to write with a wand
	 * while both your hands are tied up.
	 */
	if (!freehand() && otmp != uwep && !otmp->owornmask) {
/*JP		You("have no free %s to write with!", body_part(HAND));*/
		pline("%sの自由が効かないので書けない！", body_part(HAND));
/*JP		You("have no free %s to write with!", body_part(HAND));*/
		return(0);
	}

	if (jello) {
/*JP		You("tickle %s with your %s.", mon_nam(u.ustuck), writer);
		Your("message dissolves...");*/
		You("%sで%sをくすぐった．", writer, mon_nam(u.ustuck));
		Your("メッセージは消えた．．．");
		return(0);
	}
	if (otmp->oclass != WAND_CLASS && !can_reach_floor()) {
/*JP		You_cant("reach the %s!", surface(u.ux,u.uy));*/
		You("%sに届かない！", surface(u.ux,u.uy));
		return(0);
	}
	if (IS_ALTAR(levl[u.ux][u.uy].typ)) {
/*JP		You("make a motion towards the altar with your %s.", writer);*/
		You("%sを使って祭壇に書こうとした．", writer);
		altar_wrath(u.ux, u.uy);
		return(0);
	}

	/* SPFX for items */

	switch (otmp->oclass) {
	    default:
	    case AMULET_CLASS:
	    case CHAIN_CLASS:
	    case POTION_CLASS:
	    case GOLD_CLASS:
		break;

	    case RING_CLASS:
		/* "diamond" rings and others should work */
	    case GEM_CLASS:
		/* diamonds & other hard gems should work */
		if (objects[otmp->otyp].oc_tough) {
			type = ENGRAVE;
			break;
		}
		break;

	    case ARMOR_CLASS:
		if (is_boots(otmp)) {
			type = DUST;
			break;
		}
		/* fall through */
	    /* Objects too large to engrave with */
	    case BALL_CLASS:
	    case ROCK_CLASS:
/*JP		You_cant("engrave with such a large object!");*/
		pline("そんな大きなものを使って文字を刻めない！");
		ptext = FALSE;
		break;

	    /* Objects too silly to engrave with */
	    case FOOD_CLASS:
	    case SCROLL_CLASS:
	    case SPBOOK_CLASS:
/*JP		Your("%s would get %s.", xname(otmp),
			is_ice(u.ux,u.uy) ? "all frosty" : "too dirty");*/
		Your("%sは%sなった．", xname(otmp),
			is_ice(u.ux,u.uy) ? "氷づけに" : "汚なく");
		ptext = FALSE;
		break;

	    case RANDOM_CLASS:	/* This should mean fingers */
		break;

	    /* The charge is removed from the wand before prompting for
	     * the engraving text, because all kinds of setup decisions
	     * and pre-engraving messages are based upon knowing what type
	     * of engraving the wand is going to do.  Also, the player
	     * will have potentially seen "You wrest .." message, and
	     * therefore will know they are using a charge.
	     */
	    case WAND_CLASS:
		if (zappable(otmp)) {
		    check_unpaid(otmp);
		    zapwand = TRUE;
		    if (Levitation) ptext = FALSE;

		    switch (otmp->otyp) {
		    /* DUST wands */
		    default:
			break;

			/* NODIR wands */
		    case WAN_LIGHT:
		    case WAN_SECRET_DOOR_DETECTION:
		    case WAN_CREATE_MONSTER:
		    case WAN_WISHING:
			zapnodir(otmp);
			break;

			/* IMMEDIATE wands */
			/* If wand is "IMMEDIATE", remember to affect the
			 * previous engraving even if turning to dust.
			 */
		    case WAN_STRIKING:
			Strcpy(post_engr_text,
/*JP			"The wand unsuccessfully fights your attempt to write!"*/
			"あなたが書こうとすると杖は抵抗した！"
			);
			break;
		    case WAN_SLOW_MONSTER:
			if (!Blind) {
			   Sprintf(post_engr_text,
/*JP				   "The bugs on the %s slow down!",*/
				   "%sの上の虫の動きが遅くなった！",
				   surface(u.ux, u.uy));
			}
			break;
		    case WAN_SPEED_MONSTER:
			if (!Blind) {
			   Sprintf(post_engr_text,
/*JP				   "The bugs on the %s speed up!",*/
				   "%sの上の虫の動きが速くなった！",
				   surface(u.ux, u.uy));
			}
			break;
		    case WAN_POLYMORPH:
			if(oep)  {
			    if (!Blind) {
				type = (xchar)0;	/* random */
				(void) random_engraving(buf);
			    }
			    dengr = TRUE;
			}
			break;
		    case WAN_NOTHING:
		    case WAN_UNDEAD_TURNING:
		    case WAN_OPENING:
		    case WAN_LOCKING:
		    case WAN_PROBING:
			break;

			/* RAY wands */
		    case WAN_MAGIC_MISSILE:
			ptext = TRUE;
			if (!Blind) {
			   Sprintf(post_engr_text,
/*JP				   "The %s is riddled by bullet holes!",*/
				   "%sは散弾で細い穴だらけになった！",
				   surface(u.ux, u.uy));
			}
			break;

		    /* can't tell sleep from death - Eric Backus */
		    case WAN_SLEEP:
		    case WAN_DEATH:
			if (!Blind) {
			   Sprintf(post_engr_text,
/*JP				   "The bugs on the %s stop moving!",*/
				   "%sの上の虫の動きが止まった！",
				   surface(u.ux, u.uy));
			}
			break;

		    case WAN_COLD:
			if (!Blind)
			    Strcpy(post_engr_text,
/*JP				"A few ice cubes drop from the wand.");*/
				"氷のかけらが杖からこぼれ落ちた．");
			if(!oep || (oep->engr_type != BURN))
			    break;
		    case WAN_CANCELLATION:
		    case WAN_MAKE_INVISIBLE:
			if(oep) {
			    if (!Blind)
/*JP				pline_The("engraving on the %s vanishes!",*/
				pline("%sの上の文字は消えた！",
					surface(u.ux,u.uy));
			    dengr = TRUE;
			}
			break;
		    case WAN_TELEPORTATION:
			if (oep) {
			    if (!Blind)
/*JP				pline_The("engraving on the %s vanishes!",*/
				pline("%sの上の文字は消えた！",
					surface(u.ux,u.uy));
			    teleengr = TRUE;
			}
			break;

		    /* type = ENGRAVE wands */
		    case WAN_DIGGING:
			ptext = TRUE;
			type  = ENGRAVE;
			if(!objects[otmp->otyp].oc_name_known) {
			    if (flags.verbose)
/*JP				pline("This %s is a wand of digging!",
					xname(otmp));*/
				pline("これは穴掘りの杖だ！");
			    doknown = TRUE;
			}
			if (!Blind)
			    Strcpy(post_engr_text,
				is_ice(u.ux,u.uy) ?
/*JP				"Ice chips fly up from the ice surface!" :
				"Gravel flies up from the floor.");*/
				"氷の表面から氷のかけらが飛び散った．" :
			        "砂利が床から飛び散った．");
			else
/*JP			    Strcpy(post_engr_text, "You hear drilling!");*/
			    Strcpy(post_engr_text, "穴が開く音を聞いた！");
			break;

		    /* type = BURN wands */
		    case WAN_FIRE:
			ptext = TRUE;
			type  = BURN;
			if(!objects[otmp->otyp].oc_name_known) {
			if (flags.verbose)
/*JP			    pline("This %s is a wand of fire!", xname(otmp));*/
			    pline("これは炎の杖だ！");
			    doknown = TRUE;
			}
			Strcpy(post_engr_text,
/*JP				Blind ? "You feel the wand heat up." :
					"Flames fly from the wand.");*/
				Hallucination ? "トカゲの尻尾が飛び出した！" :
				Blind ? "杖が暖かくなったような気がした．" :
					"炎が杖から飛び散った．");
			break;
		    case WAN_LIGHTNING:
			ptext = TRUE;
			type  = BURN;
			if(!objects[otmp->otyp].oc_name_known) {
			    if (flags.verbose)
/*JP				pline("This %s is a wand of lightning!",
					xname(otmp));*/
				pline("これは雷の杖だ！");
			    doknown = TRUE;
			}
			if (!Blind) {
			    Strcpy(post_engr_text,
/*JP				    "Lightning arcs from the wand.");*/
				    Hallucination ? "ベームベームが現われた！" :
				    "火花が杖から飛び散った．");
			    doblind = TRUE;
			} else
/*JP			    Strcpy(post_engr_text, "You hear crackling!");*/
			    Strcpy(post_engr_text, "パチパチという音を聞いた！");
			break;

		    /* type = MARK wands */
		    /* type = BLOOD wands */
		    }
		} else /* end if zappable */
		    if (!can_reach_floor()) {
/*JP			You_cant("reach the %s!", surface(u.ux,u.uy));*/
			You("%sに届かない！", surface(u.ux,u.uy));
			return(0);
		    }
		break;

	    case WEAPON_CLASS:
		if(is_blade(otmp)){
		    if ((int)otmp->spe > -3)
			type = ENGRAVE;
		    else
/*JP			Your("%s too dull for engraving.", aobjnam(otmp,"are"));*/
			pline("%sは刃がボロボロで，文字を彫れない．",xname(otmp));
		}
		break;

	    case TOOL_CLASS:
		if(otmp == ublindf) {
		    pline(
/*JP		"That is a bit difficult to engrave with, don't you think?");*/
		"ちょっとそれで彫るのは大変だろう，そう思わない？");
		    return(0);
		}
		switch (otmp->otyp)  {
		    case MAGIC_MARKER:
			if (otmp->spe <= 0)
/*JP			    Your("marker has dried out.");*/
			    Your("マーカは喝ききった．");
			else
			    type = MARK;
			break;
		    case TOWEL:
			/* Can't really engrave with a towel */
			ptext = FALSE;
			if (oep)
			    if ((oep->engr_type == DUST ) ||
				(oep->engr_type == BLOOD) ||
				(oep->engr_type == MARK )) {
				if (!Blind)
/*JP				    You("wipe out the message here.");*/
				    You("メッセージを拭きとった．");
				else
/*JP				    Your("%s gets %s.", xname(otmp),
					  is_ice(u.ux,u.uy) ?
					  "frosty" : "dusty");*/
				    pline("%sは%sなった．", xname(otmp),
					  is_ice(u.ux,u.uy) ?
					  "氷づけに" : "ほこりまみれに");
				dengr = TRUE;
			    } else
/*JP				Your("%s can't wipe out this engraving.",*/
				pline("この文字は%sでは拭きとれない．",
				     xname(otmp));
			else
/*JP			    Your("%s gets %s.", xname(otmp),
				  is_ice(u.ux,u.uy) ? "frosty" : "dusty");*/
			    pline("%sは%sなった．", xname(otmp),
				  is_ice(u.ux,u.uy) ? "氷づけに" : "ほこりまみれに");
			break;
		    default:
			break;
		}
		break;

	    case VENOM_CLASS:
#ifdef WIZARD
		if (wizard) {
/*JP		    pline("Writing a poison pen letter??");*/
		    pline("ふむ．これこそ本当の毒舌だ．");
		    break;
		}
#endif
	    case ILLOBJ_CLASS:
		impossible("You're engraving with an illegal object!");
		break;
	}

	/* End of implement setup */

	/* Identify stylus */
	if (doknown) {
	    makeknown(otmp->otyp);
	    more_experienced(0,10);
	}

	if (teleengr) {
	    rloc_engr(oep);
	    oep = (struct engr *)0;
	}

	if (dengr) {
	    del_engr(oep);
	    oep = (struct engr *)0;
	}

	/* Something has changed the engraving here */
	if (*buf) {
	    make_engr_at(u.ux, u.uy, buf, moves, type);
/*JP	    pline_The("engraving now reads: \"%s\".", buf);*/
	    pline("刻まれた文字を読んだ：「%s」．", buf);
	    ptext = FALSE;
	}

	if (zapwand && (otmp->spe < 0)) {
#if 0 /*JP*/
	    pline("%s %sturns to dust.",
		  The(xname(otmp)), Blind ? "" : "glows violently, then ");
 You("are not going to get anywhere trying to write in the %s with your dust.",
		is_ice(u.ux,u.uy) ? "frost" : "dust");
#endif
	    pline("%sは%s塵となった．",
		  The(xname(otmp)), Blind ? "" : "激しく輝き");
/*JP You("are not going to get anywhere trying to write in the %s with your dust.",
		is_ice(u.ux,u.uy) ? "frost" : "dust");*/
	    You("塵で%sに何か書こうとしたが，できなかった．",
		is_ice(u.ux,u.uy) ? "氷" : "ほこり");
	    useup(otmp);
	    ptext = FALSE;
	}

	if (!ptext) {		/* Early exit for some implements. */
	    if (otmp->oclass == WAND_CLASS && !can_reach_floor())
/*JP		You_cant("reach the %s!", surface(u.ux,u.uy));*/
		You("%sに届かない！", surface(u.ux,u.uy));
	    return(1);
	}

	/* Special effects should have deleted the current engraving (if
	 * possible) by now.
	 */

	if (oep) {
	    register char c = 'n';

	    /* Give player the choice to add to engraving. */

	    if ( (type == oep->engr_type) && (!Blind ||
		 (oep->engr_type == BURN) || (oep->engr_type == ENGRAVE)) ) {
/*JP		c = yn_function("Do you want to add to the current engraving?",*/
	    	c = yn_function("何か書き加えますか？",
				ynqchars, 'y');
		if (c == 'q') {
/*JP		    pline("Never mind.");*/
		    pline("へ？");
		    return(0);
		}
	    }

	    if (c == 'n' || Blind){

		if( (oep->engr_type == DUST) || (oep->engr_type == BLOOD) ||
		    (oep->engr_type == MARK) ) {
		    if (!Blind) {
/*JP			You("wipe out the message that was %s here.",
			    ((oep->engr_type == DUST)  ? "written in the dust" :
			    ((oep->engr_type == BLOOD) ? "scrawled in blood"   :
							 "written")));*/
			You("%sメッセージを拭きとった．",
			    ((oep->engr_type == DUST)  ? "ほこりに書かれている" :
			    ((oep->engr_type == BLOOD) ? "血文字でなぐり書きされている"   :
							 "書かれている")));
			del_engr(oep);
			oep = (struct engr *)0;
		    } else
		   /* Don't delete engr until after we *know* we're engraving */
			eow = TRUE;
		} else
		    if ( (type == DUST) || (type == MARK) || (type == BLOOD) ) {
/*JP			You(
			 "cannot wipe out the message that is %s the %s here.",
			 oep->engr_type == BURN ?
			   (is_ice(u.ux,u.uy) ? "melted into" : "burned into") :
			   "engraved in", surface(u.ux,u.uy));*/
			You("%sに%sメッセージを拭きとれなかった．",
			    surface(u.ux, u.uy),
			    oep->engr_type == BURN ?
			    (is_ice(u.ux,u.uy) ? "刻まれている" : "燃えている") :
			   "刻まれている");
			return(1);
		    } else
			if ( (type != oep->engr_type) || (c == 'n') ) {
			    if (!Blind || can_reach_floor())
/*JP				You("will overwrite the current message.");*/
				You("メッセージを上書きしようとした．");
			    eow = TRUE;
			}
	    }
	}

	eloc = surface(u.ux,u.uy);
	switch(type){
	    default:
/*JP		everb = (oep && !eow ? "add to the weird writing on" :
				       "write strangely on");*/
		everb = (oep && !eow ? "奇妙な文字列に書き加える" :
				       "奇妙な文字列を書く");
		break;
	    case DUST:
/*JP		everb = (oep && !eow ? "add to the writing in" :
				       "write in");*/
		everb = (oep && !eow ? "書き加える" :
				       "書く");
/*JP		eloc = is_ice(u.ux,u.uy) ? "frost" : "dust";*/
		eloc = is_ice(u.ux,u.uy) ? "氷" : "ほこり";
		break;
	    case ENGRAVE:
/*JP		everb = (oep && !eow ? "add to the engraving in" :
				       "engrave in");*/
		everb = (oep && !eow ? "刻み加える" :
				       "刻む");
		break;
	    case BURN:
		everb = (oep && !eow ?
/*JP			( is_ice(u.ux,u.uy) ? "add to the text melted into" :
					      "add to the text burned into") :
			( is_ice(u.ux,u.uy) ? "melt into" : "burn into"));*/
			( is_ice(u.ux,u.uy) ? "刻み加える" :
			                      "燃えている文字に書き加える") :
			( is_ice(u.ux,u.uy) ? "刻む" : "焼印をいれる"));
		break;
	    case MARK:
/*JP		everb = (oep && !eow ? "add to the graffiti on" :
				       "scribble on");*/
		everb = (oep && !eow ? "落書に書き加える" :
				       "はしり書きする");
		break;
	    case BLOOD:
/*JP		everb = (oep && !eow ? "add to the scrawl on" :
				       "scrawl on");*/
		everb = (oep && !eow ? "なぐり書きに書き加える" :
				       "なぐり書きする");
		break;
	}

	/* Tell adventurer what is going on */
	if (otmp != &zeroobj)
/*JP	    You("%s the %s with %s.", everb, eloc, doname(otmp));*/
	    You("%sで%sに%s", doname(otmp), eloc, jconj(everb,"た"));
	else
/*JP	    You("%s the %s with your %s.", everb, eloc,
		makeplural(body_part(FINGER)));*/
	    You("%sで%sに%s", makeplural(body_part(FINGER)),
		eloc, jconj(everb,"た"));

	/* Prompt for engraving! */
/*JP	Sprintf(qbuf,"What do you want to %s the %s here?", everb, eloc);*/
	Sprintf(qbuf,"%sに何と%s？", eloc, jconj(everb,"ますか"));
	getlin(qbuf, ebuf);

	/* Mix up engraving if surface or state of mind is unsound.  */
	/* Original kludge by stewr 870708.  modified by njm 910722. */
	for (sp = ebuf; *sp; sp++)
	    if ( ((type == DUST || type == BLOOD) && !rn2(25)) ||
		 (Blind   && !rn2(9)) || (Confusion     && !rn2(12)) ||
		 (Stunned && !rn2(4)) || (Hallucination && !rn2(1)) ){
/*JP*/
	      	if(!mb_is_boundary(ebuf, sp-ebuf))
                  /* sp landed inside a character; act on the whole of it.
                     This was "sp-1", right only for a two-byte one. */
                  jrndm_replace((char *)mb_prev(ebuf, sp));
                else if(mb_seqlen(sp) > 1)
		  jrndm_replace(sp);
		else
		  *sp = '!' + rn2(93); /* ASCII-code only */
	    }

	/* Count the actual # of chars engraved not including spaces */
	len = strlen(ebuf);

/*JP	for (sp = ebuf, spct = 0; *sp; sp++) if (isspace(*sp)) spct++;*/
	for (sp = ebuf, spct = 0; *sp; sp++) if (isspace_8(*sp)) spct++;

	if ( (len == spct) || index(ebuf, '\033') ) {
	    if (zapwand) {
		if (!Blind)
/*JP		    pline("%s glows, then fades.", The(xname(otmp)));*/
		    pline("%sは輝いたが，すぐに消えた．", The(xname(otmp)));
		return(1);
	    } else {
/*JP		pline("Never mind.");*/
		pline("へ？");
		return(0);
	    }
	}

	len -= spct;

	/* Previous engraving is overwritten */
	if (eow) {
	    del_engr(oep);
	    oep = (struct engr *)0;
	}

	/* Figure out how long it took to engrave, and if player has
	 * engraved too much.
	 */
	switch(type){
	    default:
		multi = -(len/10);
/*JP		if (multi) nomovemsg = "You finish your weird engraving.";*/
		if (multi) nomovemsg = "あなたは奇妙な刻みを終えた．";
		break;
	    case DUST:
		multi = -(len/10);
/*JP		if (multi) nomovemsg = "You finish writing in the dust.";*/
		if (multi) nomovemsg = "あなたはほこりに書き終えた．";
		break;
	    case ENGRAVE:
		multi = -(len/10);
		if ((otmp->oclass == WEAPON_CLASS) &&
		    ((otmp->otyp != ATHAME) || otmp->cursed)) {
		    multi = -len;
		    maxelen = ((otmp->spe + 3) * 2) + 1;
			/* -2 = 3, -1 = 5, 0 = 7, +1 = 9, +2 = 11
			 * Note: this does not allow a +0 anything (except
			 *	 an athame) to engrave "Elbereth" all at once.
			 *	 However, you could now engrave "Elb", then
			 *	 "ere", then "th".
			 */
/*JP		    Your("%s dull.", aobjnam(otmp, "get"));*/
		    Your("%sは刃こぼれした．", xname(otmp));
		    if (len > maxelen) {
			multi = -maxelen;
			otmp->spe = -3;
		    } else
			if (len > 1) otmp->spe -= len >> 1;
			else otmp->spe -= 1; /* Prevent infinite engraving */
		} else
		    if ( (otmp->oclass == RING_CLASS) ||
			 (otmp->oclass == GEM_CLASS) )
			multi = -len;
/*JP		if (multi) nomovemsg = "You finish engraving.";*/
		if (multi) nomovemsg = "あなたは刻み終えた．";
		break;
	    case BURN:
		multi = -(len/10);
		if (multi)
		    nomovemsg = is_ice(u.ux,u.uy) ?
/*JP			"You finish melting your message into the ice.":
			"You finish burning your message into the floor.";*/
			"氷へメッセージを刻み終えた．":
			"炎へメッセージを燃印をいれ終えた．";
		break;
	    case MARK:
		multi = -(len/10);
		if ((otmp->oclass == TOOL_CLASS) &&
		    (otmp->otyp == MAGIC_MARKER)) {
		    maxelen = (otmp->spe) * 2; /* one charge / 2 letters */
		    if (len > maxelen) {
/*JP			Your("marker dries out.");*/
			Your("マーカは喝ききった．");
			otmp->spe = 0;
			multi = -(maxelen/10);
		    } else
			if (len > 1) otmp->spe -= len >> 1;
			else otmp->spe -= 1; /* Prevent infinite grafitti */
		}
/*JP		if (multi) nomovemsg = "You finish defacing the dungeon.";*/
		if (multi) nomovemsg = "あなたは迷宮への落書を書き終えた．";
		break;
	    case BLOOD:
		multi = -(len/10);
/*JP		if (multi) nomovemsg = "You finish scrawling.";*/
		if (multi) nomovemsg = "はしり書きを書き終えた．";
		break;
	}

	/* Chop engraving down to size if necessary */
	if (len > maxelen) {
	    for (sp = ebuf; (maxelen && *sp); sp++)
/*JP		if (!isspace(*sp)) maxelen--;*/
		if (!isspace_8(*sp)) maxelen--;
	    if (!maxelen && *sp) {
                /*JP
                 *      Cut back to a character boundary.  maxelen is a byte
                 *      budget -- it comes from the wand's charges and is
                 *      compared against strlen(ebuf) -- so the loop above
                 *      spends it in bytes; only where it lands has to be a
                 *      whole character.
                 */
                sp = ebuf + mb_trunc_bytes(ebuf, (int)(sp - ebuf));
		*sp = (char)0;
/*JP		if (multi) nomovemsg = "You cannot write any more.";*/
		if (multi) nomovemsg = "これ以上何も書けなかった．";
/*JP		You("only are able to write \"%s\"", ebuf);*/
		You("単に「%s」と書けただけだ．", ebuf);
	    }
	}

	/* Add to existing engraving */
	if (oep) Strcpy(buf, oep->engr_txt);

	(void) strncat(buf, ebuf, (BUFSZ - (int)strlen(buf) - 1));

	make_engr_at(u.ux, u.uy, buf, (moves - multi), type);

	if (post_engr_text[0]) pline(post_engr_text);

	if (doblind && !resists_blnd(&youmonst)) {
/*JP	    You("are blinded by the flash!");*/
	    You("まばゆい光で目がくらんだ！");
	    make_blinded((long)rnd(50),FALSE);
	}

	return(1);
}

void
save_engravings(fd, mode)
int fd, mode;
{
	register struct engr *ep = head_engr;
	register struct engr *ep2;
	unsigned no_more_engr = 0;

	while (ep) {
	    ep2 = ep->nxt_engr;
	    if (ep->engr_lth && ep->engr_txt[0] && perform_bwrite(mode)) {
		bwrite(fd, (genericptr_t)&(ep->engr_lth), sizeof(ep->engr_lth));
		bwrite(fd, (genericptr_t)ep, sizeof(struct engr) + ep->engr_lth);
	    }
	    if (release_data(mode))
		dealloc_engr(ep);
	    ep = ep2;
	}
	if (perform_bwrite(mode))
	    bwrite(fd, (genericptr_t)&no_more_engr, sizeof no_more_engr);
	if (release_data(mode))
	    head_engr = 0;
}

void
rest_engravings(fd)
int fd;
{
	register struct engr *ep;
	unsigned lth;

	head_engr = 0;
	while(1) {
		mread(fd, (genericptr_t) &lth, sizeof(unsigned));
		if(lth == 0) return;
		ep = newengr(lth);
		mread(fd, (genericptr_t) ep, sizeof(struct engr) + lth);
		ep->nxt_engr = head_engr;
		head_engr = ep;
		ep->engr_txt = (char *) (ep + 1);	/* Andreas Bormann */
		/* mark as finished for bones levels -- no problem for
		 * normal levels as the player must have finished engraving
		 * to be able to move again */
		ep->engr_time = moves;
	}
}

void
del_engr(ep)
register struct engr *ep;
{
	if (ep == head_engr) {
		head_engr = ep->nxt_engr;
	} else {
		register struct engr *ept;

		for (ept = head_engr; ept; ept = ept->nxt_engr)
		    if (ept->nxt_engr == ep) {
			ept->nxt_engr = ep->nxt_engr;
			break;
		    }
		if (!ept) {
		    impossible("Error in del_engr?");
		    return;
		}
	}
	dealloc_engr(ep);
}

/* randomly relocate an engraving */
void
rloc_engr(ep)
struct engr *ep;
{
	int tx, ty, tryct = 200;

	do  {
	    if (--tryct < 0) return;
	    tx = rn1(COLNO-3,2);
	    ty = rn2(ROWNO);
	} while (engr_at(tx, ty) ||
		!goodpos(tx, ty, (struct monst *)0, (struct permonst *)0));

	ep->engr_x = tx;
	ep->engr_y = ty;
}

#endif /* OVLB */

/*engrave.c*/
