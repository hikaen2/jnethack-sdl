/*	SCCS Id: @(#)sit.c	3.2	96/07/15	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"
#include "artifact.h"

void
take_gold()
{
	if (u.ugold <= 0)  {
/*JP		You_feel("a strange sensation.");*/
		You("奇妙な感覚を覚えた．");
	} else {
/*JP		You("notice you have no gold!");*/
		You("お金を持ってないことに気がついた！");
		u.ugold = 0;
		flags.botl = 1;
	}
}

int
dosit()
{
/*JP
	static const char *sit_message = "sit on the %s.";
*/
	static const char *sit_message = "%sに座った．";
	register struct trap *trap;
	register int typ = levl[u.ux][u.uy].typ;

	if(!can_reach_floor())	{
	    if (Levitation)
/*JP
		You("tumble in place.");
*/
		You("その場で宙返りした．");
	    else
/*JP
		You("are sitting on air.");
*/
		You("空中に座った．");
	    return 0;
	}

	if(OBJ_AT(u.ux, u.uy)) {
	    register struct obj *obj;

	    obj = level.objects[u.ux][u.uy];
/*JP
	    You("sit on %s.", the(xname(obj)));
	    if(!Is_box(obj)) pline("It's not very comfortable...");
*/
	    You("%sに座った．", the(xname(obj)));
	    if(!Is_box(obj)) pline("あまり座りごこちがよくない．．．");

	} else if ((trap = t_at(u.ux, u.uy)) != 0) {

	    if (u.utrap) {
		exercise(A_WIS, FALSE);	/* you're getting stuck longer */
		if(u.utraptype == TT_BEARTRAP) {
/*JP
		    You_cant("sit down with your %s in the bear trap.", body_part(FOOT));
*/
		    pline("%sが熊の罠にはさまっているので座れない．", body_part(FOOT));
		    u.utrap++;
	        } else if(u.utraptype == TT_PIT) {
		    if(trap->ttyp == SPIKED_PIT) {
/*JP
			You("sit down on a spike.  Ouch!");
			losehp(1, "sitting on an iron spike", KILLED_BY);
*/
			You("トゲの上に座った．いてっ！");
			losehp(1, "鉄のトゲの上に座って", KILLED_BY);
			exercise(A_STR, FALSE);
		    } else
/*JP
			You("sit down in the pit.");
*/
			You("落し穴の中で座った．");
		    u.utrap += rn2(5);
		} else if(u.utraptype == TT_WEB) {
/*JP
		    You("sit in the spider web and get entangled further!");
*/
		    You("蜘蛛の巣の中で座ったら，より絡まった！");
		    u.utrap += rn1(10, 5);
		} else if(u.utraptype == TT_LAVA) {
		    /* Must have fire resistance or they'd be dead already */
/*JP
		    You("sit in the lava!");
*/
		    You("溶岩の中に座った！");
		    u.utrap += rnd(4);
/*JP
		    losehp(d(2,10), "sitting in lava", KILLED_BY);
*/
		    losehp(d(2,10), "溶岩の中に座って", KILLED_BY);
		} else if(u.utraptype == TT_INFLOOR) {
/*JP
		    You_cant("maneuver to sit!");
*/
		    You("座ったところで移動できない！");
		    u.utrap++;
		}
	    } else {
/*JP
	        You("sit down.");
*/
	        You("座った．");
		dotrap(trap);
	    }
	} else if(Underwater || Is_waterlevel(&u.uz)) {
	    if (Is_waterlevel(&u.uz))
/*JP
		pline("There are no cushions floating nearby.");
*/
		pline("近くに浮いているクッションはない．");
	    else
/*JP
		You("sit down on the muddy bottom.");
*/
		You("どろどろした底に座った．");
	} else if(is_pool(u.ux, u.uy)) {

/*JP
	    You("sit in the water.");
*/
	    You("水の中で座った．");
	    if (!rn2(10) && uarm)
/*JP
		(void) rust_dmg(uarm, "armor", 1, TRUE);
*/
		(void) rust_dmg(uarm, "鎧", 1, TRUE);
	    if (!rn2(10) && uarmf && uarmf->otyp != WATER_WALKING_BOOTS)
/*JP
		(void) rust_dmg(uarm, "armor", 1, TRUE);
*/
		(void) rust_dmg(uarm, "鎧", 1, TRUE);
#ifdef SINKS
	} else if(IS_SINK(typ)) {

/*JP
	    You(sit_message, defsyms[S_sink].explanation);
	    Your("%s gets wet.", humanoid(uasmon) ? "rump" : "underside");
*/
	    You(sit_message, jtrns_obj('S',defsyms[S_sink].explanation));
	    Your("%sは濡れた．", humanoid(uasmon) ? "尻" : "下部");
#endif
	} else if(IS_ALTAR(typ)) {

/*JP
	    You(sit_message, defsyms[S_altar].explanation);
*/
	    You(sit_message, jtrns_obj('S', defsyms[S_altar].explanation));
	    altar_wrath(u.ux, u.uy);

	} else if(typ == STAIRS) {

/*JP
	    You(sit_message, "stairs");
*/
	    You(sit_message, "階段");

	} else if(typ == LADDER) {

/*JP
	    You(sit_message, "ladder");
*/
	    You(sit_message, "梯子");

	} else if (is_lava(u.ux, u.uy)) {

	    /* must be WWalking */
/*JP
	    You(sit_message, "lava");
*/
	    You(sit_message, "溶岩");
	    if (likes_lava(uasmon)) {
/*JP
		pline_The("lava feels warm.");
*/
		pline("溶岩は暖かい．");
		return 1;
	    }
/*JP
	    pline_The("lava burns you!");
	    losehp(d((Fire_resistance ? 2 : 10), 10),
		   "sitting on lava", KILLED_BY);
*/
	    You("溶岩で燃えた！");
	    losehp(d((Fire_resistance ? 2 : 10), 10),
		   "溶岩に座って", KILLED_BY);

	} else if (is_ice(u.ux, u.uy)) {

/*JP
	    You(sit_message, defsyms[S_ice].explanation);
	    if (!Cold_resistance) pline_The("ice feels cold.");
*/
	    You(sit_message, jtrns_obj('S', defsyms[S_ice].explanation));
	    if (!Cold_resistance) pline("氷は冷たく感じた");

	} else if (typ == DRAWBRIDGE_DOWN) {

/*JP
	    You(sit_message, "drawbridge");
*/
	    You(sit_message, "跳ね橋");

	} else if(IS_THRONE(typ)) {

/*JP
	    You(sit_message, defsyms[S_throne].explanation);
*/
	    You(sit_message, jtrns_obj('S',defsyms[S_throne].explanation));
	    if (rnd(6) > 4)  {
		switch (rnd(13))  {
		    case 1:
			(void) adjattrib(rn2(A_MAX), -rn1(4,3), FALSE);
/*JP
			losehp(rnd(10), "cursed throne", KILLED_BY_AN);
*/
			losehp(rnd(10), "呪われた玉座で", KILLED_BY_AN);
			break;
		    case 2:
			(void) adjattrib(rn2(A_MAX), 1, FALSE);
			break;
		    case 3:
/*JP
			pline("A%s electric shock shoots through your body!",
			      (Shock_resistance) ? "n" : " massive");
			losehp(Shock_resistance ? rnd(6) : rnd(30),
			       "electric chair", KILLED_BY_AN);
*/
			pline("%s電気があなたの体を走り抜けた！",
			      (Shock_resistance) ? "" : "激しい");
			losehp(Shock_resistance ? rnd(6) : rnd(30),
			       "電気椅子で", KILLED_BY_AN);
			exercise(A_CON, FALSE);
			break;
		    case 4:
/*JP
			You_feel("much, much better!");
*/
			You("とても，とても元気になったような気がした！");
			if (Upolyd) {
			    if (u.mh >= (u.mhmax - 5))  u.mhmax += 4;
			    u.mh = u.mhmax;
			}
			if(u.uhp >= (u.uhpmax - 5))  u.uhpmax += 4;
			u.uhp = u.uhpmax;
			make_blinded(0L,TRUE);
			make_sick(0L, (char *) 0, FALSE, SICK_ALL);
			heal_legs();
			flags.botl = 1;
			break;
		    case 5:
			take_gold();
			break;
		    case 6:
			if(u.uluck + rn2(5) < 0) {
/*JP			    You_feel("your luck is changing.");*/
			    pline("運がよくなったような気がする．");
			    change_luck(1);
			} else	    makewish();
			break;
		    case 7:
			{
			register int cnt = rnd(10);

/*JP			pline("A voice echoes:");*/
			pline("声が響いた:");
/*JP			verbalize("Thy audience hath been summoned, %s!",
				  flags.female ? "Dame" : "Sire");*/
			verbalize("%sよ！汝の聴衆召喚されし．",
				  flags.female ? "女" : "男");
			while(cnt--)
			    (void) makemon(courtmon(), u.ux, u.uy, NO_MM_FLAGS);
			break;
			}
		    case 8:
/*JP			pline("A voice echoes:");*/
			pline("声が響いた:");
/*JP			verbalize("By thy Imperious order, %s...",
				  flags.female ? "Dame" : "Sire");*/
			verbalize("%sよ！汝の傲慢聞きいれようぞ．",
				  flags.female ? "女" : "男");
			do_genocide(1);
			break;
		    case 9:
/*JP			pline("A voice echoes:");*/
			pline("声が響いた:");
/*JP	verbalize("A curse upon thee for sitting upon this most holy throne!");*/
	verbalize("聖なる玉座に座りし汝に呪いあれ！");
			if (Luck > 0)  {
			    make_blinded(Blinded + rn1(100,250),TRUE);
			} else	    rndcurse();
			break;
		    case 10:
			if (Luck < 0 || (HSee_invisible & INTRINSIC))  {
				if (level.flags.nommap) {
					pline(
/*JP					"A terrible drone fills your head!");*/
					"恐しいブンブンという音が頭に響いた！");
					make_confused(HConfusion + rnd(30),
									FALSE);
				} else {
/*JP					pline("An image forms in your mind.");*/
					pline("イメージが頭に浮んだ．");
					do_mapping();
				}
			} else  {
/*JP				Your("vision becomes clear.");*/
				Your("視界は冴え渡った．");
				HSee_invisible |= FROMOUTSIDE;
				newsym(u.ux, u.uy);
			}
			break;
		    case 11:
			if (Luck < 0)  {
/*JP			    You_feel("threatened.");*/
			    You("脅迫されているような気がした．");
			    aggravate();
			} else  {

/*JP			    You_feel("a wrenching sensation.");*/
			    You("ねじられたような感覚を感じた．");

			    tele();		/* teleport him */
			}
			break;
		    case 12:
/*JP			You("are granted an insight!");*/
			You("洞察力を得た！");
			if (invent) {
			    /* rn2(5) agrees w/seffects() */
			    identify_pack(rn2(5));
			}
			break;
		    case 13:
/*JP			Your("mind turns into a pretzel!");*/
			Your("心はクネクネになった！");
			make_confused(HConfusion + rn1(7,16),FALSE);
			break;
		    default:	impossible("throne effect");
				break;
		}
/*JP	    } else	You_feel("somehow out of place...");*/
	    } else	You("何故か場違いの気がした．．．");

	    if (!rn2(3) && IS_THRONE(levl[u.ux][u.uy].typ)) {
		/* may have teleported */
/*JP		pline_The("throne vanishes in a puff of logic.");*/
		pline("玉座はふっと消えた．");
		levl[u.ux][u.uy].typ = ROOM;
		if(Invisible) newsym(u.ux,u.uy);
	    }

	} else if (lays_eggs(uasmon)) {
		struct obj *uegg;

		if (!flags.female) {
/*JP			pline("Males can't lay eggs!");*/
			pline("雄は卵を産めない！");
			return 0;
		}

		if (u.uhunger < (int)objects[EGG].oc_nutrition) {
/*JP			You("don't have enough energy to lay an egg.");*/
			You("卵を産むだけのエネルギーがない．");
			return 0;
		}

		uegg = mksobj(EGG, FALSE, FALSE);
		uegg->spe = 1;
		uegg->quan = 1;
		uegg->owt = weight(uegg);
		uegg->corpsenm = egg_type_from_parent(u.umonnum, FALSE);
		uegg->known = uegg->dknown = 1;
		attach_egg_hatch_timeout(uegg);
/*JP		You("lay an egg.");*/
		You("卵を産んだ");
		dropy(uegg);
		stackobj(uegg);
		morehungry((int)objects[EGG].oc_nutrition);
	} else if (u.uswallow)
/*JP		pline("There are no seats in here!");*/
		pline("ここには椅子はない！");
	else
/*JP		pline("Having fun sitting on the %s?", surface(u.ux,u.uy));*/
		pline("%sに座って楽しいかい？", surface(u.ux,u.uy));
	return(1);
}

