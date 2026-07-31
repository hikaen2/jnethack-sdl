/*	SCCS Id: @(#)dokick.c	3.2	96/10/12	*/
/* Copyright (c) Izchak Miller, Mike Stephenson, Steve Linhart, 1989. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"
#include "eshk.h"

#define is_bigfoot(x)	((x) == &mons[PM_SASQUATCH])
#define martial()	(martial_bonus() || is_bigfoot(uasmon))

static NEARDATA struct rm *maploc;
static NEARDATA const char *gate_str;

extern boolean notonhead;	/* for long worms */

static void FDECL(kickdmg, (struct monst *, BOOLEAN_P));
static void FDECL(kick_monster, (XCHAR_P, XCHAR_P));
static int FDECL(kick_object, (XCHAR_P, XCHAR_P));
static char *FDECL(kickstr, (char *));
static void FDECL(otransit_msg, (struct obj *, BOOLEAN_P, int));
static void FDECL(drop_to, (coord *,SCHAR_P));

static NEARDATA struct obj *kickobj;

#define IS_SHOP(x)	(rooms[x].rtype >= SHOPBASE)

static void
kickdmg(mon, clumsy)
register struct monst *mon;
register boolean clumsy;
{
	register int mdx, mdy;
	register int dmg = ( ACURRSTR + ACURR(A_DEX) + ACURR(A_CON) )/ 15;
#ifdef WEAPON_SKILLS
	int kick_skill = P_NO_TYPE;
#endif

	/* excessive wt affects dex, so it affects dmg */
	if(clumsy) dmg = dmg/2;

	/* kicking a dragon or an elephant will not harm it */
	if(thick_skinned(mon->data)) dmg = 0;

	/* a good kick exercises your dex */
	exercise(A_DEX, TRUE);

/*	it is unchivalrous to attack the defenseless or from behind */
	if (Role_is('K') &&
		u.ualign.type == A_LAWFUL && u.ualign.record > -10 &&
		(!mon->mcanmove || mon->msleep || mon->mflee))
		adjalign(-1);

	/* squeeze some guilt feelings... */
	if(mon->mtame) {
	    abuse_dog(mon);
	    mon->mflee = mon->mtame ? 1 : 0;
#ifdef HISX
	    mon->mfleetim = mon->mfleetim + (dmg ? rnd(dmg) : 1);
#else
	    mon->mfleetim += (dmg ? rnd(dmg) : 1);
#endif
	}

	if (dmg > 0) {
		/* convert potential damage to actual damage */
		dmg = rnd(dmg);
#ifdef WEAPON_SKILLS
		if (dmg > 1 && martial()) kick_skill = P_MARTIAL_ARTS;
#endif
		if (martial()) dmg += rn2(ACURR(A_DEX)/2 + 1);
	}
	dmg += u.udaminc;	/* add ring(s) of increase damage */
	if (dmg > 0)
		mon->mhp -= dmg;
	if(mon->mhp > 0 && martial() && !bigmonst(mon->data) && !rn2(3)
			&& mon->mcanmove && mon != u.ustuck) {
		/* see if the monster has a place to move into */
		mdx = mon->mx + u.dx;
		mdy = mon->my + u.dy;
		if(goodpos(mdx, mdy, mon, mon->data)) {
/*JP			pline("%s reels from the blow.", Monnam(mon));*/
			pline("%sは強打されよろめいた．", Monnam(mon));
			remove_monster(mon->mx, mon->my);
			newsym(mon->mx, mon->my);
			place_monster(mon, mdx, mdy);
			newsym(mon->mx, mon->my);
			set_apparxy(mon);
		}
	}

	(void) passive(mon, TRUE, mon->mhp > 0, TRUE);
	if (mon->mhp <= 0) killed(mon);

#ifdef WEAPON_SKILLS
	/* may bring up a dialog, so put this after all messages */
	if (kick_skill != P_NO_TYPE)	/* exercise proficiency */
	    use_skill(kick_skill);
#endif
}

static void
kick_monster(x, y)
register xchar x, y;
{
	register boolean clumsy = FALSE;
	register struct monst *mon = m_at(x, y);
	register int i, j;

	bhitpos.x = x;
	bhitpos.y = y;
	if (attack_checks(mon, (struct obj *)0)) return;
	setmangry(mon);

	/* Kick attacks by kicking monsters are normal attacks, not special.
	 * If you have >1 kick attack, you get all of them.
	 */
	if (attacktype(uasmon, AT_KICK)) {
	    schar tmp = find_roll_to_hit(mon);
	    for(i=0; i<NATTK; i++) {
		if (uasmon->mattk[i].aatyp == AT_KICK && multi >= 0) {
		    /* check multi; maybe they had 2 kicks and the first */
		    /* was a kick against a floating eye */
		    if (tmp > rnd(20)) {
			int sum;

/*JP			You("kick %s.", mon_nam(mon));*/
			You("%sを蹴った．", mon_nam(mon));
			sum = damageum(mon, &(uasmon->mattk[i]));
			if (sum == 2)
				(void)passive(mon, 1, 0, TRUE);
			else (void)passive(mon, sum, 1, TRUE);
		    } else {
			missum(mon, &(uasmon->mattk[i]));
			(void)passive(mon, 0, 1, TRUE);
		    }
		}
	    }
	    return;
	}

	if(noncorporeal(mon->data)) {
/*JP		Your("kick passes through!");*/
		Your("蹴りは空をきった！");
		return;
	}

	if(Levitation && !rn2(3) && verysmall(mon->data) &&
	   !is_flyer(mon->data)) {
/*JP		pline("Floating in the air, you miss wildly!");*/
		pline("空中に浮いているので，大きく外した！");
		exercise(A_DEX, FALSE);
		(void) passive(mon, FALSE, 1, TRUE);
		return;
	}

	i = -inv_weight();
	j = weight_cap();

	if(i < (j*3)/10) {
		if(!rn2((i < j/10) ? 2 : (i < j/5) ? 3 : 4)) {
			if(martial() && !rn2(2)) goto doit;
/*JP			Your("clumsy kick does no damage.");*/
			Your("不器用な蹴りはダメージを与えない．");
			(void) passive(mon, FALSE, 1, TRUE);
			return;
		}
		if(i < j/10) clumsy = TRUE;
		else if(!rn2((i < j/5) ? 2 : 3)) clumsy = TRUE;
	}

	if(Fumbling) clumsy = TRUE;

	else if(uarm && objects[uarm->otyp].oc_bulky && ACURR(A_DEX) < rnd(25))
		clumsy = TRUE;
doit:
/*JP
	You("kick %s.", mon_nam(mon));
*/
	You("%sを蹴った．", mon_nam(mon));
	if(!rn2(clumsy ? 3 : 4) && (clumsy || !bigmonst(mon->data)) &&
	   mon->mcansee && !mon->mtrapped && !thick_skinned(mon->data) &&
	   mon->data->mlet != S_EEL && haseyes(mon->data) && mon->mcanmove &&
	   !mon->mstun && !mon->mconf && !mon->msleep &&
	   mon->data->mmove >= 12) {
		if(!nohands(mon->data) && !rn2(martial() ? 5 : 3)) {
/*JP
		    pline("%s blocks your %skick.", Monnam(mon),
				clumsy ? "clumsy " : "");
*/
		    pline("%sはあなたの%s蹴りを防いだ．", Monnam(mon),
				clumsy ? "不器用な" : "");
		    (void) passive(mon, FALSE, 1, TRUE);
		    return;
		} else {
		    mnexto(mon);
		    if(mon->mx != x || mon->my != y) {
#if 0 /*JP*/
			pline("%s %s, %s evading your %skick.", Monnam(mon),
				(can_teleport(mon->data) ? "teleports" :
				 is_floater(mon->data) ? "floats" :
				 is_flyer(mon->data) ? "flutters" :
				 (nolimbs(mon->data) || slithy(mon->data)) ?
					"slides" : "jumps"),
				clumsy ? "easily" : "nimbly",
				clumsy ? "clumsy " : "");
#endif
			pline("%sは%s，%sあなたの%s蹴りをたくみに避けた．", Monnam(mon),
				(can_teleport(mon->data) ? "瞬間移動し" :
				 is_floater(mon->data) ? "浮き" :
				 is_flyer(mon->data) ? "はばたき" :
				 (nolimbs(mon->data)  || slithy(mon->data))
					? "横に滑り" : "跳ね"),
				clumsy ? "楽々と" : "素早く",
				clumsy ? "不器用な" : "");
			(void) passive(mon, FALSE, 1, TRUE);
			return;
		    }
		}
	}
	kickdmg(mon, clumsy);
}

