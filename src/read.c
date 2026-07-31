/*	SCCS Id: @(#)read.c	3.2	96/05/26	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"

#define Your_Own_Role(mndx) (\
	Role_is('C') ? (mndx == PM_CAVEMAN || mndx == PM_CAVEWOMAN) :	\
	Role_is('P') ? (mndx == PM_PRIEST  || mndx == PM_PRIESTESS) :	\
	(mndx == u.umonster))

#ifdef OVLB

/* elven armor vibrates warningly when enchanted beyond a limit */
#define is_elven_armor(optr)	((optr)->otyp == ELVEN_LEATHER_HELM\
				|| (optr)->otyp == ELVEN_MITHRIL_COAT\
				|| (optr)->otyp == ELVEN_CLOAK\
				|| (optr)->otyp == ELVEN_SHIELD\
				|| (optr)->otyp == ELVEN_BOOTS)

boolean	known;

static NEARDATA const char readable[] =
		   { ALL_CLASSES, SCROLL_CLASS, SPBOOK_CLASS, 0 };
static const char all_count[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

static void FDECL(wand_explode, (struct obj *));
static void NDECL(do_class_genocide);
static void FDECL(stripspe,(struct obj *));
static void FDECL(p_glow1,(struct obj *));
static void FDECL(p_glow2,(struct obj *,const char *));
static void FDECL(randomize,(int *, int));
static void FDECL(forget_single_object, (int));
static void FDECL(forget, (int));

STATIC_PTR void FDECL(set_lit, (int,int,genericptr_t));

int
doread()
{
	register struct obj *scroll;
	register boolean confused;

	known = FALSE;
	if(check_capacity((char *)0)) return (0);
/*JP	scroll = getobj(readable, "read");*/
	scroll = getobj(readable, "読む");
	if(!scroll) return(0);

	/* outrumor has its own blindness check */
	if(scroll->otyp == FORTUNE_COOKIE) {
	    if(flags.verbose)
/*JP		You("break up the cookie and throw away the pieces.");*/
		You("クッキーを割り，かけらを投げすてた．");
	    outrumor(bcsign(scroll), TRUE);
	    useup(scroll);
	    return(1);
#ifdef TOURIST
	} else if (scroll->otyp == T_SHIRT) {
	    char buf[BUFSZ];

	    if (Blind) {
/*JP		You_cant("feel any Braille writing.");*/
		You("点字はどうも書いてないようだ．");
		return 0;
	    }
	    if(flags.verbose)
/*JP		pline("It reads:");*/
		pline("それを読んだ：");
/*JP	    Sprintf(buf,  "I explored the Dungeons of Doom, %s.",*/
	    Sprintf(buf,  "私は運命の迷宮を調査していた，%s",
		    Hallucination ?
			(scroll == uarmu ?
			    /* (force these two to have identical length) */
/*JP			    "and never did any laundry..." :*/
			    "それによるとクリーニング屋はないようだ．．．":
/*JP			    "and couldn't find my way out") :*/
			    "が出口を見失ってしまった．") :
/*JP			"but all I got was this lousy T-shirt");*/
			"が手に入れたのはきたないＴシャツだけだ．");
	    if (scroll->oeroded)
		wipeout_text(buf,
			(int)(strlen(buf) * scroll->oeroded / (2*MAX_ERODE)),
			     scroll->o_id ^ (unsigned)u.ubirthday);
/*JP	    pline("\"%s\"", buf);*/
	    pline("「%s」", buf);
	    return 1;
#endif	/* TOURIST */
	} else if (scroll->oclass != SCROLL_CLASS
		&& scroll->oclass != SPBOOK_CLASS) {
/*JP	    pline(silly_thing_to, "read");*/
	    pline(silly_thing_to, "読む");
	    return(0);
	} else if (Blind) {
	    const char *what = 0;
	    if (scroll->oclass == SPBOOK_CLASS)
/*JP		what = "mystic runes";*/
		what = "神秘的なルーン文字";
	    else if (!scroll->dknown)
/*JP		what = "formula on the scroll";*/
		what = "巻物の呪文";
	    if (what) {
/*JP		pline("Being blind, you cannot read the %s.", what);*/
		pline("目が見えないので，あなたは%sを読むことができない．", what);
		return(0);
	    }
	}

	confused = (Confusion != 0);
#ifdef MAIL
	if (scroll->otyp == SCR_MAIL) confused = FALSE;
#endif
	if(scroll->oclass == SPBOOK_CLASS) {
	    if(confused) {
/*JP		You("cannot grasp the meaning of this tome.");*/
		You("この本の意味を理解できない．");
		return(0);
	    } else
		return(study_book(scroll));
	}
#ifndef NO_SIGNAL
	scroll->in_use = TRUE;	/* scroll, not spellbook, now being read */
#endif
	if(scroll->otyp != SCR_BLANK_PAPER) {
	  if(Blind)
/*JP	    pline("As you pronounce the formula on it, the scroll disappears.");*/
	    pline("呪文を唱えると，巻物は消えた．");
	  else
/*JP	    pline("As you read the scroll, it disappears.");*/
	    pline("巻物を読むと，それは消えた．");
	  if(confused) {
	    if (Hallucination)
/*JP		pline("Being so trippy, you screw up...");*/
		pline("とてもへろへろなので，くしゃくしゃにしてしまった．．．");
	    else
/*JP		pline("Being confused, you mispronounce the magic words...");*/
		pline("混乱しているので，呪文を間違って唱えてしまった．．．");
	  }
	}
	if(!seffects(scroll))  {
		if(!objects[scroll->otyp].oc_name_known) {
		    if(known) {
			makeknown(scroll->otyp);
			more_experienced(0,10);
		    } else if(!objects[scroll->otyp].oc_uname)
			docall(scroll);
		}
		if(scroll->otyp != SCR_BLANK_PAPER)
			useup(scroll);
#ifndef NO_SIGNAL
		else scroll->in_use = FALSE;
#endif
	}
	return(1);
}

static void
stripspe(obj)
register struct obj *obj;
{
	if (obj->blessed) pline(nothing_happens);
	else {
		if (obj->spe > 0) {
		    obj->spe = 0;
		    if (obj->otyp == OIL_LAMP || obj->otyp == BRASS_LANTERN)
			obj->age = 0;
/*JP		    Your("%s vibrates briefly.",xname(obj));*/
		    Your("%sは小刻みに振動した．",xname(obj));
		} else pline(nothing_happens);
	}
}

static void
p_glow1(otmp)
register struct obj	*otmp;
{
/*JP	Your("%s %s briefly.", xname(otmp),
		Blind ? "vibrates" : "glows");*/
	Your("%sは小刻みに%s．", xname(otmp),
		Blind ? "振動した" : "輝いた");
}

static void
p_glow2(otmp,color)
register struct obj	*otmp;
register const char *color;
{
/*JP	Your("%s %s%s for a moment.",
		xname(otmp),
		Blind ? "vibrates" : "glows ",
		Blind ? (const char *)"" : hcolor(color));*/
	Your("%sは一瞬%s%s．",
		xname(otmp),
		Blind ? (const char *)"" : jconj_adj(hcolor(color)),
		Blind ? "振動した" : "輝いた");
}

/* Is the object chargeable?  For purposes of inventory display; it is */
/* possible to be able to charge things for which this returns FALSE. */
boolean
is_chargeable(obj)
struct obj *obj;
{
	if (obj->oclass == WAND_CLASS) return TRUE;
	/* known && !uname is possible after amnesia/mind flayer */
	if (obj->oclass == RING_CLASS)
	    return (boolean)(objects[obj->otyp].oc_charged &&
			(obj->known || objects[obj->otyp].oc_uname));
	if (obj->oclass == TOOL_CLASS)
	    return (boolean)(objects[obj->otyp].oc_charged);
	return FALSE; /* why are weapons/armor considered charged anyway? */
}

/*
 * recharge an object; curse_bless is -1 if the recharging implement
 * was cursed, +1 if blessed, 0 otherwise.
 */
void
recharge(obj, curse_bless)
struct obj *obj;
int curse_bless;
{
	register int n;
	boolean is_cursed, is_blessed;

	is_cursed = curse_bless < 0;
	is_blessed = curse_bless > 0;

	if (obj->oclass == WAND_CLASS) {
	    if (obj->otyp == WAN_WISHING) {
		if (obj->recharged) {	/* recharged once already? */
		    wand_explode(obj);
		    return;
		}
		if (is_cursed) stripspe(obj);
		else if (is_blessed) {
		    if (obj->spe != 3) {
			obj->spe = 3;
			p_glow2(obj,blue);
		    } else {
			wand_explode(obj);
			return;
		    }
		} else {
		    if (obj->spe < 3) {
			obj->spe++;
			p_glow2(obj,blue);
		    } else pline(nothing_happens);
		}
		obj->recharged = 1; /* another recharging disallowed */
	    } else {
		if (is_cursed) stripspe(obj);
		else if (is_blessed) {
		    if (objects[obj->otyp].oc_dir == NODIR) {
			n = rn1(5,11);
			if (obj->spe < n) obj->spe = n;
			else obj->spe++;
		    } else {
			n = rn1(5,4);
			if (obj->spe < n) obj->spe = n;
			else obj->spe++;
		    }
		    p_glow2(obj,blue);
		} else {
		    obj->spe++;
		    p_glow1(obj);
		}
	    }
	} else if (obj->oclass == RING_CLASS &&
					objects[obj->otyp].oc_charged) {
	    /* charging does not affect ring's curse/bless status */
	    int s = is_blessed ? rnd(3) : is_cursed ? -rnd(2) : 1;
	    boolean is_on = (obj == uleft || obj == uright);

	    /* destruction depends on current state, not adjustment */
	    if (obj->spe > rn2(7) || obj->spe <= -5) {
/*JP		Your("%s pulsates momentarily, then explodes!",*/
		Your("%sは一瞬脈動し，爆発した！",
		     xname(obj));
		if (is_on) Ring_gone(obj);
		s = rnd(3 * abs(obj->spe));	/* amount of damage */
		useup(obj);
/*JP		losehp(s, "exploding ring", KILLED_BY_AN);*/
		losehp(s, "指輪の爆発で", KILLED_BY_AN);
	    } else {
		long mask = is_on ? (obj == uleft ? LEFT_RING :
				     RIGHT_RING) : 0L;
/*JP		Your("%s spins %sclockwise for a moment.",
		     xname(obj), s < 0 ? "counter" : "");*/
		Your("%sは一瞬%s時計回りに回転した．",
		     xname(obj), s < 0 ? "反" : "");
		/* cause attributes and/or properties to be updated */
		if (is_on) Ring_off(obj);
		obj->spe += s;	/* update the ring while it's off */
		if (is_on) setworn(obj, mask), Ring_on(obj);
		/* oartifact: if a touch-sensitive artifact ring is
		   ever created the above will need to be revised  */
	    }
	} else {
	    switch(obj->otyp) {
	    case BELL_OF_OPENING:
		if (is_cursed) stripspe(obj);
		else if (is_blessed) obj->spe += rnd(3);
		else obj->spe += 1;
		if (obj->spe > 5) obj->spe = 5;
		break;
	    case MAGIC_MARKER:
		if (is_cursed) stripspe(obj);
		else if (obj->recharged) {
		    if (obj->spe < 3)
/*JP			Your("marker seems permanently dried out.");*/
			Your("マーカは完全に喝ききってしまった．");
		    else
			pline(nothing_happens);
		} else if (is_blessed) {
		    n = obj->spe;
		    if (n < 50) obj->spe = 50;
		    if (n >= 50 && n < 75) obj->spe = 75;
		    if (n >= 75) obj->spe += 10;
		    p_glow2(obj,blue);
		    obj->recharged = 1;
		} else {
		    if (obj->spe < 50) obj->spe = 50;
		    else obj->spe++;
		    p_glow2(obj,White);
		    obj->recharged = 1;
		}
		break;
	    case OIL_LAMP:
	    case BRASS_LANTERN:
		if (is_cursed) {
		    stripspe(obj);
		    if (obj->lamplit) {
			if (!Blind)
/*JP			    pline("%s goes out!", The(xname(obj)));*/
			    pline("%sは消えた！", The(xname(obj)));
			end_burn(obj, TRUE);
		    }
		} else if (is_blessed) {
		    obj->spe = 1;
		    obj->age = 1500;
		    p_glow2(obj,blue);
		} else {
		    obj->spe = 1;
		    obj->age += 750;
		    if (obj->age > 1500) obj->age = 1500;
		    p_glow1(obj);
		}
		break;
	    case CRYSTAL_BALL:
		if (is_cursed) stripspe(obj);
		else if (is_blessed) {
		    obj->spe = 6;
		    p_glow2(obj,blue);
		} else {
		    if (obj->spe < 5) {
			obj->spe++;
			p_glow1(obj);
		    } else pline(nothing_happens);
		}
		break;
	    case HORN_OF_PLENTY:
	    case BAG_OF_TRICKS:
	    case CAN_OF_GREASE:
		if (is_cursed) stripspe(obj);
		else if (is_blessed) {
		    if (obj->spe <= 10)
			obj->spe += rn1(10, 6);
		    else obj->spe += rn1(5, 6);
		    p_glow2(obj,blue);
		} else {
		    obj->spe += rnd(5);
		    p_glow1(obj);
		}
		break;
	    case MAGIC_FLUTE:
	    case MAGIC_HARP:
	    case FROST_HORN:
	    case FIRE_HORN:
	    case DRUM_OF_EARTHQUAKE:
		if (is_cursed) {
		    stripspe(obj);
		} else if (is_blessed) {
		    obj->spe += d(2,4);
		    p_glow2(obj,blue);
		} else {
		    obj->spe += rnd(4);
		    p_glow1(obj);
		}
		break;
	    default:
/*JP		You("have a feeling of loss.");*/
		You("脱力感を感じた．");
		break;
	    } /* switch */
	}
}


/* Forget known information about this object class. */
static void
forget_single_object(obj_id)
	int obj_id;
{
	objects[obj_id].oc_name_known = 0;
	objects[obj_id].oc_pre_discovered = 0;	/* a discovery when relearned */
	if (objects[obj_id].oc_uname) {
	    /* this only works if oc_name_known is false */
	    undiscover_object(obj_id);

	    free((genericptr_t)objects[obj_id].oc_uname);
	    objects[obj_id].oc_uname = 0;
	}
	/* clear & free object names from matching inventory items too? */
}


#if 0	/* here if anyone wants it.... */
/* Forget everything known about a particular object class. */
static void
forget_objclass(oclass)
	int oclass;
{
	int i;

	for (i=bases[oclass];
		i < NUM_OBJECTS && objects[i].oc_class==oclass; i++)
	    forget_single_object(i);
}
#endif


/* randomize the given list of numbers  0 <= i < count */
static void
randomize(indices, count)
	int *indices;
	int count;
{
	int i, iswap, temp;

	for (i = count - 1; i > 0; i--) {
	    if ((iswap = rn2(i + 1)) == i) continue;
	    temp = indices[i];
	    indices[i] = indices[iswap];
	    indices[iswap] = temp;
	}
}


/* Forget % of known objects. */
void
forget_objects(percent)
	int percent;
{
	int i, count;
	int indices[NUM_OBJECTS];

	if (percent == 0) return;
	if (percent <= 0 || percent > 100) {
	    impossible("forget_objects: bad percent %d", percent);
	    return;
	}

	for (count = 0, i = 1; i < NUM_OBJECTS; i++)
	    if (OBJ_DESCR(objects[i]) &&
		    (objects[i].oc_name_known || objects[i].oc_uname))
		indices[count++] = i;

	randomize(indices, count);

	/* forget first % of randomized indices */
	count = ((count * percent) + 50) / 100;
	for (i = 0; i < count; i++)
	    forget_single_object(indices[i]);
}


/* Forget some or all of map (depends on parameters). */
void
forget_map(howmuch)
	int howmuch;
{
	register int zx, zy;

	known = TRUE;
	for(zx = 0; zx < COLNO; zx++) for(zy = 0; zy < ROWNO; zy++)
	    if (howmuch & ALL_MAP || rn2(7)) {
		/* Zonk all memory of this location. */
		levl[zx][zy].seenv = 0;
		levl[zx][zy].waslit = 0;
		levl[zx][zy].glyph = cmap_to_glyph(S_stone);
	    }
}

/* Forget all traps on the level. */
void
forget_traps()
{
	register struct trap *trap;

	/* forget all traps (except the one the hero is in :-) */
	for (trap = ftrap; trap; trap = trap->ntrap)
	    if ((trap->tx != u.ux || trap->ty != u.uy) && (trap->ttyp != HOLE))
		trap->tseen = 0;
}

/*
 * Forget given % of all levels that the hero has visited and not forgotten,
 * except this one.
 */
void
forget_levels(percent)
	int percent;
{
	int i, count;
	xchar  maxl, this_lev;
	int indices[MAXLINFO];

	if (percent == 0) return;

	if (percent <= 0 || percent > 100) {
	    impossible("forget_levels: bad percent %d", percent);
	    return;
	}

	this_lev = ledger_no(&u.uz);
	maxl = maxledgerno();

	/* count & save indices of non-forgotten visited levels */
	for (count = 0, i = 0; i <= maxl; i++)
	    if ((level_info[i].flags & VISITED) &&
			!(level_info[i].flags & FORGOTTEN) && i != this_lev)
		indices[count++] = i;

	randomize(indices, count);

	/* forget first % of randomized indices */
	count = ((count * percent) + 50) / 100;
	for (i = 0; i < count; i++) {
	    level_info[indices[i]].flags |= FORGOTTEN;
	}
}

/*
 * Forget some things (e.g. after reading a scroll of amnesia).  When called,
 * the following are always forgotten:
 *
 *	- felt ball & chain
 *	- traps
 *	- part (6 out of 7) of the map
 *
 * Other things are subject to flags:
 *
 *	howmuch & ALL_MAP	= forget whole map
 *	howmuch & ALL_SPELLS	= forget all spells
 */
static void
forget(howmuch)
int howmuch;
{

	if (Punished) u.bc_felt = 0;	/* forget felt ball&chain */

	forget_map(howmuch);
	forget_traps();

	/* 1 in 3 chance of forgetting some levels */
	if (!rn2(3)) forget_levels(rn2(25));

	/* 1 in 3 chance of forgeting some objects */
	if (!rn2(3)) forget_objects(rn2(25));

	if (howmuch & ALL_SPELLS) losespells();
	/*
	 * Make sure that what was seen is restored correctly.  To do this,
	 * we need to go blind for an instant --- turn off the display,
	 * then restart it.  All this work is needed to correctly handle
	 * walls which are stone on one side and wall on the other.  Turning
	 * off the seen bits above will make the wall revert to stone,  but
	 * there are cases where we don't want this to happen.  The easiest
	 * thing to do is to run it through the vision system again, which
	 * is always correct.
	 */
	docrt();		/* this correctly will reset vision */
}

int
seffects(sobj)
register struct obj	*sobj;
{
	register int cval;
	register boolean confused = (Confusion != 0);
	register struct obj *otmp;

	if (objects[sobj->otyp].oc_magic)
		exercise(A_WIS, TRUE);		/* just for trying */
	switch(sobj->otyp) {
#ifdef MAIL
	case SCR_MAIL:
		known = TRUE;
		if (sobj->spe)
/*JP		    pline("This seems to be junk mail addressed to the finder of the Eye of Larn.");*/
		    pline("Eye of Larnのファインダーに宛てられたゴミメイルのようだ．");
		/* note to the puzzled: the game Larn actually sends you junk
		 * mail if you win!
		 */
		else readmail(sobj);
		break;
#endif
	case SCR_ENCHANT_ARMOR:
	    {
		register schar s;
		boolean special_armor;

		otmp = some_armor();
		if(!otmp) {
			strange_feeling(sobj,
/*JP					!Blind ? "Your skin glows then fades." :
					"Your skin feels warm for a moment.");*/
					!Blind ? "あなたの体は一瞬輝いた．" :
					"あなたの体は一瞬暖かくなった．");
			exercise(A_CON, !sobj->cursed);
			exercise(A_STR, !sobj->cursed);
			return(1);
		}
		if(confused) {
			otmp->oerodeproof = !(sobj->cursed);
			if(Blind) {
			    otmp->rknown = FALSE;
/*JP			    Your("%s feels warm for a moment.",*/
			    Your("%sは一瞬暖かくなった．",
				xname(otmp));
			} else {
			    otmp->rknown = TRUE;
/*JP			    Your("%s is covered by a %s %s %s!",
				xname(otmp),
				sobj->cursed ? "mottled" : "shimmering",
				hcolor(sobj->cursed ? Black : golden),
				sobj->cursed ? "glow" :
				  (is_shield(otmp) ? "layer" : "shield"));*/
			    Your("%sは%s%s%sで覆われた！",
				xname(otmp),
				jconj_adj(hcolor(sobj->cursed ? Black : golden)),
				sobj->cursed ? "光るまだらの" : "ゆらめく",
				sobj->cursed ? "輝き" :
				  (is_shield(otmp) ? "バリア" : "バリア"));
			}
			if (otmp->oerodeproof && otmp->oeroded) {
			    otmp->oeroded = 0;
/*JP			    Your("%s %ss good as new!",
				 xname(otmp), Blind ? "feel" : "look");*/
			    Your("%sは新品同様になったように%s！",
				 xname(otmp), Blind ? "感じた" : "見えた");
			}
			break;
		}
		special_armor = is_elven_armor(otmp) ||
#ifdef FIGHTER
				(Role_is('F') && otmp->otyp == SAILOR_BLOUSE) ||
#endif
				(Role_is('W') && otmp->otyp == CORNUTHAUM);
		if ((otmp->spe > (special_armor ? 5 : 3)) &&
		    rn2(otmp->spe) && !sobj->cursed) {
/*JP		Your("%s violently %s%s for a while, then evaporates.",
			    xname(otmp),
			    Blind ? "vibrates" : "glows ",
			    Blind ? nul : hcolor(silver));*/
		Your("%sはしばらくの間激しく%s%s，蒸発した．",
			    xname(otmp),
			    Blind ? nul : jconj_adj(hcolor(silver)),
			    Blind ? "振動し" : "輝き");

			if(is_cloak(otmp)) (void) Cloak_off();
			if(is_boots(otmp)) (void) Boots_off();
			if(is_helmet(otmp)) (void) Helmet_off();
			if(is_gloves(otmp)) (void) Gloves_off();
			if(is_shield(otmp)) (void) Shield_off();
			if(otmp == uarm) (void) Armor_gone();
			useup(otmp);
			break;
		}
		s = sobj->cursed ? -1 :
		    otmp->spe >= 9 ? (rn2(otmp->spe) == 0) :
		    sobj->blessed ? rnd(3-otmp->spe/3) : 1;
		if (s >= 0 && otmp->otyp >= GRAY_DRAGON_SCALES &&
					otmp->otyp <= YELLOW_DRAGON_SCALES) {
			/* dragon scales get turned into dragon scale mail */
/*JP			Your("%s merges and hardens!", xname(otmp));*/
			Your("%sは融合し固くなった！", xname(otmp));
			setworn((struct obj *)0, W_ARM);
			/* assumes same order */
			otmp->otyp = GRAY_DRAGON_SCALE_MAIL +
						otmp->otyp - GRAY_DRAGON_SCALES;
			otmp->cursed = 0;
			if (sobj->blessed) {
				otmp->spe++;
				otmp->blessed = 1;
			}
			otmp->known = 1;
			setworn(otmp, W_ARM);
			break;
		}
/*JP		Your("%s %s%s%s for a %s.",
			xname(otmp),
		        s == 0 ? "violently " : nul,
			Blind ? "vibrates" : "glows ",
			Blind ? nul : hcolor(sobj->cursed ? Black : silver),
			  (s*s>1) ? "while" : "moment");*/
		Your("%sは%s%s%s%s．",
			xname(otmp),
			(s*s>1) ? "しばらくの間" : "一瞬",
		        s == 0 ? "激しく" : nul,
			Blind ? nul : 
		     jconj_adj(hcolor(sobj->cursed ? Black : silver)),
			Blind ? "振動した" : "輝いた");
		otmp->cursed = sobj->cursed;
		if (!otmp->blessed || sobj->cursed)
			otmp->blessed = sobj->blessed;
		if (s) {
			otmp->spe += s;
			adj_abon(otmp, s);
			known = otmp->known;
		}

		if ((otmp->spe > (special_armor ? 5 : 3)) &&
		    (special_armor || !rn2(7)))
/*JP			Your("%s suddenly vibrates %s.",
				xname(otmp),
				Blind ? "again" : "unexpectedly");*/
			Your("%sは突然%s振動した．",
				xname(otmp),
				Blind ? "また" : "思いもよらず");
		break;
	    }
	case SCR_DESTROY_ARMOR:
	    {
		otmp = some_armor();
		if(confused) {
			if(!otmp) {
/*JP				strange_feeling(sobj,"Your bones itch.");*/
				strange_feeling(sobj,"骨がムズムズする．");
				exercise(A_STR, FALSE);
				exercise(A_CON, FALSE);
				return(1);
			}
			otmp->oerodeproof = sobj->cursed;
			p_glow2(otmp,purple);
			break;
		}
		if(!sobj->cursed || !otmp || !otmp->cursed) {
		    if(!destroy_arm(otmp)) {
/*JP			strange_feeling(sobj,"Your skin itches.");*/
			strange_feeling(sobj,"皮膚がムズムズする．");
			exercise(A_STR, FALSE);
			exercise(A_CON, FALSE);
			return(1);
		    } else
			known = TRUE;
		} else {	/* armor and scroll both cursed */
/*JP		    Your("%s vibrates.", xname(otmp));*/
		    Your("%sは振動した．", xname(otmp));
		    if (otmp->spe >= -6) otmp->spe--;
		    make_stunned(HStun + rn1(10, 10), TRUE);
		}
	    }
	    break;
	case SCR_CONFUSE_MONSTER:
	case SPE_CONFUSE_MONSTER:
		if(u.usym != S_HUMAN || sobj->cursed) {
/*JP			if(!HConfusion) You_feel("confused.");*/
			if(!HConfusion) You("混乱した．");
			make_confused(HConfusion + rnd(100),FALSE);
		} else  if(confused) {
		    if(!sobj->blessed) {
/*JP			Your("%s begin to %s%s.",
			    makeplural(body_part(HAND)),
			    Blind ? "tingle" : "glow ",
			    Blind ? nul : hcolor(purple));*/
			Your("%sは%s%sはじめた．",
			    makeplural(body_part(HAND)),
			    Blind ? nul : jconj_adj(hcolor(purple)),
			    Blind ? "ヒリヒリし" : "輝き");
			make_confused(HConfusion + rnd(100),FALSE);
		    } else {
/*JP			pline("A %s%s surrounds your %s.",
			    Blind ? nul : hcolor(red),
			    Blind ? "faint buzz" : " glow",
			    body_part(HEAD));*/
			pline("%s%sがあなたの%sを取り巻いた．",
			    Blind ? nul : jconj_adj(hcolor(red)),
			    Blind ? "かすかにブーンと鳴るもの" : "輝くもの",
  			    body_part(HEAD));
			make_confused(0L,TRUE);
		    }
		} else {
		    if (!sobj->blessed) {
/*JP			Your("%s%s %s%s.",
			makeplural(body_part(HAND)),
			Blind ? "" : " begin to glow",
			Blind ? (const char *)"tingle" : hcolor(red),
			u.umconf ? " even more" : "");*/
			Your("%sは%s%s%s．",
			makeplural(body_part(HAND)),
			u.umconf ? "少し" : "",
			Blind ? (const char *)"ヒリヒリした" : 
			     jconj_adj(hcolor(red)),
			Blind ? "" : "輝きはじめた");
			u.umconf++;
		    } else {
			if (Blind)
/*JP			    Your("%s tingle %s sharply.",
				makeplural(body_part(HAND)),
				u.umconf ? "even more" : "very");*/
			    Your("%sは%sピリピリする．",
				makeplural(body_part(HAND)),
				u.umconf ? "少し" : "とても");
			else
/*JP			    Your("%s glow a%s brilliant %s.",
				makeplural(body_part(HAND)),
				u.umconf ? "n even more" : "",
				hcolor(red));*/
			    Your("%sは%s%s明るく輝いた．",
				makeplural(body_part(HAND)),
				u.umconf ? "少し" : "",
				jconj_adj(hcolor(red)));
			u.umconf += rn1(8, 2);
		    }
		}
		break;
	case SCR_SCARE_MONSTER:
	case SPE_CAUSE_FEAR:
	    {	register int ct = 0;
		register struct monst *mtmp;

		for(mtmp = fmon; mtmp; mtmp = mtmp->nmon)
		    if(cansee(mtmp->mx,mtmp->my)) {
			if(confused || sobj->cursed) {
			    mtmp->mflee = mtmp->mfrozen = mtmp->msleep = 0;
			    mtmp->mcanmove = 1;
			} else
			    if (! resist(mtmp, sobj->oclass, 0, NOTELL))
				mtmp->mflee = 1;
			if(!mtmp->mtame) ct++;	/* pets don't laugh at you */
		    }
		if(!ct)
/*JP		      You_hear("%s in the distance.",
			       (confused || sobj->cursed) ? "sad wailing" :
							"maniacal laughter");*/
		      You("遠くで%sを聞いた．",
			       (confused || sobj->cursed) ? "悲しく泣き叫ぶ声" :
							"狂ったように笑う声");
		else if(sobj->otyp == SCR_SCARE_MONSTER)
/*JP			You_hear("%s close by.",
				  (confused || sobj->cursed) ? "sad wailing" :
						 "maniacal laughter");*/
			You("近くで%sを聞いた．",
				  (confused || sobj->cursed) ? "悲しく泣き叫ぶ声" :
						 "狂ったように笑う声");
		break;
	    }
	case SCR_BLANK_PAPER:
	    if (Blind)
/*JP		You("don't remember there being any magic words on this scroll.");*/
		You("巻物に呪文が書いてなかったことを思いだした．");
	    else
/*JP		pline("This scroll seems to be blank.");*/
		pline("この巻物には何も書いてないように見える．");
	    known = TRUE;
	    break;
	case SCR_REMOVE_CURSE:
	case SPE_REMOVE_CURSE:
	    {	register struct obj *obj;
		if(confused)
		    if (Hallucination)
/*JP			You_feel("the power of the Force against you!");*/
			You("魔の力があなたにはむかっているように感じた！");
		    else
/*JP			You_feel("like you need some help.");*/
			You("自分が助けを必要としているような気がした．");
		else
		    if (Hallucination)
/*JP			You_feel("in touch with the Universal Oneness.");*/
			You("宇宙原理の調和に触れているような気がした．");
		    else
/*JP			You_feel("like someone is helping you.");*/
			pline("誰かがあなたを助けているような気がした．");

/*JP		if(sobj->cursed) pline_The("scroll disintegrates.");*/
		if(sobj->cursed) pline("巻物は粉々になった．");
		else {
		    for(obj = invent; obj ; obj = obj->nobj)
			if(sobj->blessed || obj->owornmask ||
			   (obj->otyp == LOADSTONE)) {
			    if(confused) blessorcurse(obj, 2);
			    else uncurse(obj);
			}
		}
		if(Punished && !confused) unpunish();
		break;
	    }
	case SCR_CREATE_MONSTER:
	case SPE_CREATE_MONSTER:
	    if (create_critters(1 + ((confused || sobj->cursed) ? 12 : 0) +
				((sobj->blessed || rn2(73)) ? 0 : rnd(4)),
			confused ? &mons[PM_ACID_BLOB] : (struct permonst *)0))
		known = TRUE;
	    /* no need to flush monsters; we ask for identification only if the
	     * monsters are not visible
	     */
	    break;
	case SCR_ENCHANT_WEAPON:
		if(uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep))
			&& confused) {
		/* oclass check added 10/25/86 GAN */
			uwep->oerodeproof = !(sobj->cursed);
			if (Blind) {
			    uwep->rknown = FALSE;
/*JP			    Your("weapon feels warm for a moment.");*/
			    pline("武器が一瞬暖かくなったような気がした．");
			} else {
			    uwep->rknown = TRUE;
/*JP			    Your("%s covered by a %s %s %s!",
				aobjnam(uwep, "are"),
				sobj->cursed ? "mottled" : "shimmering",
				hcolor(sobj->cursed ? purple : golden),
				sobj->cursed ? "glow" : "shield");*/
			    Your("%sは%s%s%sで覆われた！",
				xname(uwep),
				jconj_adj(hcolor(
				  sobj->cursed ? purple : golden)),
				sobj->cursed ? "光るまだらの" : "ゆらめく",
				sobj->cursed ? "輝き" : "バリア");

			}
			if (uwep->oerodeproof && uwep->oeroded) {
			    uwep->oeroded = 0;
/*JP			    Your("%s good as new!",
				 aobjnam(uwep, Blind ? "feel" : "look"));*/
			    pline("%sは新品同様になったように%s！",
				 xname(uwep), Blind ? "感じた" : "見えた");
			}
		} else return !chwepon(sobj,
				       sobj->cursed ? -1 :
				       !uwep ? 1 :
				       uwep->spe >= 9 ? (rn2(uwep->spe) == 0) :
				       sobj->blessed ? rnd(3-uwep->spe/3) : 1);
		break;
	case SCR_TAMING:
	case SPE_CHARM_MONSTER:
	    {	register int i,j;
		register int bd = confused ? 5 : 1;
		register struct monst *mtmp;

		for(i = -bd; i <= bd; i++) for(j = -bd; j <= bd; j++)
		if(isok(u.ux+i, u.uy+j) && (mtmp = m_at(u.ux+i, u.uy+j))) {
		    if(sobj->cursed) {
			setmangry(mtmp);
		    } else {
			if (mtmp->isshk)
			    make_happy_shk(mtmp, FALSE);
			else if (!resist(mtmp, sobj->oclass, 0, NOTELL))
			    (void) tamedog(mtmp, (struct obj *) 0);
		    }
		}
		break;
	    }
	case SCR_GENOCIDE:
/*JP		You("have found a scroll of genocide!");*/
		pline("これは虐殺の巻物だ！");
		known = TRUE;
		if (sobj->blessed) do_class_genocide();
		else do_genocide(!sobj->cursed | (2 * !!Confusion));
		break;
	case SCR_LIGHT:
		if(!Blind) known = TRUE;
		litroom(!confused && !sobj->cursed, sobj);
		break;
	case SCR_TELEPORTATION:
		if(confused || sobj->cursed) level_tele();
		else {
			if (sobj->blessed && !Teleport_control) {
				known = TRUE;
/*JP				if (yn("Do you wish to teleport?")=='n')*/
				if (yn("瞬間移動しますか？")=='n')
					break;
			}
			tele();
			if(Teleport_control || !couldsee(u.ux0, u.uy0) ||
			   (distu(u.ux0, u.uy0) >= 16))
				known = TRUE;
		}
		break;
	case SCR_GOLD_DETECTION:
		if (confused || sobj->cursed) return(trap_detect(sobj));
		else return(gold_detect(sobj));
	case SCR_FOOD_DETECTION:
	case SPE_DETECT_FOOD:
		if (food_detect(sobj))
			return(1);	/* nothing detected */
		break;
	case SPE_IDENTIFY:
		cval = rn2(5);
		goto id;
	case SCR_IDENTIFY:
		/* known = TRUE; */
		if(confused)
/*JP			You("identify this as an identify scroll.");*/
			You("これは識別の巻物だと識別した．");
		else
/*JP			pline("This is an identify scroll.");*/
			pline("これは識別の巻物だ．");
		if (sobj->blessed || (!sobj->cursed && !rn2(5))) {
			cval = rn2(5);
			/* Note: if rn2(5)==0, identify all items */
			if (cval == 1 && sobj->blessed && Luck > 0) ++cval;
		} else	cval = 1;
		useup(sobj);
		makeknown(SCR_IDENTIFY);
	id:
		if(invent && !confused) {
		    identify_pack(cval);
		}
		return(1);
	case SCR_CHARGING:
		if (confused) {
/*JP		    You_feel("charged up!");*/
		    You("充填されたような気がする！");
		    if (u.uen < u.uenmax)
			u.uen = u.uenmax;
		    else
			u.uen = (u.uenmax += d(5,4));
		    flags.botl = 1;
		    break;
		}
		known = TRUE;
/*JP		pline("This is a charging scroll.");
		otmp = getobj(all_count, "charge");*/
		pline("これは充填の巻物だ！");
		otmp = getobj(all_count, "充填する");
		if (!otmp) break;
		recharge(otmp, sobj->cursed ? -1 : (sobj->blessed ? 1 : 0));
		break;
	case SCR_MAGIC_MAPPING:
		if (level.flags.nommap) {
/*JP		    Your("mind is filled with crazy lines!");*/
		    Your("心にただならぬ線！線！線！が浮びあがった！");
		    if (Hallucination)
/*JP			pline("Wow!  Modern art.");*/
			pline("ワォ！モダンアートだ！");
		    else
/*JP			Your("head spins in bewilderment.");*/
			You("当惑して目がまわった．");
		    make_confused(HConfusion + rnd(30), FALSE);
		    break;
		}
		known = TRUE;
	case SPE_MAGIC_MAPPING:
		if (level.flags.nommap) {
/*JP		    Your("head spins as something blocks the spell!");*/
		    You("何かが呪文をさえぎり，目がまわった．！");
		    make_confused(HConfusion + rnd(30), FALSE);
		    break;
		}
/*JP		pline("A map coalesces in your mind!");*/
		pline("地図があなたの心に融合した！");
		cval = (sobj->cursed && !confused);
		if(cval) HConfusion = 1;	/* to screw up map */
		do_mapping();
		if(cval) {
		    HConfusion = 0;		/* restore */
/*JP		    pline("Unfortunately, you can't grasp the details.");*/
		    pline("不幸にも，あなたは詳細を得ることができなかった．");
		}
		break;
	case SCR_AMNESIA:
		known = TRUE;
		forget(	(!sobj->blessed ? ALL_SPELLS : 0) |
			(!confused || sobj->cursed ? ALL_MAP : 0) );
		if (Hallucination) /* Ommmmmm! */
/*JP			Your("mind releases itself from mundane concerns.");*/
			Your("心は平凡な利害関係から解放された．");
		else if (!strncmpi(plname, "Maud", 4))
/*JP			pline("As your mind turns inward on itself, you forget everything else.");*/
			pline("あなたの心は内側に向き，全てを忘れてしまった．");
		else if (rn2(2))
/*JP			pline("Who was that Maud person anyway?");*/
			pline("Maudって娘はいったい誰だい？");
		else
/*JP			pline("Thinking of Maud you forget everything else.");*/
			pline("Maudを考えること以外，あなたは全てを忘れてしまった．");
		exercise(A_WIS, FALSE);
		break;
	case SCR_FIRE:
		/*
		 * Note: Modifications have been made as of 3.0 to allow for
		 * some damage under all potential cases.
		 */
		cval = bcsign(sobj);
		useup(sobj);
		makeknown(SCR_FIRE);
		if(confused) {
		    if(Fire_resistance) {
			shieldeff(u.ux, u.uy);
			if(!Blind)
/*JP			    pline("Oh, look, what a pretty fire in your %s.",*/
			    pline("わぁごらん．小さな火が%sにある．",
				makeplural(body_part(HAND)));
/*JP			else You_feel("a pleasant warmth in your %s.",*/
			else You("%sの中に快適な暖かさを感じた．",
				makeplural(body_part(HAND)));
		    } else {
/*JP			pline_The("scroll catches fire and you burn your %s.",*/
			pline("巻物に火が燃えうつり，あなたの%sを焼いた．",
				makeplural(body_part(HAND)));
/*JP			losehp(1, "scroll of fire", KILLED_BY_AN);*/
			losehp(1, "炎の巻物で", KILLED_BY_AN);
		    }
		    return(1);
		}
		if (Underwater)
/*JP			pline_The("water around you vaporizes violently!");*/
			pline("あなたの回りの水は沸騰した！");
		else
/*JP			pline_The("scroll erupts in a tower of flame!");*/
			pline("巻物から火柱が立ち昇った！");
		explode(u.ux, u.uy, 11, (2*(rn1(3, 3) + 2 * cval) + 1)/3,
							SCROLL_CLASS);
		return(1);
	case SCR_PUNISHMENT:
		known = TRUE;
		if(confused || sobj->blessed) {
/*JP			You_feel("guilty.");*/
			You("罪を感じた．");
			break;
		}
		punish(sobj);
		break;
#ifdef JPEXTENSION
	case SCR_DESTROY_WEAPON:
	    otmp = uwep;
	    if(confused) {
		if(!otmp) {
		    strange_feeling(sobj,"骨がムズムズする．");
		    exercise(A_STR, FALSE);
		    exercise(A_CON, FALSE);
		    return(1);
		}
		otmp->oerodeproof = sobj->cursed;
		p_glow2(otmp,purple);
		break;
	    }
	    
	    if(!otmp){
		pline("%sがムズムズした．", body_part(HAND));
	    }
	    else if(!sobj->cursed || !otmp || !otmp->cursed) {
		known = TRUE;
		useup(otmp);
		Your("%sは塵となってしまった．", xname(otmp));
	    }
	    
	    break;
	case SCR_CREATE_CREATE_SCROLL:
	    pline("足元に巻物が突如出現した．");
	    known = FALSE;
	    otmp = mksobj_at(SCR_CREATE_CREATE_SCROLL, u.ux, u.uy, FALSE);
	    otmp->cursed = sobj->cursed;

	    break;
	case SCR_CREATE_ALTAR:
	    if(levl[u.ux][u.uy].typ != ROOM)
		break;

	    pline("煙が立ちのぼり祭壇が出現した．");
	    known = TRUE;
	    levl[u.ux][u.uy].typ = ALTAR;
	    levl[u.ux][u.uy].altarmask = Align2amask(A_NONE);
	    newsym(u.ux, u.uy);
	    break;
	case SCR_CREATE_SINK:
	    if(levl[u.ux][u.uy].typ != ROOM)
		break;

	    pline("煙が立ちのぼり流し台が出現した．");
	    known = TRUE;
	    levl[u.ux][u.uy].typ = SINK;
	    newsym(u.ux, u.uy);
	    break;
	case SCR_CREATE_TRAP:
	    if(levl[u.ux][u.uy].typ != ROOM)
		break;

	    pline("煙が立ちのぼった．");
	    known = FALSE;
	    maketrap(u.ux, u.uy, rn2(TRAPNUM) + NO_TRAP + 1);
	    break;
	case SCR_SYMMETRY:
	    You("鏡の中に入ったような気がした．");
	    known = TRUE;
	    make_totter(Totter + rnd(500), FALSE);
	    break;
#endif
	default:
		impossible("What weird effect is this? (%u)", sobj->otyp);
	}
	return(0);
}