void
rndcurse()			/* curse a few inventory items at random! */
{
	int	nobj = 0;
	int	cnt, onum;
	struct	obj	*otmp;
/*JP	static const char *mal_aura = "feel a malignant aura surround %s.";*/
	static const char *mal_aura = "邪悪なオーラを%sの回りに感じた．";

	if (uwep && (uwep->oartifact == ART_MAGICBANE) && rn2(20)) {
/*JP	    You(mal_aura, "the magic-absorbing blade");*/
	    You(mal_aura, "魔力を吸いとる刀");
	    return;
	}

	if(Antimagic) {
	    shieldeff(u.ux, u.uy);
/*JP	    You(mal_aura, "you");*/
	    You(mal_aura, "あなた");
	}

	for (otmp = invent; otmp; otmp = otmp->nobj)  nobj++;

	if (nobj)
	    for (cnt = rnd(6/((!!Antimagic) + (!!Half_spell_damage) + 1));
		 cnt > 0; cnt--)  {
		onum = rn2(nobj);
		for(otmp = invent; onum != 0; onum--)
		    otmp = otmp->nobj;

		if(otmp->oartifact && spec_ability(otmp, SPFX_INTEL) &&
		   rn2(10) < 8) {
/*JP		    pline("%s resists!", The(xname(otmp)));*/
		    pline("%sは影響を受けない！", The(xname(otmp)));
		    continue;
		}

		if(otmp->blessed)
			unbless(otmp);
		else
			curse(otmp);
	    }
}