/*
 *  Return TRUE if caught (the gold taken care of), FALSE otherwise.
 *  The gold object is *not* attached to the fobj chain!
 */
boolean
ghitm(mtmp, gold)
register struct monst *mtmp;
register struct obj *gold;
{
	if(!likes_gold(mtmp->data) && !mtmp->isshk && !mtmp->ispriest
			&& !is_mercenary(mtmp->data)) {
		wakeup(mtmp);
	} else if (!mtmp->mcanmove) {
		/* too light to do real damage */
		if (canseemon(mtmp))
/*JP		    pline_The("gold hits %s.", mon_nam(mtmp));*/
		    pline("ゴールドは%sに命中した．", mon_nam(mtmp));
	} else {
		mtmp->msleep = 0;
		mtmp->meating = 0;
		if(!rn2(4)) setmangry(mtmp); /* not always pleasing */

		/* greedy monsters catch gold */
		if (cansee(mtmp->mx, mtmp->my))
/*JP		    pline("%s catches the gold.", Monnam(mtmp));*/
		    pline("%sはゴールドを受けとった．", Monnam(mtmp));
		mtmp->mgold += gold->quan;
		if (mtmp->isshk) {
			long robbed = ESHK(mtmp)->robbed;

			if (robbed) {
				robbed -= gold->quan;
				if (robbed < 0) robbed = 0;
/*JP				pline_The("amount %scovers %s recent losses.",
				      !robbed ? "" : "partially ",*/
				pline("%s%sの損失を補填するのに使われた．",
				      !robbed ? "" : "金の一部は",
				      his[mtmp->female]);
				ESHK(mtmp)->robbed = robbed;
				if(!robbed)
					make_happy_shk(mtmp, FALSE);
			} else {
				if(mtmp->mpeaceful) {
				    ESHK(mtmp)->credit += gold->quan;
/*JP				    You("have %ld zorkmid%s in credit.",
					ESHK(mtmp)->credit,
					plur(ESHK(mtmp)->credit));*/
				    You("%ldゴールドをクレジットにした．",
					ESHK(mtmp)->credit);
/*JP				} else verbalize("Thanks, scum!");*/
				} else verbalize("ありがとよ！くそったれ！");

			}
		} else if (mtmp->ispriest) {
			if (mtmp->mpeaceful)
/*JP			    verbalize("Thank you for your contribution.");
			else verbalize("Thanks, scum!");*/
			    verbalize("寄付をどうもありがとう．");
			else verbalize("ありがとよ！くそったれ！");

		} else if (is_mercenary(mtmp->data)) {
		    long goldreqd = 0L;

		    if (rn2(3)) {
			if (mtmp->data == &mons[PM_SOLDIER])
			   goldreqd = 100L;
			else if (mtmp->data == &mons[PM_SERGEANT])
			   goldreqd = 250L;
			else if (mtmp->data == &mons[PM_LIEUTENANT])
			   goldreqd = 500L;
			else if (mtmp->data == &mons[PM_CAPTAIN])
			   goldreqd = 750L;

			if (goldreqd) {
			   if (gold->quan > goldreqd +
				(u.ugold + u.ulevel*rn2(5))/ACURR(A_CHA))
			    mtmp->mpeaceful = TRUE;
			}
		     }
		     if (mtmp->mpeaceful)
/*JP			    verbalize("That should do.  Now beat it!");*/
			    verbalize("なんだい？これは？");
/*JP		     else verbalize("That's not enough, coward!");*/
		     else verbalize("そんなもので済むか，卑怯者！");
		 }

		dealloc_obj(gold);
		return(1);
	}
	return(0);
}