static void
wand_explode(obj)
register struct obj *obj;
{
/*JP    Your("%s vibrates violently, and explodes!",xname(obj));*/
    Your("%sは激しく振動し，爆発した！",xname(obj));
    nhbell();
/*JP    losehp(rnd(2*(u.uhpmax+1)/3), "exploding wand", KILLED_BY_AN);*/
    losehp(rnd(2*(u.uhpmax+1)/3), "杖の爆発で", KILLED_BY_AN);
    useup(obj);
    exercise(A_STR, FALSE);
}

/*
 * Low-level lit-field update routine.
 */
STATIC_PTR void
set_lit(x,y,val)
int x, y;
genericptr_t val;
{
	if (val)
	    levl[x][y].lit = 1;
	else {
	    levl[x][y].lit = 0;
	    snuff_light_source(x, y);
	}
}

void
litroom(on,obj)
register boolean on;
struct obj *obj;
{
	char is_lit;	/* value is irrelevant; we use its address
			   as a `not null' flag for set_lit() */

	/* first produce the text (provided you're not blind) */
	if(!on) {
		register struct obj *otmp;

		if (!Blind) {
		    if(u.uswallow) {
/*JP			pline("It seems even darker in here than before.");*/
			pline("前より暗くなったように見える．");
			return;
		    }
/*JP		    You("are surrounded by darkness!");*/
		    You("暗闇に覆われた！");
		}

		/* the magic douses lamps, et al, too */
		for(otmp = invent; otmp; otmp = otmp->nobj)
		    if (otmp->lamplit)
			(void) snuff_lit(otmp);
		if (Blind) goto do_it;
	} else {
		if (Blind) goto do_it;
		if(u.uswallow){
			if (is_animal(u.ustuck->data))
/*JP				pline("%s stomach is lit.",*/
				pline("%sの胃は明るくなった．",
				         s_suffix(Monnam(u.ustuck)));
			else
				if (is_whirly(u.ustuck->data))
/*JP					pline("%s shines briefly.",*/
					pline("%sはちょっと輝いた．",
					      Monnam(u.ustuck));
				else
/*JP					pline("%s glistens.", Monnam(u.ustuck));*/
					pline("%sはきらきら輝いた．", Monnam(u.ustuck));
			return;
		}
/*JP		pline("A lit field surrounds you!");*/
		pline("灯りがあなたを取り囲んだ！");
	}

do_it:
	/* No-op in water - can only see the adjacent squares and that's it! */
	if (Underwater || Is_waterlevel(&u.uz)) return;
	/*
	 *  If we are darkening the room and the hero is punished but not
	 *  blind, then we have to pick up and replace the ball and chain so
	 *  that we don't remember them if they are out of sight.
	 */
	if (Punished && !on && !Blind)
	    move_bc(1, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);

#ifdef REINCARNATION
	if (Is_rogue_level(&u.uz)) {
	    /* Can't use do_clear_area because MAX_RADIUS is too small */
	    /* rogue lighting must light the entire room */
	    int rnum = levl[u.ux][u.uy].roomno - ROOMOFFSET;
	    int rx, ry;
	    if(rnum >= 0) {
		for(rx = rooms[rnum].lx-1; rx <= rooms[rnum].hx+1; rx++)
		    for(ry = rooms[rnum].ly-1; ry <= rooms[rnum].hy+1; ry++)
			set_lit(rx, ry,
				(genericptr_t)(on ? &is_lit : (char *)0));
		rooms[rnum].rlit = on;
	    }
	    /* hallways remain dark on the rogue level */
	} else
#endif
	    do_clear_area(u.ux,u.uy,
		(obj && obj->oclass==SCROLL_CLASS && obj->blessed) ? 9 : 5,
		set_lit, (genericptr_t)(on ? &is_lit : (char *)0));