void
attrcurse()			/* remove a random INTRINSIC ability */
{
	switch(rnd(11)) {
	case 1 : if (HFire_resistance & INTRINSIC) {
			HFire_resistance &= ~INTRINSIC;
/*JP			You_feel("warmer.");*/
			You("暖かさを感じた．");
			break;
		}
	case 2 : if (HTeleportation & INTRINSIC) {
			HTeleportation &= ~INTRINSIC;
/*JP			You_feel("less jumpy.");*/
			You("ちょっと神経過敏になった．");
			break;
		}
	case 3 : if (HPoison_resistance & INTRINSIC) {
			HPoison_resistance &= ~INTRINSIC;
/*JP			You_feel("a little sick!");*/
			You("少し気分が悪くなった！");
			break;
		}
	case 4 : if (HTelepat & INTRINSIC) {
			HTelepat &= ~INTRINSIC;
			if (Blind && !Telepat)
			    see_monsters();	/* Can't sense mons anymore! */
/*JP			Your("senses fail!");*/
			Your("五感は麻痺した！");
			break;
		}
	case 5 : if (HCold_resistance & INTRINSIC) {
			HCold_resistance &= ~INTRINSIC;
/*JP			You_feel("cooler.");*/
			You("涼しさを感じた．");
			break;
		}
	case 6 : if (HInvis & INTRINSIC) {
			HInvis &= ~INTRINSIC;
/*JP			You_feel("paranoid.");*/
			You("妄想を抱いた．");
			break;
		}
	case 7 : if (HSee_invisible & INTRINSIC) {
			HSee_invisible &= ~INTRINSIC;
/*JP			You("%s!", Hallucination ? "tawt you taw a puttie tat"
						: "thought you saw something");*/
			if(Hallucination)
			  You("だれ蟹みら，れている．");
			else
			  You("誰かに見られているような気がした！");
			break;
		}
	case 8 : if (Fast & INTRINSIC) {
			Fast &= ~INTRINSIC;
/*JP			You_feel("slower.");*/
			You("遅くなったような気がした．");
			break;
		}
	case 9 : if (Stealth & INTRINSIC) {
			Stealth &= ~INTRINSIC;
/*JP			You_feel("clumsy.");*/
			You("不器用になったような気がした．");
			break;
		}
	case 10: if (Protection & INTRINSIC) {
			Protection &= ~INTRINSIC;
/*JP			You_feel("vulnerable.");*/
			You("目立つようになった気がした．");
			break;
		}
	case 11: if (Aggravate_monster & INTRINSIC) {
			Aggravate_monster &= ~INTRINSIC;
/*JP			You_feel("less attractive.");*/
			You("魅力が失せたような気がした．");
			break;
		}
	default: break;
	}
}

/*sit.c*/