static int
kick_object(x, y)
xchar x, y;
{
	int range;
	register struct monst *mon, *shkp;
	register struct obj *otmp;
	struct trap *trap;
	char bhitroom;
	boolean costly, insider, isgold, slide = FALSE;

	/* if a pile, the "top" object gets kicked */
	kickobj = level.objects[x][y];

	/* kickobj should always be set due to conditions of call */
	if(!kickobj || kickobj->otyp == BOULDER
			|| kickobj == uball || kickobj == uchain)
		return(0);

	if((trap = t_at(x,y)) && trap->tseen) {
		if (((trap->ttyp == PIT || trap->ttyp == SPIKED_PIT)
					&& !passes_walls(uasmon))
				|| trap->ttyp == WEB) {
/*JP			You_cant("kick %s that's in a %s!", something,
				trap->ttyp == WEB ? "web" : "pit");*/
			You("%sの中では蹴ることができない",
				trap->ttyp == WEB ? "くもの巣" : "落し穴");

			return(1);
		}
	}

	if(Fumbling && !rn2(3)) {
/*JP		Your("clumsy kick missed.");*/
		Your("不器用な蹴りは外れた．");
		return(1);
	}

	if(kickobj->otyp == CORPSE && kickobj->corpsenm == PM_COCKATRICE
		&& !resists_ston(&youmonst) && !uarmf) {
/*JP	    You("kick the cockatrice corpse with your bare %s.",*/
	    You("素%sでコカトリスの死体を蹴った．",
		makeplural(body_part(FOOT)));
	    if (!(poly_when_stoned(uasmon) && polymon(PM_STONE_GOLEM))) {
/*JP		You("turn to stone...");*/
		You("石化した．．．");
		killer_format = KILLED_BY;
/*JP		killer = "kicking a cockatrice corpse without boots";*/
		killer = "靴無しでコカトリスの死体を蹴って";
		done(STONING);
	    }
	}

	/* range < 2 means the object will not move.	*/
	/* maybe dexterity should also figure here.     */
	range = (int)((ACURRSTR)/2 - kickobj->owt/40);

	if(martial()) range += rnd(3);

	if (is_pool(x, y)) {
	    /* you're in the water too; significantly reduce range */
	    range = range / 3 + 1;	/* {1,2}=>1, {3,4,5}=>2, {6,7,8}=>3 */
	} else {
	    if (is_ice(x, y)) range += rnd(3),  slide = TRUE;
	    if (kickobj->greased) range += rnd(3),  slide = TRUE;
	}

	/* Mjollnir is magically too heavy to kick */
	if(kickobj->oartifact == ART_MJOLLNIR) range = 1;

	/* see if the object has a place to move into */
	if(!ZAP_POS(levl[x+u.dx][y+u.dy].typ) || closed_door(x+u.dx, y+u.dy))
		range = 1;

	costly = ((shkp = shop_keeper(*in_rooms(x, y, SHOPBASE))) &&
				    costly_spot(x, y));
	insider = (*u.ushops && inside_shop(u.ux, u.uy) &&
				    *in_rooms(x, y, SHOPBASE) == *u.ushops);

	/* a box gets a chance of breaking open here */
	if(Is_box(kickobj)) {
		boolean otrp = kickobj->otrapped;
		struct obj *otmp2;
		long loss = 0L;

/*JP		if(range < 2) pline("THUD!");*/
		if(range < 2) pline("ガン！");

		for(otmp = kickobj->cobj; otmp; otmp = otmp2) {
			const char *result = (char *)0;

			otmp2 = otmp->nobj;
			if (objects[otmp->otyp].oc_material == GLASS
			    && otmp->oclass != GEM_CLASS
			    && !obj_resists(otmp, 33, 100)) {
/*JP				result = "shatter";*/
				result = "ガチャン";
			} else if (otmp->otyp == EGG && !rn2(3)) {
/*JP				result = "cracking";*/
				result = "グシャッ";
			}
			if (result) {
/*JP				You_hear("a muffled %s.",result);*/
				You_hear("%sという音を聞いた．", result);
				if(costly) loss += stolen_value(otmp, x, y,
					    (boolean)shkp->mpeaceful, TRUE);
				if (otmp->quan > 1L)
					useup(otmp);
				else {
					obj_extract_self(otmp);
					obfree(otmp, (struct obj *) 0);
				}
			}
		}
		if(costly && loss) {
		    if(!insider) {
/*JP			You("caused %ld zorkmids worth of damage!", loss);*/
		        You("%ldゴールド分のダメージをくらった！", loss);
			make_angry_shk(shkp, x, y);
		    } else {
/*JP		        You("owe %s %ld zorkmids for objects destroyed.",*/
		        You("器物破損で%sに%ldゴールドの借りをつくった．",
			    mon_nam(shkp), loss);
		    }
		}

		if (kickobj->olocked) {
		    if (!rn2(5) || (martial() && !rn2(2))) {
/*JP			You("break open the lock!");*/
			You("鍵を壊し開けた！");
			kickobj->olocked = 0;
			kickobj->obroken = 1;
			if (otrp) (void) chest_trap(kickobj, LEG, FALSE);
			return(1);
		    }
		} else {
		    if (!rn2(3) || (martial() && !rn2(2))) {
/*JP			pline_The("lid slams open, then falls shut.");*/
			pline("蓋がばたんと開き，閉じた．");
			if (otrp) (void) chest_trap(kickobj, LEG, FALSE);
			return(1);
		    }
		}
		if(range < 2) return(1);
		/* else let it fall through to the next cases... */
	}

	/* fragile objects should not be kicked */
	if (hero_breaks(kickobj, kickobj->ox, kickobj->oy, FALSE)) return 1;

	if(IS_ROCK(levl[x][y].typ)) {
		if ((!martial() && rn2(20) > ACURR(A_DEX))
				|| IS_ROCK(levl[u.ux][u.uy].typ)) {
/*JP			if (Blind) pline("It doesn't come loose.");*/
			if (Blind) pline("びくともしない．");
/*JP			else pline("%s do%sn't come loose.",
				The(distant_name(kickobj, xname)),
				(kickobj->quan == 1L) ? "es" : "");*/
			else pline("%sはびくともしない．",
				The(distant_name(kickobj, xname)));
			return(!rn2(3) || martial());
		}
/*JP		if (Blind) pline("It comes loose.");*/
		if (Blind) pline("ヒビが入ってきた．");
/*JP		else pline("%s come%s loose.",
			   The(distant_name(kickobj, xname)),
			   (kickobj->quan == 1L) ? "s" : "");*/
		else pline("%sにヒビが入ってきた．",
			   The(distant_name(kickobj, xname)));
		obj_extract_self(kickobj);
		newsym(x, y);
		if (costly && (!costly_spot(u.ux, u.uy)
			       || !index(u.urooms, *in_rooms(x, y, SHOPBASE))))
			addtobill(kickobj, FALSE, FALSE, FALSE);
/*JP		if(!flooreffects(kickobj,u.ux,u.uy,"fall")) {*/
		if(!flooreffects(kickobj,u.ux,u.uy,"落ちる")) {
		    place_object(kickobj, u.ux, u.uy);
		    stackobj(kickobj);
		    newsym(u.ux, u.uy);
		}
		return(1);
	}

	isgold = (kickobj->oclass == GOLD_CLASS);

	/* too heavy to move.  range is calculated as potential distance from
	 * player, so range == 2 means the object may move up to one square
	 * from its current position
	 */
	if(range < 2 || (isgold && kickobj->quan > 300L)) {
/*JP	    if(!Is_box(kickobj)) pline("Thump!");*/
	    if(!Is_box(kickobj)) pline("ゴツン！");
	    return(!rn2(3) || martial());
	}

	if (kickobj->quan > 1L && !isgold) (void) splitobj(kickobj, 1L);

	if (slide && !Blind)
/*JP	    pline("Whee!  %s slide%s across the %s.", Doname2(kickobj),
		kickobj->quan > 1L ? "" : "s",
		surface(x,y));*/
	    pline("ズルッ！%sは%sの上を滑った．", Doname2(kickobj),
		surface(x,y));

	obj_extract_self(kickobj);
	(void) snuff_candle(kickobj);
	newsym(x, y);
	mon = bhit(u.dx, u.dy, range, KICKED_WEAPON,
		   (int (*)()) 0, (int (*)()) 0, kickobj);

	if(mon) {
	    if (mon->isshk &&
		    kickobj->where == OBJ_MINVENT && kickobj->ocarry == mon)
		return 1;	/* alert shk caught it */
	    notonhead = (mon->mx != bhitpos.x || mon->my != bhitpos.y);
	    /* awake monster if sleeping */
	    wakeup(mon);
	    if(isgold ? ghitm(mon, kickobj) :	/* caught? */
		thitmonst(mon, kickobj))	/* hit && used up? */
		return(1);
	}

	/* the object might have fallen down a hole */
	if (kickobj->where == OBJ_MIGRATING)
	    return 1;

	bhitroom = *in_rooms(bhitpos.x, bhitpos.y, SHOPBASE);
	if (costly && (!costly_spot(bhitpos.x, bhitpos.y) ||
			*in_rooms(x, y, SHOPBASE) != bhitroom)) {
	    if(isgold)
		costly_gold(x, y, kickobj->quan);
	    else (void)stolen_value(kickobj, x, y,
				    (boolean)shkp->mpeaceful, FALSE);
	}

/*JP	if(flooreffects(kickobj,bhitpos.x,bhitpos.y,"fall")) return(1);*/
	if(flooreffects(kickobj,bhitpos.x,bhitpos.y,"落ちる")) return(1);
	place_object(kickobj, bhitpos.x, bhitpos.y);
	stackobj(kickobj);
	newsym(kickobj->ox, kickobj->oy);
	return(1);
}