	/*
	 *  If we are not blind, then force a redraw on all positions in sight
	 *  by temporarily blinding the hero.  The vision recalculation will
	 *  correctly update all previously seen positions *and* correctly
	 *  set the waslit bit [could be messed up from above].
	 */
	if (!Blind) {
	    vision_recalc(2);

	    /* replace ball&chain */
	    if (Punished && !on)
		move_bc(0, 0, uball->ox, uball->oy, uchain->ox, uchain->oy);
	}

	vision_full_recalc = 1;	/* delayed vision recalculation */
}

static void
do_class_genocide()
{
	register int i, j, immunecnt, gonecnt, goodcnt, class;
	char buf[BUFSZ];

	for(j=0; ; j++) {
		if (j >= 5) {
			pline(thats_enough_tries);
			return;
		}
		do {
/*JP		    getlin("What class of monsters do you wish to genocide?",*/
	    getlin("どのクラスに属する怪物を虐殺しますか？[文字を入れてね]",
			buf);
		} while (buf[0]=='\033' || !buf[0]);
		if (strlen(buf) == 1)
		    class = def_char_to_monclass(buf[0]);
		else
		    class = 0;
		immunecnt = gonecnt = goodcnt = 0;
		for (i = LOW_PM; i < NUMMONS; i++) {
		    if (class ? mons[i].mlet == class :
			    strstri(monexplain[(int)mons[i].mlet],
				    makesingular(buf)) != 0) {
			class = mons[i].mlet;
			if (!(mons[i].geno & G_GENO)) immunecnt++;
			else if(mvitals[i].mvflags & G_GENOD) gonecnt++;
			else goodcnt++;
		    }
		}
		if (!goodcnt && class != S_HUMAN) {
			if (gonecnt)
/*JP	pline("All such monsters are already nonexistent.");*/
	pline("その怪物はもういない．");
			else if (immunecnt)
/*JP	You("aren't permitted to genocide such monsters.");*/
	You("その怪物を虐殺することはできない．");
			else
#ifdef WIZARD	/* to aid in topology testing; remove pesky monsters */
			  if (wizard && buf[0] == '*') {
			    register struct monst *mtmp, *mtmp2;

			    gonecnt = 0;
			    for (mtmp = fmon; mtmp; mtmp = mtmp2) {
				mtmp2 = mtmp->nmon;
				mongone(mtmp);
				gonecnt++;
			    }
/*JP	pline("Eliminated %d monster%s.", gonecnt, plur(gonecnt));*/
	pline("%dの怪物を除いた．", gonecnt);
			    return;
			} else
#endif
/*JP	pline("That symbol does not represent any monster.");*/
	pline("その記号の怪物はいない．");
			continue;
		}
		for (i = LOW_PM; i < NUMMONS; i++) {
		    if(mons[i].mlet == class) {
			const char *n = makeplural(mons[i].mname);

			if (Your_Own_Role(i) || ((mons[i].geno & G_GENO)
				&& !(mvitals[i].mvflags & G_GENOD))) {
			/* This check must be first since player monsters might
			 * have G_GENOD or !G_GENO.
			 */
/*JP			    pline("Wiped out all %s.", n);*/
			    pline("%sを全て排除した．", jtrns_mon(n, -1));
			    if (&mons[i] == player_mon()) {
				u.uhp = -1;
				killer_format = KILLED_BY_AN;
/*JP				killer = "scroll of genocide";*/
				killer = "虐殺の巻物で";
				if (u.umonnum >= LOW_PM)
/*JP				    You_feel("dead inside.");*/
				    You("魂が死んだような気がした．");
				else
				    done(GENOCIDED);
			    }
			    /* for simplicity (and fairness) let's avoid
			     * alignment changes here...
			     */
			    if (i==u.umonnum) rehumanize();
			    mvitals[i].mvflags |= (G_GENOD|G_NOCORPSE);
			    reset_rndmonst(i);
			    kill_genocided_monsters();
			    update_inventory();		/* eggs & tins */
			} else if (mvitals[i].mvflags & G_GENOD) {
/*JP			    pline("All %s are already nonexistent.", n);*/
			    pline("%sは既にいない．", jtrns_mon(n, -1));
			} else {
			  /* suppress feedback about quest beings except
			     for those applicable to our own role */
			  if ((mons[i].msound != MS_LEADER ||
			       quest_info(MS_LEADER) == i)
			   && (mons[i].msound != MS_NEMESIS ||
			       quest_info(MS_NEMESIS) == i)
			   && (mons[i].msound != MS_GUARDIAN ||
			       quest_info(MS_GUARDIAN) == i)
			/* non-leader/nemesis/guardian role-specific monster */
			   && (i != PM_NINJA ||		/* nuisance */
			       Role_is('S'))) {
				boolean named, uniq;

				named = type_is_pname(&mons[i]) ? TRUE : FALSE;
				uniq = (mons[i].geno & G_UNIQ) ? TRUE : FALSE;
				/* one special case */
				if (i == PM_HIGH_PRIEST) uniq = FALSE;

/*JP				You("aren't permitted to genocide %s%s.",
				    (uniq && !named) ? "the " : "",
				    (uniq || named) ? mons[i].mname : n);*/
				You("%sを虐殺できない．",
				    jtrns_mon((uniq || named) ? mons[i].mname : n, -1));
			    }
			}
		    }
		}
		return;
	}
}