static char *
kickstr(buf)
char *buf;
{
	const char *what;

#if 0 /*JP*/
	if (kickobj) what = distant_name(kickobj,doname);
	else if (IS_DOOR(maploc->typ)) what = "a door";
	else if (IS_STWALL(maploc->typ)) what = "a wall";
	else if (IS_ROCK(maploc->typ)) what = "a rock";
	else if (IS_THRONE(maploc->typ)) what = "a throne";
#ifdef SINKS
	else if (IS_SINK(maploc->typ)) what = "a sink";
#endif
	else if (IS_ALTAR(maploc->typ)) what = "an altar";
	else if (IS_DRAWBRIDGE(maploc->typ)) what = "the drawbridge";
	else if (maploc->typ == STAIRS) what = "the stairs";
	else if (maploc->typ == LADDER) what = "a ladder";
	else what = "something weird";
	return strcat(strcpy(buf, "kicking "), what);
#endif
	if (kickobj) what = distant_name(kickobj,doname);
	else if (IS_DOOR(maploc->typ)) what = "扉";
	else if (IS_STWALL(maploc->typ)) what = "壁";
	else if (IS_ROCK(maploc->typ)) what = "岩";
	else if (IS_THRONE(maploc->typ)) what = "玉座";
#ifdef SINKS
	else if (IS_SINK(maploc->typ)) what = "流し台";
#endif
	else if (IS_ALTAR(maploc->typ)) what = "祭壇";
	else if (IS_DRAWBRIDGE(maploc->typ)) what = "跳ね橋";
	else if (maploc->typ == STAIRS) what = "階段";
	else if (maploc->typ == LADDER) what = "はしご";
	else what = "何か妙なもの";
/*JP	return strcat(strcpy(buf, "kicking "), what);*/

	Sprintf(buf, "%sを蹴って", what);
	return buf;
}

int
dokick()
{
	register int x, y;
	int avrg_attrib;
	register struct monst *mtmp;
	s_level *slev;
	boolean no_kick = FALSE;
	char buf[BUFSZ];

	if (nolimbs(uasmon) || slithy(uasmon)) {
/*JP
		You("have no legs to kick with.");
*/
		You("何かを蹴ろうにも足がない．");
		no_kick = TRUE;
	} else if (verysmall(uasmon)) {
/*JP
		You("are too small to do any kicking.");
*/
		You("何かを蹴るには小さすぎる．");
		no_kick = TRUE;
	} else if (Wounded_legs) {
		/* note: dojump() has similar code */
		long wl = (Wounded_legs & BOTH_SIDES);
		const char *bp = body_part(LEG);

		if (wl == BOTH_SIDES) bp = makeplural(bp);
/*JP
		Your("%s%s %s in no shape for kicking.",
		     (wl == LEFT_SIDE) ? "left " :
			(wl == RIGHT_SIDE) ? "right " : "",
		     bp, (wl == BOTH_SIDES) ? "are" : "is");
*/
		You("%s%sを怪我をしており蹴れない．",
		     (wl == LEFT_SIDE) ? "左" :
			(wl == RIGHT_SIDE) ? "右" : "",
		     bp);
		no_kick = TRUE;
	} else if (near_capacity() > SLT_ENCUMBER) {
/*JP
		Your("load is too heavy to balance yourself for a kick.");
*/
		You("たくさんものを持ちすぎて蹴りのためのバランスがとれない．");
		no_kick = TRUE;
	} else if (u.uinwater && !rn2(2)) {
/*JP
		Your("slow motion kick doesn't hit anything.");
*/
		Your("遅い動きの蹴りでは命中しようがない．");
		no_kick = TRUE;
	} else if (u.utrap) {
		switch (u.utraptype) {
		    case TT_PIT:
/*JP
			pline("There's not enough room to kick down here.");
*/
			pline("落し穴にはまっているので，蹴れない．");
			break;
		    case TT_WEB:
		    case TT_BEARTRAP:
/*JP
			You_cant("move your %s!", body_part(LEG));
*/
			You("%sを動かすことができない！", body_part(LEG));
			break;
		    default:
			break;
		}
		no_kick = TRUE;
	}

	if (no_kick) {
		/* discard direction typeahead, if any */
		display_nhwindow(WIN_MESSAGE, TRUE);	/* --More-- */
		return 0;
	}

	if(!getdir((char *)0)) return(0);
	if(!u.dx && !u.dy) return(0);

	x = u.ux + u.dx;
	y = u.uy + u.dy;

	avrg_attrib = (ACURRSTR+ACURR(A_DEX)+ACURR(A_CON))/3;

	if(u.uswallow) {
		switch(rn2(3)) {
/*JP		case 0:  You_cant("move your %s!", body_part(LEG));*/
		case 0:  You("%sを動かすことができない！", body_part(LEG));
			 break;
		case 1:  if (is_animal(u.ustuck->data)) {
/*JP				pline("%s burps loudly.", Monnam(u.ustuck));*/
				pline("%sは大きなゲップをした．", Monnam(u.ustuck));
				break;
			 }
/*JP		default: Your("feeble kick has no effect."); break;*/
		default: Your("弱々しい蹴りは効果がない"); break;
		}
		return(1);
	}
	if (Levitation) {
		int xx, yy;

		xx = u.ux - u.dx;
		yy = u.uy - u.dy;
		/* doors can be opened while levitating, so they must be
		 * reachable for bracing purposes
		 * Possible extension: allow bracing against stuff on the side?
		 */
		if (isok(xx,yy) && !IS_ROCK(levl[xx][yy].typ) &&
			!IS_DOOR(levl[xx][yy].typ) &&
			(!Is_airlevel(&u.uz) || !OBJ_AT(xx,yy))) {
/*JP		    You("have nothing to brace yourself against.");*/
		    pline("支えにできるようなものが無い．");
		    return(0);
		}
	}

	wake_nearby();
	u_wipe_engr(2);

	maploc = &levl[x][y];
	/* The next five tests should stay in    */
	/* their present order: monsters, pools, */
	/* objects, non-doors, doors.		 */

	if(MON_AT(x, y)) {
		struct permonst *mdat = m_at(x,y)->data;
		kick_monster(x, y);
		if((Is_airlevel(&u.uz) || Levitation) && flags.move) {
		    int range;

		    range = ((int)uasmon->cwt + (weight_cap() + inv_weight()));
		    if (range < 1) range = 1; /* divide by zero avoidance */
		    range = (3*(int)mdat->cwt) / range;

		    if(range < 1) range = 1;
		    hurtle(-u.dx, -u.dy, range);
		}
		return(1);
	}

	if (is_pool(x, y) ^ !!u.uinwater) {
		/* objects normally can't be removed from water by kicking */
/*JP		You("splash some water around.");*/
		You("水を回りにまきちらした．");
		return 1;
	}

	kickobj = (struct obj *)0;
	if (OBJ_AT(x, y) &&
	    (!Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)
	     || sobj_at(BOULDER,x,y))) {
		if(kick_object(x, y)) {
		    if(Is_airlevel(&u.uz))
			hurtle(-u.dx, -u.dy, 1); /* assume it's light */
		    return(1);
		}
		goto ouch;
	}

	if(!IS_DOOR(maploc->typ)) {
		if(maploc->typ == SDOOR) {
#ifdef NEWBIE
	newbie.try_open = 1000;
#endif
		    if(!Levitation && rn2(30) < avrg_attrib) {
			cvt_sdoor_to_door(maploc);	/* ->typ = DOOR */
/*JP
			pline("Crash!  %s a secret door!",
*/
			pline("ガシャン！あなたは秘密の扉を%s！",
			      /* don't "kick open" when it's locked
				 unless it also happens to be trapped */
			(maploc->doormask & (D_LOCKED|D_TRAPPED)) == D_LOCKED ?
/*JP
			      "Your kick uncovers" : "You kick open");
*/
			      "発見した" : "蹴り開けた");
			exercise(A_DEX, TRUE);
			if(maploc->doormask & D_TRAPPED) {
			    maploc->doormask = D_NODOOR;
/*JP
			    b_trapped("door", FOOT);
*/
			    b_trapped("扉", FOOT);
			} else if (maploc->doormask != D_NODOOR &&
				   !(maploc->doormask & D_LOCKED))
			    maploc->doormask = D_ISOPEN;
			if (Blind)
			    feel_location(x,y);	/* we know its gone */
			else
			    newsym(x,y);
			if (maploc->doormask == D_ISOPEN ||
			    maploc->doormask == D_NODOOR)
			    unblock_point(x,y);	/* vision */
			return(1);
		    } else goto ouch;
		}
		if(maploc->typ == SCORR) {
#ifdef NEWBIE
	newbie.try_open = 1000;
#endif
		    if(!Levitation && rn2(30) < avrg_attrib) {
/*JP			pline("Crash!  You kick open a secret passage!");*/
			pline("ガシャン！あなたは秘密の通路を蹴りやぶった！");
			exercise(A_DEX, TRUE);
			maploc->typ = CORR;
			if (Blind)
			    feel_location(x,y);	/* we known it's gone */
			else
			    newsym(x,y);
			unblock_point(x,y);	/* vision */
			return(1);
		    } else goto ouch;
		}
		if(IS_THRONE(maploc->typ)) {
		    register int i;
		    if(Levitation) goto dumb;
		    if((Luck < 0 || maploc->doormask) && !rn2(3)) {
			maploc->typ = ROOM;
			maploc->doormask = 0; /* don't leave loose ends.. */
			mkgold((long)rnd(200), x, y);
			if (Blind)
/*JP			    pline("CRASH!  You destroy it.");*/
			    pline("ガシャン！あなたは何かを破壊した．");
			else {
/*JP			    pline("CRASH!  You destroy the throne.");*/
			    pline("ガシャン！あなたは玉座を破壊した．");
			    newsym(x, y);
			}
			exercise(A_DEX, TRUE);
			return(1);
		    } else if(Luck > 0 && !rn2(3) && !maploc->looted) {
			mkgold((long) rn1(201, 300), x, y);
			i = Luck + 1;
			if(i > 6) i = 6;
			while(i--) (void) mkobj_at(GEM_CLASS, x, y, TRUE);
			if (Blind)
/*JP			    You("kick %s loose!", something);*/
			    You("なにかを蹴り散らした！");
			else {
/*JP			    You("kick loose some ornamental coins and gems!");*/
			    You("装飾用の金貨や宝石を蹴り散らした！");
			    newsym(x, y);
			}
			/* prevent endless milking */
			maploc->looted = T_LOOTED;
			return(1);
		    } else if (!rn2(4)) {
			if(dunlev(&u.uz) < dunlevs_in_dungeon(&u.uz)) {
			    fall_through(FALSE);
			    return(1);
			} else goto ouch;
		    }
		    goto ouch;
		}
		if(IS_ALTAR(maploc->typ)) {
		    if(Levitation) goto dumb;
/*JP		    You("kick %s.",(Blind ? something : "the altar"));*/
		    You("%sを蹴った．",(Blind ? "何かを" : "祭壇を"));
		    if(!rn2(3)) goto ouch;
		    altar_wrath(x, y);
		    exercise(A_DEX, TRUE);
		    return(1);
		}
		if(IS_FOUNTAIN(maploc->typ)) {
		    if(Levitation) goto dumb;
/*JP		    You("kick %s.",(Blind ? something : "the fountain"));*/
		    You("%sを蹴った．",(Blind ? "何かを" : "泉"));
		    if(!rn2(3)) goto ouch;
		    /* make metal boots rust */
		    if(uarmf && rn2(3))
/*JP			if (!rust_dmg(uarmf, "metal boots", 1, FALSE)) {*/
			if (!rust_dmg(uarmf, "金属の靴", 1, FALSE)) {
/*JP				Your("boots get wet.");*/
				Your("靴は濡れた．");
				/* could cause short-lived fumbling here */
			}
		    exercise(A_DEX, TRUE);
		    return(1);
		}
#ifdef SINKS
		if(IS_SINK(maploc->typ)) {
		    if(Levitation) goto dumb;
		    if(rn2(5)) {
			if(flags.soundok)
/*JP			    pline("Klunk!  The pipes vibrate noisily.");*/
			    pline("ガラン！パイプはうるさく振動した．");
/*JP			else pline("Klunk!");*/
			else pline("ガラン！");
			exercise(A_DEX, TRUE);
			return(1);
		    } else if(!(maploc->looted & S_LPUDDING) && !rn2(3) &&
			  !(mvitals[PM_BLACK_PUDDING].mvflags & G_GONE)) {
			if (Blind)
/*JP			    You_hear("a gushing sound.");*/
			    You("なにかが噴出する音を聞いた．");
			else
/*JP			    pline("A %s ooze gushes up from the drain!",*/
			    pline("%s液体が排水口からにじみ出た！",
					 hcolor(Black));
			(void) makemon(&mons[PM_BLACK_PUDDING],
					 x, y, NO_MM_FLAGS);
			exercise(A_DEX, TRUE);
			newsym(x,y);
			maploc->looted |= S_LPUDDING;
			return(1);
		    } else if(!(maploc->looted & S_LDWASHER) && !rn2(3) &&
			      !(mvitals[poly_gender() == 1 ? PM_INCUBUS
					: PM_SUCCUBUS].mvflags & G_GONE)) {
			/* can't resist... */
/*JP			pline("%s returns!", (Blind ? Something :
							"The dish washer"));*/
			pline("%sは戻った！", (Blind ? "何か" :
							"皿洗い"));

			if (makemon(&mons[poly_gender() == 1 ?
				PM_INCUBUS : PM_SUCCUBUS], x, y, NO_MM_FLAGS))
			    newsym(x,y);
			maploc->looted |= S_LDWASHER;
			exercise(A_DEX, TRUE);
			return(1);
		    } else if(!rn2(3)) {
/*JP			pline("Flupp!  %s.", (Blind ?
				      "You hear a sloshing sound" :
				      "Muddy waste pops up from the drain"));*/
			pline("うわ！%s．", (Blind ?
				      "あなたは，バチャバチャする音を聞いた" :
				      "排水口から泥々の廃棄物が出てくる．"));
			if(!(maploc->looted & S_LRING)) { /* once per sink */
			    if (!Blind)
/*JP				You("see a ring shining in its midst.");*/
				You("その中央に光る指輪を見つけた．");
			    (void) mkobj_at(RING_CLASS, x, y, TRUE);
			    newsym(x, y);
			    exercise(A_DEX, TRUE);
			    exercise(A_WIS, TRUE);	/* a discovery! */
			    maploc->looted |= S_LRING;
			}
			return(1);
		    }
		    goto ouch;
		}
#endif
		if (maploc->typ == STAIRS || maploc->typ == LADDER ||
						    IS_STWALL(maploc->typ)) {
		    if(!IS_STWALL(maploc->typ) && maploc->ladder == LA_DOWN)
			goto dumb;
ouch:
/*JP		    pline("Ouch!  That hurts!");*/
		    pline("いてっ！怪我した！");
		    exercise(A_DEX, FALSE);
		    exercise(A_STR, FALSE);
		    if (Blind) feel_location(x,y); /* we know we hit it */
		    if(!rn2(3)) set_wounded_legs(RIGHT_SIDE, 5 + rnd(5));
		    losehp(rnd(ACURR(A_CON) > 15 ? 3 : 5), kickstr(buf),
			KILLED_BY);
		    if(Is_airlevel(&u.uz) || Levitation)
			hurtle(-u.dx, -u.dy, rn1(2,4)); /* assume it's heavy */
		    return(1);
		}
		if (is_drawbridge_wall(x,y) >= 0) {
/*JP		    pline_The("drawbridge is unaffected.");*/
		    pline("跳ね橋はびくともしない．");
		    if(Levitation)
			hurtle(-u.dx, -u.dy, rn1(2,4)); /* it's heavy */
		    return(1);
		}
		goto dumb;
	}

	if(maploc->doormask == D_ISOPEN ||
	   maploc->doormask == D_BROKEN ||
	   maploc->doormask == D_NODOOR) {
dumb:
		exercise(A_DEX, FALSE);
		if (martial() || ACURR(A_DEX) >= 16 || rn2(3)) {
/*JP			You("kick at empty space.");*/
			You("何もない空間を蹴った．");
			if (Blind) feel_location(x,y);
		} else {
/*JP			pline("Dumb move!  You strain a muscle.");*/
			pline("ばかげた動きだ！筋肉を痛めた．");
			exercise(A_STR, FALSE);
			set_wounded_legs(RIGHT_SIDE, 5 + rnd(5));
		}
		if ((Is_airlevel(&u.uz) || Levitation) && rn2(2)) {
		    hurtle(-u.dx, -u.dy, 1);
		    return 1;		/* you moved, so use up a turn */
		}
		return(0);
	}

	/* not enough leverage to kick open doors while levitating */
	if(Levitation) goto ouch;

	exercise(A_DEX, TRUE);
	/* door is known to be CLOSED or LOCKED */
	if(rnl(35) < avrg_attrib + (!martial() ? 0 : ACURR(A_DEX))) {
		boolean shopdoor = *in_rooms(x, y, SHOPBASE) ? TRUE : FALSE;
#ifdef NEWBIE
	newbie.try_open = 1000;
#endif
		/* break the door */
		if(maploc->doormask & D_TRAPPED) {
/*JP		    if (flags.verbose) You("kick the door.");*/
		    if (flags.verbose) You("扉を蹴った．");
		    exercise(A_STR, FALSE);
		    maploc->doormask = D_NODOOR;
/*JP		    b_trapped("door", FOOT);*/
		    b_trapped("扉", FOOT);
		} else if(ACURR(A_STR) > 18 && !rn2(5) && !shopdoor) {
/*JP		    pline("As you kick the door, it shatters to pieces!");*/
		    pline("扉を蹴ると，こなごなにくだけた！");
		    exercise(A_STR, TRUE);
		    maploc->doormask = D_NODOOR;
		} else {
/*JP		    pline("As you kick the door, it crashes open!");*/
		    pline("扉を蹴ると，壊れて開いた！");
		    exercise(A_STR, TRUE);
		    maploc->doormask = D_BROKEN;
		}
		if (Blind)
		    feel_location(x,y);		/* we know we broke it */
		else
		    newsym(x,y);
		unblock_point(x,y);		/* vision */
		if (shopdoor) {
		    add_damage(x, y, 400L);
/*JP		    pay_for_damage("break");*/
		    pay_for_damage("破壊する");
		}
		if ((slev = Is_special(&u.uz)) && slev->flags.town)
		  for(mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
		    if((mtmp->data == &mons[PM_WATCHMAN] ||
			mtmp->data == &mons[PM_WATCH_CAPTAIN]) &&
			couldsee(mtmp->mx, mtmp->my) &&
			mtmp->mpeaceful) {
/*JP			pline("%s yells:", Amonnam(mtmp));*/
			pline("%sは叫んだ：", Amonnam(mtmp));
/*JP			verbalize("Halt, thief!  You're under arrest!");*/
			verbalize("止まれ泥棒！おまえを逮捕する！");
			(void) angry_guards(FALSE);
			break;
		    }
		  }
	} else {
	    if (Blind) feel_location(x,y);	/* we know we hit it */
	    exercise(A_STR, TRUE);
/*JP	    pline("WHAMMM!!!");*/
	    pline("ぐぁぁぁん");
	    if ((slev = Is_special(&u.uz)) && slev->flags.town)
		for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
		    if ((mtmp->data == &mons[PM_WATCHMAN] ||
				mtmp->data == &mons[PM_WATCH_CAPTAIN]) &&
			    mtmp->mpeaceful && couldsee(mtmp->mx, mtmp->my)) {
/*JP			pline("%s yells:", Amonnam(mtmp));*/
		        pline("%sは叫んだ：", Amonnam(mtmp));

			if(levl[x][y].looted & D_WARNED) {
/*JP			    verbalize("Halt, vandal!  You're under arrest!");*/
			    verbalize("止まれ野蛮人！おまえを逮捕する！");
			    (void) angry_guards(FALSE);
			} else {
/*JP			    verbalize("Hey, stop damaging that door!");*/
			    verbalize("おい，扉を破壊するのをやめろ！");
			    levl[x][y].looted |= D_WARNED;
			}
			break;
		    }
		}
	}
	return(1);
}