#define REALLY 1
#define PLAYER 2
void
do_genocide(how)
int how;
/* 0 = no genocide; create monsters (cursed scroll) */
/* 1 = normal genocide */
/* 3 = forced genocide of player */
{
	char buf[BUFSZ];
	register int	i, killplayer = 0;
	register int mndx;
	register struct permonst *ptr;
	const char *which;

	if (how & PLAYER) {
		ptr = player_mon();
		mndx = monsndx(ptr);
		Strcpy(buf, ptr->mname);
		killplayer++;
	} else {
	    for(i = 0; ; i++) {
		if(i >= 5) {
		    pline(thats_enough_tries);
		    return;
		}
/*JP		getlin("What monster do you want to genocide? [type the name]",*/
		getlin("どの怪物を虐殺しますか？",
			buf);

/*JP		mndx = name_to_mon(buf);*/
		mndx = name_to_mon(etrns_mon(buf));

		if (mndx == NON_PM || (mvitals[mndx].mvflags & G_GENOD)) {
/*JP			pline("Such creatures %s exist in this world.",
			      (mndx == NON_PM) ? "do not" : "no longer");*/
			pline("そのような生き物は%sこの世界に存在しない．",
			      (mndx == NON_PM) ? "" : "もはや");
			continue;
		}
		ptr = &mons[mndx];
		if (Your_Own_Role(mndx)) {
			killplayer++;
			break;
		}
		if (is_human(ptr)) adjalign(-sgn(u.ualign.type));
		if (is_demon(ptr)) adjalign(sgn(u.ualign.type));

		if(!(ptr->geno & G_GENO))  {
			if(flags.soundok) {
	/* fixme: unconditional "caverns" will be silly in some circumstances */
			    if(flags.verbose)
/*JP			pline("A thunderous voice booms though the caverns:");
			    verbalize("No, mortal!  That will not be done.");*/
			pline("雷のような声が洞窟に響いた：");
			    pline("「人間よ，その望みはかなうまい」");
			}
			continue;
		}
		break;
	    }
	}

/*JP	which = "all ";*/
	which = "全て";
	if (Hallucination) {
	    if (u.umonnum != PM_PLAYERMON)
		Strcpy(buf,uasmon->mname);
	    else {
		Strcpy(buf, pl_character);
		buf[0] = lowc(buf[0]);
	    }
	} else {
	    Strcpy(buf, ptr->mname); /* make sure we have standard singular */
	    if ((ptr->geno & G_UNIQ) && ptr != &mons[PM_HIGH_PRIEST])
/*JP		which = !type_is_pname(ptr) ? "the " : "";*/
		which = !type_is_pname(ptr) ? "" : "";
	}
	if (how & REALLY) {
	    /* setting no-corpse affects wishing and random tin generation */
	    mvitals[mndx].mvflags |= (G_GENOD | G_NOCORPSE);
/*	    pline("Wiped out %s%s.", which,
		  (*which != 'a') ? buf : makeplural(buf));*/
	    pline("%sを%s一掃した．", jtrns_mon(buf, -1), which);

	    if (killplayer) {
		/* might need to wipe out dual role */
		int altx = Role_is('C') ? (PM_CAVEMAN + PM_CAVEWOMAN - mndx) :
			   Role_is('P') ? (PM_PRIEST  + PM_PRIESTESS - mndx) :
			   0;
		if (altx) mvitals[altx].mvflags |= (G_GENOD | G_NOCORPSE);

		u.uhp = -1;
		killer_format = KILLED_BY_AN;
		if (how & PLAYER)
/*JP
			killer = "genocidal confusion";
*/
			killer = "混乱による自虐的虐殺で";
		else /* selected player deliberately, not confused */
/*JP
			killer = "scroll of genocide";
*/
			killer = "虐殺の巻物で";
	/* Polymorphed characters will die as soon as they're rehumanized. */
/*JP
		if (u.umonnum >= LOW_PM) You_feel("dead inside.");
*/
		if (u.umonnum >= LOW_PM) You("魂が死んだような気がした．");
		else
			done(GENOCIDED);
	    } else if (ptr == uasmon) {
		rehumanize();
	    }
	    reset_rndmonst(mndx);
	    kill_genocided_monsters();
	    update_inventory();	/* in case identified eggs were affected */
	} else {
	    int cnt = 0;

	    if (!(mons[mndx].geno & G_UNIQ) &&
		    !(mvitals[mndx].mvflags & (G_GENOD | G_EXTINCT)))
		for (i = rn1(3, 4); i > 0; i--) {
		    if (!makemon(ptr, u.ux, u.uy, NO_MINVENT))
			break;	/* couldn't make one */
		    ++cnt;
		    if (mvitals[mndx].mvflags & G_EXTINCT)
			break;	/* just made last one */
		}
	    if (cnt)
/*JP		pline("Sent in some %s.", makeplural(buf));*/
	        pline("%sが送られてきた．", jtrns_mon(buf, -1));
	    else
		pline(nothing_happens);
	}
}