static void
drop_to(cc, loc)
coord *cc;
schar loc;
{
	/* cover all the MIGR_xxx choices generated by down_gate() */
	switch (loc) {
	 case MIGR_RANDOM:	/* trap door or hole */
		    if (Is_stronghold(&u.uz)) {
			cc->x = valley_level.dnum;
			cc->y = valley_level.dlevel;
			break;
		    } else if (In_endgame(&u.uz) || Is_botlevel(&u.uz)) {
			cc->y = cc->x = 0;
			break;
		    } /* else fall to the next cases */
	 case MIGR_STAIRS_UP:
	 case MIGR_LADDER_UP:
		    cc->x = u.uz.dnum;
		    cc->y = u.uz.dlevel + 1;
		    break;
	 case MIGR_SSTAIRS:
		    cc->x = sstairs.tolev.dnum;
		    cc->y = sstairs.tolev.dlevel;
		    break;
	 default:
	 case MIGR_NOWHERE:
		    /* y==0 means "nowhere", in which case x doesn't matter */
		    cc->y = cc->x = 0;
		    break;
	}
}

void
impact_drop(missile, x, y, dlev)
struct obj *missile;
xchar x, y, dlev;
{
	schar toloc;
	register struct obj *obj, *obj2;
	register struct monst *shkp;
	long oct, dct, price, debit, robbed;
	boolean angry, costly, isrock;
	coord cc;

	if(!OBJ_AT(x, y)) return;

	toloc = down_gate(x, y);
	drop_to(&cc, toloc);
	if (!cc.y) return;

	if (dlev) {
		/* send objects next to player falling through trap door.
		 * checked in obj_delivery().
		 */
		toloc = MIGR_NEAR_PLAYER;
		cc.y = dlev;
	}

	costly = costly_spot(x, y);
	price = debit = robbed = 0L;
	angry = FALSE;
	shkp = (struct monst *) 0;
	/* if 'costly', we must keep a record of ESHK(shkp) before
	 * it undergoes changes through the calls to stolen_value.
	 * the angry bit must be reset, if needed, in this fn, since
	 * stolen_value is called under the 'silent' flag to avoid
	 * unsavory pline repetitions.
	 */
	if(costly) {
	    if ((shkp = shop_keeper(*in_rooms(x, y, SHOPBASE))) != 0) {
		debit	= ESHK(shkp)->debit;
		robbed	= ESHK(shkp)->robbed;
		angry	= !shkp->mpeaceful;
	    }
	}

	isrock = (missile && missile->otyp == ROCK);
	oct = dct = 0L;
	for(obj = level.objects[x][y]; obj; obj = obj2) {
		obj2 = obj->nexthere;
		if(obj == missile) continue;
		/* number of objects in the pile */
		oct += obj->quan;
		if(obj == uball || obj == uchain) continue;
		/* boulders can fall too, but rarely & never due to rocks */
		if((isrock && obj->otyp == BOULDER) ||
		   rn2(obj->otyp == BOULDER ? 30 : 3)) continue;
		obj_extract_self(obj);

		if(costly) {
		    price += stolen_value(obj, x, y,
				(costly_spot(u.ux, u.uy) &&
				 index(u.urooms, *in_rooms(x, y, SHOPBASE))),
				TRUE);
		    /* set obj->no_charge to 0 */
		    if (Has_contents(obj))
			picked_container(obj);	/* does the right thing */
		    if (obj->oclass != GOLD_CLASS)
			obj->no_charge = 0;
		}

		add_to_migration(obj);
		obj->ox = cc.x;
		obj->oy = cc.y;
		obj->owornmask = (long)toloc;

		/* number of fallen objects */
		dct += obj->quan;
	}

	if (dct && cansee(x,y)) {	/* at least one object fell */
/*JP
	    const char *what = (dct == 1L ? "object falls" : "objects fall");
*/
	    const char *what = "物";

	    if (missile)
/*JP
		pline("From the impact, %sother %s.",
		      dct == oct ? "the " : dct == 1L ? "an" : "", what);
*/
		pline("衝撃で，他の%sが落ちた．",what);
	    else if (oct == dct)
/*JP
		pline("%s adjacent %s %s.",
		      dct == 1L ? "The" : "All the", what, gate_str);
*/
		pline("近くにあった%sが%s落ちた．",
		      what, gate_str);
	    else
/*JP
		pline("%s adjacent %s %s.",
		      dct == 1L ? "One of the" : "Some of the",
		      dct == 1L ? "objects falls" : what, gate_str);
*/
		pline("近くにあった%s%s%s落ちた．",
		      what,
		      dct == 1L ? "は" : "のいくつかは",
		      gate_str);
	}

	if(costly && shkp && price) {
		if(ESHK(shkp)->robbed > robbed) {
/*JP
		    You("removed %ld zorkmids worth of goods!", price);
*/
		    You("%ldゴールド分の品物を取りさった！",price);
		    if(cansee(shkp->mx, shkp->my)) {
			if(ESHK(shkp)->customer[0] == 0)
			    (void) strncpy(ESHK(shkp)->customer,
					   plname, PL_NSIZ);
			if(angry)
/*JP
			    pline("%s is infuriated!", Monnam(shkp));
*/
			    pline("%sは激怒した！", Monnam(shkp));
/*JP
			else pline("\"%s, you are a thief!\"", plname);
*/
			else pline("「%s，おまえは盗賊だな！」", plname);
/*JP
		    } else  You_hear("a scream, \"Thief!\"");
*/
		    } else  You("金切り声を聞いた「泥棒！」");
		    hot_pursuit(shkp);
		    (void) angry_guards(FALSE);
		    return;
		}
		if(ESHK(shkp)->debit > debit)
/*JP
		    You("owe %s %ld zorkmids for goods lost.",
*/
		    You("品物消失のため%sに%ldゴールドの借りをつくった．",
			Monnam(shkp),
			(ESHK(shkp)->debit - debit));
	}

}

/* NOTE: ship_object assumes otmp was FREED from fobj or invent.
 * <x,y> is the point of drop.  otmp is _not_ an <x,y> resident:
 * otmp is either a kicked, dropped, or thrown object.
 */
boolean
ship_object(otmp, x, y, shop_floor_obj)
xchar  x, y;
struct obj *otmp;
boolean shop_floor_obj;
{
	schar toloc;
	xchar ox, oy;
	coord cc;
	struct obj *obj;
	struct trap *t;
	boolean nodrop, unpaid, container, impact = FALSE;
	int n = 0;

	if (!otmp) return(FALSE);
	if ((toloc = down_gate(x, y)) == MIGR_NOWHERE) return(FALSE);
	drop_to(&cc, toloc);
	if (!cc.y) return(FALSE);

	/* objects other than attached iron ball always fall down ladder,
	   but have a chance of staying otherwise */
	nodrop = (otmp == uball) || (otmp == uchain) ||
			(toloc != MIGR_LADDER_UP && rn2(3));

	container = Has_contents(otmp);
	unpaid = (otmp->unpaid || (container && count_unpaid(otmp->cobj)));

	if(OBJ_AT(x, y)) {
	    for(obj = level.objects[x][y]; obj; obj = obj->nexthere)
		if(obj != otmp) n++;
	    if(n) impact = TRUE;
	}
	/* boulders never fall through trap doors, but they might knock
	   other things down before plugging the hole */
	if (otmp->otyp == BOULDER &&
		((t = t_at(x, y)) != 0) &&
		(t->ttyp == TRAPDOOR || t->ttyp == HOLE)) {
	    if (impact) impact_drop(otmp, x, y, 0);
	    return FALSE;		/* let caller finish the drop */
	}

	if (cansee(x, y))
	    otransit_msg(otmp, nodrop, n);

	if (nodrop) {
	    if (impact) impact_drop(otmp, x, y, 0);
	    return(FALSE);
	}

	if(unpaid || shop_floor_obj) {
	    if(unpaid) {
		subfrombill(otmp, shop_keeper(*u.ushops));
		(void)stolen_value(otmp, u.ux, u.uy, TRUE, FALSE);
	    } else {
		ox = otmp->ox;
		oy = otmp->oy;
		(void)stolen_value(otmp, ox, oy,
			  (costly_spot(u.ux, u.uy) &&
			      index(u.urooms, *in_rooms(ox, oy, SHOPBASE))),
			  FALSE);
	    }
	    /* set otmp->no_charge to 0 */
	    if(container)
		picked_container(otmp); /* happens to do the right thing */
	    if(otmp->oclass != GOLD_CLASS)
		otmp->no_charge = 0;
	}

	add_to_migration(otmp);
	otmp->ox = cc.x;
	otmp->oy = cc.y;
	otmp->owornmask = (long)toloc;

	if(impact) {
	    /* the objs impacted may be in a shop other than
	     * the one in which the hero is located.  another
	     * check for a shk is made in impact_drop.  it is, e.g.,
	     * possible to kick/throw an object belonging to one
	     * shop into another shop through a gap in the wall,
	     * and cause objects belonging to the other shop to
	     * fall down a trapdoor--thereby getting two shopkeepers
	     * angry at the hero in one shot.
	     */
	    impact_drop(otmp, x, y, 0);
	    newsym(x,y);
	}
	return(TRUE);
}