void
punish(sobj)
register struct obj	*sobj;
{
/*JP	You("are being punished for your misbehavior!");*/
	You("不信心のため罰を受けた！");
	if(Punished){
/*JP		Your("iron ball gets heavier.");*/
		Your("鉄球はさらに重くなった．");
		uball->owt += 160 * (1 + sobj->cursed);
		return;
	}
	if (amorphous(uasmon) || is_whirly(uasmon) || unsolid(uasmon)) {
/*JP		pline("A ball and chain appears, then falls away.");*/
		pline("鉄球と鎖が現われたが，するっと抜けた．");
		dropy(mkobj(BALL_CLASS, TRUE));
		return;
	}
	setworn(mkobj(CHAIN_CLASS, TRUE), W_CHAIN);
	setworn(mkobj(BALL_CLASS, TRUE), W_BALL);
	uball->spe = 1;		/* special ball (see save) */

	/*
	 *  Place ball & chain if not swallowed.  If swallowed, the ball &
	 *  chain variables will be set at the next call to placebc().
	 */
	if (!u.uswallow) {
	    placebc();
	    if (Blind) set_bc(1);	/* set up ball and chain variables */
	    newsym(u.ux,u.uy);		/* see ball&chain if can't see self */
	}
}

void
unpunish()
{	    /* remove the ball and chain */
	struct obj *savechain = uchain;

	obj_extract_self(uchain);
	newsym(uchain->ox,uchain->oy);
	setworn((struct obj *)0, W_CHAIN);
	dealloc_obj(savechain);
	uball->spe = 0;
	setworn((struct obj *)0, W_BALL);
}