void
obj_delivery()
{
	register struct obj *otmp, *otmp2;
	register int nx, ny;
	long where;

	for (otmp = migrating_objs; otmp; otmp = otmp2) {
	    otmp2 = otmp->nobj;
	    if (otmp->ox != u.uz.dnum || otmp->oy != u.uz.dlevel) continue;

	    obj_extract_self(otmp);
	    where = otmp->owornmask;		/* destination code */
	    otmp->owornmask = 0L;

	    switch ((int)where) {
	     case MIGR_STAIRS_UP:   nx = xupstair,  ny = yupstair;
				break;
	     case MIGR_LADDER_UP:   nx = xupladder,  ny = yupladder;
				break;
	     case MIGR_SSTAIRS:	    nx = sstairs.sx,  ny = sstairs.sy;
				break;
	     case MIGR_NEAR_PLAYER: nx = u.ux,  ny = u.uy;
				break;
	     default:
	     case MIGR_RANDOM:	    nx = ny = 0;
				break;
	    }
	    if (nx > 0) {
		place_object(otmp, nx, ny);
		stackobj(otmp);
		scatter(nx, ny, rnd(2), 0);
	    } else {		/* random location */
		/* set dummy coordinates because there's no
		   current position for rloco() to update */
		otmp->ox = otmp->oy = 0;
		rloco(otmp);
	    }
	}
}

static void
otransit_msg(otmp, nodrop, num)
register struct obj *otmp;
register boolean nodrop;
int num;
{
	char obuf[BUFSZ];

/*JP	Sprintf(obuf, "%s%s",
		 (otmp->otyp == CORPSE &&
			type_is_pname(&mons[otmp->corpsenm])) ? "" : "The ",
		 xname(otmp));*/
	Sprintf(obuf, "%sは",
		 xname(otmp));

	if(num) { /* means: other objects are impacted */
/*JP	    Sprintf(eos(obuf), " hit%s %s object%s",
		      otmp->quan == 1L ? "s" : "",
		      num == 1 ? "another" : "other",
		      num > 1 ? "s" : "");*/
	    Sprintf(eos(obuf), "他の物体に命中して");
	    if(nodrop)
/*JP		Sprintf(eos(obuf), " and stop%s.",
			otmp->quan == 1L ? "s" : "");*/
		Sprintf(eos(obuf), "止まった．");
	    else
/*JP		Sprintf(eos(obuf), " and fall%s %s.",
			otmp->quan == 1L ? "s" : "", gate_str);*/
		Sprintf(eos(obuf), "%s落ちた．", gate_str);
	    pline(obuf);
	} else if(!nodrop)
/*JP	    pline("%s fall%s %s.", obuf,
		  otmp->quan == 1L ? "s" : "", gate_str);*/
	    pline("%sは%s落ちた．", obuf,
		  gate_str);
}

/* migration destination for objects which fall down to next level */
schar
down_gate(x, y)
xchar x, y;
{
	struct trap *ttmp;

	gate_str = 0;
	/* this matches the player restriction in goto_level() */
	if (on_level(&u.uz, &qstart_level) && !ok_to_quest())
	    return MIGR_NOWHERE;

	if ((xdnstair == x && ydnstair == y) ||
		(sstairs.sx == x && sstairs.sy == y && !sstairs.up)) {
/*JP	    gate_str = "down the stairs";*/
	    gate_str = "階段から";
	    return (xdnstair == x && ydnstair == y) ?
		    MIGR_STAIRS_UP : MIGR_SSTAIRS;
	}
	if (xdnladder == x && ydnladder == y) {
/*JP	    gate_str = "down the ladder";*/
	    gate_str = "はしごから";
	    return MIGR_LADDER_UP;
	}

	if (((ttmp = t_at(x, y)) != 0 && ttmp->tseen) &&
		(ttmp->ttyp == TRAPDOOR || ttmp->ttyp == HOLE)) {
	    gate_str = (ttmp->ttyp == TRAPDOOR) ?
/*JP		    "through the trap door" : "through the hole";*/
		    "落し扉に" : "穴に";
	    return MIGR_RANDOM;
	}
	return MIGR_NOWHERE;
}

/*dokick.c*/