/* some creatures have special data structures that only make sense in their
 * normal locations -- if the player tries to create one elsewhere, or to revive
 * one, the disoriented creature becomes a zombie
 */
boolean
cant_create(mtype)
int *mtype;
{

	if (*mtype==PM_GUARD || *mtype==PM_SHOPKEEPER
	     || *mtype==PM_ALIGNED_PRIEST || *mtype==PM_ANGEL) {
		*mtype = PM_HUMAN_ZOMBIE;
		return TRUE;
	} else if (*mtype==PM_LONG_WORM_TAIL) {	/* for create_particular() */
		*mtype = PM_LONG_WORM;
		return TRUE;
	}
	return FALSE;
}

#ifdef WIZARD
boolean
create_particular()
{
	char buf[BUFSZ];
	int which, tries = 0;

	do {
/*JP	    getlin("Create what kind of monster? [type the name]", buf);*/
	    getlin("どの種の怪物を作りますか？", buf);
	    if (buf[0] == '\033') return FALSE;
/*JP	    which = name_to_mon(buf);*/
	    which = name_to_mon(etrns_mon(buf));
/*JP	    if (which < LOW_PM) pline("I've never heard of such monsters.");*/
	    if (which < 0) pline("そのような怪物は聞いたことがない．");
	    else break;
	} while (++tries < 5);
	if (tries == 5) pline(thats_enough_tries);
	else {
	    (void) cant_create(&which);
	    return((boolean)(makemon(&mons[which],
				u.ux, u.uy, NO_MM_FLAGS) != 0));
	}
	return FALSE;
}
#endif /* WIZARD */

#endif /* OVLB */

/*read.c*/
