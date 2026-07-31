/*	SCCS Id: @(#)do_wear.c	3.2	96/02/29	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"

#ifndef OVLB

STATIC_DCL long takeoff_mask, taking_off;

#else /* OVLB */

STATIC_OVL NEARDATA long takeoff_mask = 0L, taking_off = 0L;

static NEARDATA int todelay;

static NEARDATA const char see_yourself[] = "see yourself";
static NEARDATA const char unknown_type[] = "Unknown type of %s (%d)";
/*JP
static NEARDATA const char *c_armor  = "armor",
			   *c_suit   = "suit",
#ifdef TOURIST
			   *c_shirt  = "shirt",
#endif
			   *c_cloak  = "cloak",
			   *c_gloves = "gloves",
			   *c_boots  = "boots",
			   *c_helmet = "helmet",
			   *c_shield = "shield",
			   *c_weapon = "weapon",
			   *c_sword  = "sword",
			   *c_axe    = "axe",
			   *c_that_  = "that";
*/
static NEARDATA const char *c_armor  = "鎧",
			   *c_suit   = "服",
#ifdef TOURIST
			   *c_shirt  = "シャツ",
#endif
			   *c_cloak  = "クローク",
			   *c_gloves = "小手",
			   *c_boots  = "靴",
			   *c_helmet = "兜",
			   *c_shield = "盾",
			   *c_weapon = "武器",
			   *c_sword  = "剣",
			   *c_axe    = "斧",
			   *c_that_  = "それ";

static NEARDATA const long takeoff_order[] = { WORN_BLINDF, 1L, /* weapon */
	WORN_SHIELD, WORN_GLOVES, LEFT_RING, RIGHT_RING, WORN_CLOAK,
	WORN_HELMET, WORN_AMUL, WORN_ARMOR,
#ifdef TOURIST
	WORN_SHIRT,
#endif
	WORN_BOOTS, 0L };

static void FDECL(on_msg, (struct obj *));
STATIC_PTR int NDECL(Armor_on);
STATIC_PTR int NDECL(Boots_on);
static int NDECL(Cloak_on);
STATIC_PTR int NDECL(Helmet_on);
STATIC_PTR int NDECL(Gloves_on);
static void NDECL(Amulet_on);
static void FDECL(Ring_off_or_gone, (struct obj *, BOOLEAN_P));
STATIC_PTR int FDECL(select_off, (struct obj *));
static struct obj *NDECL(do_takeoff);
STATIC_PTR int NDECL(take_off);
static int FDECL(menu_remarm, (int));
/*JP
static void FDECL(already_wearing, (const char*));
*/
static void FDECL(already_wearing, (const char*, struct obj*));

void
off_msg(otmp)
register struct obj *otmp;
{
  const char *j;
  const char *m;
  m = joffmsg(otmp, &j);

	if(flags.verbose)
/*JP	    You("were wearing %s.", doname(otmp));*/
	  You("%s%s%s．", doname(otmp), j, jpast(m));
}

/* for items that involve no delay */
static void
on_msg(otmp)
register struct obj *otmp;
{
  const char *j;
  const char *m;
  m = jonmsg(otmp, &j);

	if(flags.verbose)
/*JP	    You("are now wearing %s.",*/
/*JP	      obj_is_pname(otmp) ? the(xname(otmp)) : an(xname(otmp)));*/
	  You("%s%s%s．", xname(otmp), j, jpast(m));
}

/*
 * The Type_on() functions should be called *after* setworn().
 * The Type_off() functions call setworn() themselves.
 */


STATIC_PTR
int
Boots_on()
{
    long oldprop = u.uprops[objects[uarmf->otyp].oc_oprop].p_flgs & ~WORN_BOOTS;

    switch(uarmf->otyp) {
	case LOW_BOOTS:
	case IRON_SHOES:
	case HIGH_BOOTS:
	case JUMPING_BOOTS:
		break;
	case WATER_WALKING_BOOTS:
		if (u.uinwater) spoteffects();
		break;
	case SPEED_BOOTS:
		/* Speed boots are still better than intrinsic speed, */
		/* though not better than potion speed */
		if (!(oldprop & TIMEOUT)) {
			makeknown(uarmf->otyp);
/*JP			You_feel("yourself speed up%s.",
				oldprop ? " a bit more" : "");*/
			You("%s素早くなったような気がした．",
				oldprop ? "さらに" : "");
		}
		break;
	case ELVEN_BOOTS:
		if (!oldprop) {
			makeknown(uarmf->otyp);
/*JP			You("walk very quietly.");*/
			Your("足音は小さくなった．");
		}
		break;
	case FUMBLE_BOOTS:
		if (!(oldprop & ~TIMEOUT))
			Fumbling += rnd(20);
		break;
	case LEVITATION_BOOTS:
		if (!oldprop) {
			makeknown(uarmf->otyp);
			float_up();
		}
		break;
	default: impossible(unknown_type, c_boots, uarmf->otyp);
    }
    return 0;
}

int
Boots_off()
{
    int otyp = uarmf->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].p_flgs & ~WORN_BOOTS;

	/* For levitation, float_down() returns if Levitation, so we
	 * must do a setworn() _before_ the levitation case.
	 */
    setworn((struct obj *)0, W_ARMF);
    switch (otyp) {
	case SPEED_BOOTS:
		if (!(oldprop & TIMEOUT)) {
			makeknown(otyp);
/*JP			You_feel("yourself slow down%s.",
				oldprop ? " a bit" : "");*/
			You("%sのろくなったような気がした．",
				oldprop ? "ちょっと" : "");
		}
		break;
	case WATER_WALKING_BOOTS:
		if (is_pool(u.ux,u.uy) && !Levitation
			    && !is_flyer(uasmon) && !is_clinger(uasmon)) {
			makeknown(otyp);
			/* make boots known in case you survive the drowning */
			spoteffects();
		}
		break;
	case ELVEN_BOOTS:
		if (!oldprop) {
			makeknown(otyp);
/*JP			You("sure are noisy.");*/
			Your("足音は大きくなった．");
		}
		break;
	case FUMBLE_BOOTS:
		if (!(oldprop & ~TIMEOUT))
			Fumbling = 0;
		break;
	case LEVITATION_BOOTS:
		if (!oldprop) {
			(void) float_down(0L);
			makeknown(otyp);
		}
		break;
	case LOW_BOOTS:
	case IRON_SHOES:
	case HIGH_BOOTS:
	case JUMPING_BOOTS:
		break;
	default: impossible(unknown_type, c_boots, otyp);
    }
    return 0;
}

static int
Cloak_on()
{
    long oldprop = u.uprops[objects[uarmc->otyp].oc_oprop].p_flgs & ~WORN_CLOAK;

    switch(uarmc->otyp) {
	case ELVEN_CLOAK:
	case CLOAK_OF_PROTECTION:
	case CLOAK_OF_DISPLACEMENT:
		makeknown(uarmc->otyp);
		break;
	case ORCISH_CLOAK:
	case DWARVISH_CLOAK:
	case CLOAK_OF_MAGIC_RESISTANCE:
		break;
	case MUMMY_WRAPPING:
		/* Note: it's already being worn, so we have to cheat here. */
		if (((HInvis & ~I_BLOCKED) || pm_invisible(uasmon))
		    && !See_invisible && !Blind) {
			newsym(u.ux,u.uy);
/*JP			You("can %s!", see_yourself);*/
			pline("自分自身が見えるようになった．");
		}
		break;
	case CLOAK_OF_INVISIBILITY:
		/* since cloak of invisibility was worn, we know mummy wrapping
		   wasn't, so no need to check `oldprop' against I_BLOCKED */
		if (!oldprop && !See_invisible && !Blind) {
			makeknown(uarmc->otyp);
			newsym(u.ux,u.uy);
/*JP			pline("Suddenly you cannot %s.", see_yourself);*/
			pline("突然，自分自身が見えなくなった．");
		}
		break;
	case OILSKIN_CLOAK:
/*JP		pline("%s fits very tightly.",The(xname(uarmc)));*/
		pline("%sはとてもぴっちり合う．",The(xname(uarmc)));
		break;
	default: impossible(unknown_type, c_cloak, uarmc->otyp);
    }
    return 0;
}

int
Cloak_off()
{
    int otyp = uarmc->otyp;
    long oldprop = u.uprops[objects[otyp].oc_oprop].p_flgs & ~WORN_CLOAK;

	/* For mummy wrapping, taking it off first resets `Invisible'. */
    setworn((struct obj *)0, W_ARMC);
    switch (otyp) {
	case ELVEN_CLOAK:
	case ORCISH_CLOAK:
	case DWARVISH_CLOAK:
	case CLOAK_OF_PROTECTION:
	case CLOAK_OF_MAGIC_RESISTANCE:
	case CLOAK_OF_DISPLACEMENT:
	case OILSKIN_CLOAK:
		break;
	case MUMMY_WRAPPING:
		if (Invisible && !Blind) {
			newsym(u.ux,u.uy);
/*JP			You("can no longer %s.", see_yourself);*/
			You("自分自身が見えなくなった．");
		}
		break;
	case CLOAK_OF_INVISIBILITY:
		if (!oldprop && !See_invisible && !Blind) {
			makeknown(CLOAK_OF_INVISIBILITY);
			newsym(u.ux,u.uy);
/*JP			pline("Suddenly you can %s.", see_yourself);*/
			pline("突然，自分自身が見えるようになった．");
		}
		break;
	default: impossible(unknown_type, c_cloak, otyp);
    }
    return 0;
}

STATIC_PTR
int
Helmet_on()
{
    switch(uarmh->otyp) {
	case FEDORA:
	case HELMET:
	case DENTED_POT:
	case ELVEN_LEATHER_HELM:
	case DWARVISH_IRON_HELM:
	case ORCISH_HELM:
	case HELM_OF_TELEPATHY:
		break;
	case HELM_OF_BRILLIANCE:
		adj_abon(uarmh, uarmh->spe);
		break;
	case CORNUTHAUM:
		/* people think marked wizards know what they're talking
		 * about, but it takes trained arrogance to pull it off,
		 * and the actual enchantment of the hat is irrelevant.
		 */
		ABON(A_CHA) += (Role_is('W') ? 1 : -1);
		flags.botl = 1;
		makeknown(uarmh->otyp);
		break;
	case HELM_OF_OPPOSITE_ALIGNMENT:
		if (u.ualign.type == A_NEUTRAL)
		    u.ualign.type = rn2(2) ? A_CHAOTIC : A_LAWFUL;
		else u.ualign.type = -(u.ualign.type);
		u.ublessed = 0; /* lose your god's protection */
	     /* makeknown(uarmh->otyp);   -- moved below, after xname() */
		/*FALLTHRU*/
	case DUNCE_CAP:
		if (!uarmh->cursed) {
/*JP		    pline("%s %s%s for a moment.", The(xname(uarmh)),
			  Blind ? "vibrates" : "glows ",
			  Blind ? (const char *)"" : hcolor(Black));*/
		    pline("%sは一瞬%s%s．", The(xname(uarmh)),
                          Blind ? (const char *)"" : jconj_adj(hcolor(Black)),
                          Blind ? "震えた" : "輝いた");
		    curse(uarmh);
		}
		flags.botl = 1;		/* reveal new alignment or INT & WIS */
		if (Hallucination) {
/*JP		    pline("My brain hurts!"); * Monty Python's Flying Circus */
		    pline("のーみーそバーン！"); /*モンティパイソンとはちょっと違うけど*/			    
		} else if (uarmh->otyp == DUNCE_CAP) {
/*JP		    You_feel("%s.",	* track INT change; ignore WIS *
		  ACURR(A_INT) <= (ABASE(A_INT) + ABON(A_INT) + ATEMP(A_INT)) ?
			     "like sitting in a corner" : "giddy");*/
		    You("%sような気がした．",
		  ACURR(A_INT) <= (ABASE(A_INT) + ABON(A_INT) + ATEMP(A_INT)) ?
			     "街角に座っている" : "目がまわった");
		} else {
/*JP		    Your("mind oscillates briefly.");*/
		    You("少しも迷わずねがえった．");
		    makeknown(HELM_OF_OPPOSITE_ALIGNMENT);
		}
		break;
	default: impossible(unknown_type, c_helmet, uarmh->otyp);
    }
    return 0;
}

int
Helmet_off()
{
    switch(uarmh->otyp) {
	case FEDORA:
	case HELMET:
	case DENTED_POT:
	case ELVEN_LEATHER_HELM:
	case DWARVISH_IRON_HELM:
	case ORCISH_HELM:
		break;
	case DUNCE_CAP:
		flags.botl = 1;
		break;
	case CORNUTHAUM:
		ABON(A_CHA) += (Role_is('W') ? -1 : 1);
		flags.botl = 1;
		break;
	case HELM_OF_TELEPATHY:
		/* need to update ability before calling see_monsters() */
		setworn((struct obj *)0, W_ARMH);
		see_monsters();
		return 0;
	case HELM_OF_BRILLIANCE:
		adj_abon(uarmh, -uarmh->spe);
		break;
	case HELM_OF_OPPOSITE_ALIGNMENT:
		u.ualign.type = u.ualignbase[0];
		u.ublessed = 0; /* lose the other god's protection */
		flags.botl = 1;
		break;
	default: impossible(unknown_type, c_helmet, uarmh->otyp);
    }
    setworn((struct obj *)0, W_ARMH);
    return 0;
}

STATIC_PTR
int
Gloves_on()
{
    long oldprop =
	u.uprops[objects[uarmg->otyp].oc_oprop].p_flgs & ~(WORN_GLOVES | TIMEOUT);

    switch(uarmg->otyp) {
	case LEATHER_GLOVES:
		break;
	case GAUNTLETS_OF_FUMBLING:
		if (!oldprop)
			Fumbling += rnd(20);
		break;
	case GAUNTLETS_OF_POWER:
		makeknown(uarmg->otyp);
		flags.botl = 1; /* taken care of in attrib.c */
		break;
	case GAUNTLETS_OF_DEXTERITY:
		adj_abon(uarmg, uarmg->spe);
		break;
	default: impossible(unknown_type, c_gloves, uarmg->otyp);
    }
    return 0;
}

int
Gloves_off()
{
    long oldprop =
	u.uprops[objects[uarmg->otyp].oc_oprop].p_flgs & ~(WORN_GLOVES | TIMEOUT);

    switch(uarmg->otyp) {
	case LEATHER_GLOVES:
		break;
	case GAUNTLETS_OF_FUMBLING:
		if (!oldprop)
			Fumbling = 0;
		break;
	case GAUNTLETS_OF_POWER:
		makeknown(uarmg->otyp);
		flags.botl = 1; /* taken care of in attrib.c */
		break;
	case GAUNTLETS_OF_DEXTERITY:
		adj_abon(uarmg, -uarmg->spe);
		break;
	default: impossible(unknown_type, c_gloves, uarmg->otyp);
    }
    setworn((struct obj *)0, W_ARMG);
    if (uwep && uwep->otyp == CORPSE && uwep->corpsenm == PM_COCKATRICE) {
	/* Prevent wielding cockatrice when not wearing gloves */
/*JP	You("wield the cockatrice corpse in your bare %s.",*/
	You("コカトリスの死体を素%sで持った．",
	    makeplural(body_part(HAND)));
/*JP	instapetrify("cockatrice corpse");*/
	instapetrify("コカトリスの死体に触れて");
	uwepgone();  /* life-saved still doesn't allow touching cockatrice */
    }
    return 0;
}

/*
static int
Shield_on()
{
    switch(uarms->otyp) {
	case SMALL_SHIELD:
	case ELVEN_SHIELD:
	case URUK_HAI_SHIELD:
	case ORCISH_SHIELD:
	case DWARVISH_ROUNDSHIELD:
	case LARGE_SHIELD:
	case SHIELD_OF_REFLECTION:
		break;
	default: impossible(unknown_type, c_shield, uarms->otyp);
    }
    return 0;
}
*/

int
Shield_off()
{
/*
    switch(uarms->otyp) {
	case SMALL_SHIELD:
	case ELVEN_SHIELD:
	case URUK_HAI_SHIELD:
	case ORCISH_SHIELD:
	case DWARVISH_ROUNDSHIELD:
	case LARGE_SHIELD:
	case SHIELD_OF_REFLECTION:
		break;
	default: impossible(unknown_type, c_shield, uarms->otyp);
    }
*/
    setworn((struct obj *)0, W_ARMS);
    return 0;
}

/* This must be done in worn.c, because one of the possible intrinsics conferred
 * is fire resistance, and we have to immediately set HFire_resistance in worn.c
 * since worn.c will check it before returning.
 */
STATIC_PTR
int
Armor_on()
{
#ifdef FIGHTER
  switch(uarm->otyp) {
  case SAILOR_BLOUSE:
    if(flags.female || Role_is('F'))
      ;
    else
      curse(uarm);
    break;
  }
#endif
  return 0;
}

int
Armor_off()
{
    setworn((struct obj *)0, W_ARM);
    return 0;
}

/* The gone functions differ from the off functions in that if you die from
 * taking it off and have life saving, you still die.
 */
int
Armor_gone()
{
    setnotworn(uarm);
    return 0;
}

static void
Amulet_on()
{
    switch(uamul->otyp) {
	case AMULET_OF_ESP:
	case AMULET_OF_LIFE_SAVING:
	case AMULET_VERSUS_POISON:
	case AMULET_OF_REFLECTION:
	case AMULET_OF_MAGICAL_BREATHING:
	case FAKE_AMULET_OF_YENDOR:
		break;
	case AMULET_OF_CHANGE:
		makeknown(AMULET_OF_CHANGE);
		change_sex();
		/* Don't use same message as polymorph */
/*JP		You("are suddenly very %s!", flags.female ? "feminine"
			: "masculine");*/
		You("突然%s！", flags.female ? "女っぽくなった"
			: "筋肉質になった");
		flags.botl = 1;
/*JP		pline_The("amulet disintegrates!");*/
		pline("魔除けはこなごなになった！");
		useup(uamul);
		break;
	case AMULET_OF_STRANGULATION:
		makeknown(AMULET_OF_STRANGULATION);
/*JP		pline("It constricts your throat!");*/
		pline("魔除けはあなたの喉を絞めつけた！");
		Strangled = 6;
		break;
	case AMULET_OF_RESTFUL_SLEEP:
		Sleeping = rnd(100);
		break;
	case AMULET_OF_YENDOR:
		break;
    }
}

void
Amulet_off()
{
    switch(uamul->otyp) {
	case AMULET_OF_ESP:
		/* need to update ability before calling see_monsters() */
		setworn((struct obj *)0, W_AMUL);
		see_monsters();
		return;
	case AMULET_OF_LIFE_SAVING:
	case AMULET_VERSUS_POISON:
	case AMULET_OF_REFLECTION:
	case FAKE_AMULET_OF_YENDOR:
		break;
	case AMULET_OF_MAGICAL_BREATHING:
		if (Underwater) {
		    if (!breathless(uasmon) && !amphibious(uasmon)
						&& !is_swimmer(uasmon))
/*JP			You("suddenly inhale an unhealthy amount of water!");*/
			You("突然，大量の水を吸いこんだ！");
		    /* HMagical_breathing must be set off
		       before calling drown() */
		    setworn((struct obj *)0, W_AMUL);
		    (void) drown();
		    return;
		}
		break;
	case AMULET_OF_CHANGE:
		impossible("Wearing an amulet of change?");
		break;
	case AMULET_OF_STRANGULATION:
		if (Strangled) {
/*JP			You("can breathe more easily!");*/
			You("楽に呼吸できるようになった！");
			Strangled = 0;
		}
		break;
	case AMULET_OF_RESTFUL_SLEEP:
		Sleeping = 0;
		break;
	case AMULET_OF_YENDOR:
		break;
    }
    setworn((struct obj *)0, W_AMUL);
}

void
Ring_on(obj)
register struct obj *obj;
{
    long oldprop = u.uprops[objects[obj->otyp].oc_oprop].p_flgs;
    int old_attrib;

    /* only mask out W_RING when we don't have both
       left and right rings of the same type */
    if ((oldprop & W_RING) != W_RING) oldprop &= ~W_RING;

    switch(obj->otyp){
	case RIN_TELEPORTATION:
	case RIN_REGENERATION:
	case RIN_SEARCHING:
	case RIN_STEALTH:
	case RIN_HUNGER:
	case RIN_AGGRAVATE_MONSTER:
	case RIN_POISON_RESISTANCE:
	case RIN_FIRE_RESISTANCE:
	case RIN_COLD_RESISTANCE:
	case RIN_SHOCK_RESISTANCE:
	case RIN_CONFLICT:
	case RIN_WARNING:
	case RIN_TELEPORT_CONTROL:
	case RIN_POLYMORPH:
	case RIN_POLYMORPH_CONTROL:
		break;
	case RIN_SEE_INVISIBLE:
		/* can now see invisible monsters */
		set_mimic_blocking(); /* do special mimic handling */
		see_monsters();

		if (Invis && !oldprop && !perceives(uasmon) && !Blind) {
			newsym(u.ux,u.uy);
/*JP			pline("Suddenly you can see yourself.");*/
			pline("突然，自分自身が見えるようになった．");
			makeknown(RIN_SEE_INVISIBLE);
		}
		break;
	case RIN_INVISIBILITY:
		if (!oldprop && !See_invisible && !Blind) {
			makeknown(RIN_INVISIBILITY);
			newsym(u.ux,u.uy);
/*JP			Your("body takes on a %s transparency...",
				Hallucination ? "normal" : "strange");*/
			pline("%sあなたの体は透過性をもった．．．",
				Hallucination ? "あたりまえなことだが" : "奇妙なことに");
		}
		break;
	case RIN_ADORNMENT:
		old_attrib = ACURR(A_CHA);
		ABON(A_CHA) += obj->spe;
		flags.botl = 1;
		if (ACURR(A_CHA) != old_attrib ||
		    (objects[RIN_ADORNMENT].oc_name_known &&
		     old_attrib != 25 && old_attrib != 3)) {
			makeknown(RIN_ADORNMENT);
			obj->known = TRUE;
		}
		break;
	case RIN_LEVITATION:
		if(!oldprop) {
			float_up();
			makeknown(RIN_LEVITATION);
			obj->known = TRUE;
		}
		break;
	case RIN_GAIN_STRENGTH:
		old_attrib = ACURR(A_STR);
		ABON(A_STR) += obj->spe;
		flags.botl = 1;
		if (ACURR(A_STR) != old_attrib ||
		    (objects[RIN_GAIN_STRENGTH].oc_name_known &&
		     old_attrib != 125 && old_attrib != 3)) {
			makeknown(RIN_GAIN_STRENGTH);
			obj->known = TRUE;
		}
		break;
	case RIN_INCREASE_DAMAGE:
		u.udaminc += obj->spe;
		break;
	case RIN_PROTECTION_FROM_SHAPE_CHAN:
		rescham();
		break;
	case RIN_PROTECTION:
		flags.botl = 1;
		if (obj->spe || objects[RIN_PROTECTION].oc_name_known) {
			makeknown(RIN_PROTECTION);
			obj->known = TRUE;
		}
		break;
    }
}

static void
Ring_off_or_gone(obj,gone)
register struct obj *obj;
boolean gone;
{
    register long mask = obj->owornmask & W_RING;
    int old_attrib;

    if(!(u.uprops[objects[obj->otyp].oc_oprop].p_flgs & mask))
	impossible("Strange... I didn't know you had that ring.");

    if(gone) setnotworn(obj);
    else setworn((struct obj *)0, obj->owornmask);
    switch(obj->otyp) {
	case RIN_TELEPORTATION:
	case RIN_REGENERATION:
	case RIN_SEARCHING:
	case RIN_STEALTH:
	case RIN_HUNGER:
	case RIN_AGGRAVATE_MONSTER:
	case RIN_POISON_RESISTANCE:
	case RIN_FIRE_RESISTANCE:
	case RIN_COLD_RESISTANCE:
	case RIN_SHOCK_RESISTANCE:
	case RIN_CONFLICT:
	case RIN_WARNING:
	case RIN_TELEPORT_CONTROL:
	case RIN_POLYMORPH:
	case RIN_POLYMORPH_CONTROL:
		break;
	case RIN_SEE_INVISIBLE:
		/* Make invisible monsters go away */
		if (!See_invisible) {
		    set_mimic_blocking(); /* do special mimic handling */
		    see_monsters();
		}

		if (Invisible && !Blind) {
			newsym(u.ux,u.uy);
/*JP			pline("Suddenly you cannot see yourself.");*/
			pline("突然，自分自身が見えなくなった．");
			makeknown(RIN_SEE_INVISIBLE);
		}
		break;
	case RIN_INVISIBILITY:
		if (!Invis && !(HInvis & I_BLOCKED) &&
		    !See_invisible && !Blind) {
			newsym(u.ux,u.uy);
/*JP			Your("body seems to unfade...");*/
			Your("体が次第に現われてきた．．．");
			makeknown(RIN_INVISIBILITY);
		}
		break;
	case RIN_ADORNMENT:
		old_attrib = ACURR(A_CHA);
		ABON(A_CHA) -= obj->spe;
		if (ACURR(A_CHA) != old_attrib) makeknown(RIN_ADORNMENT);
		flags.botl = 1;
		break;
	case RIN_LEVITATION:
		(void) float_down(0L);
		if (!Levitation) makeknown(RIN_LEVITATION);
		break;
	case RIN_GAIN_STRENGTH:
		old_attrib = ACURR(A_STR);
		ABON(A_STR) -= obj->spe;
		if (ACURR(A_STR) != old_attrib) makeknown(RIN_GAIN_STRENGTH);
		flags.botl = 1;
		break;
	case RIN_INCREASE_DAMAGE:
		u.udaminc -= obj->spe;
		break;
	case RIN_PROTECTION_FROM_SHAPE_CHAN:
		/* If you're no longer protected, let the chameleons
		 * change shape again -dgk
		 */
		restartcham();
		break;
    }
}

void
Ring_off(obj)
struct obj *obj;
{
	Ring_off_or_gone(obj,FALSE);
}

void
Ring_gone(obj)
struct obj *obj;
{
	Ring_off_or_gone(obj,TRUE);
}

void
Blindf_on(otmp)
register struct obj *otmp;
{
	long already_blinded = Blinded;

	if (otmp == uwep)
	    setuwep((struct obj *) 0);
	setworn(otmp, W_TOOL);
	if (otmp->otyp == TOWEL && flags.verbose)
/*HP	    You("wrap %s around your %s.", an(xname(otmp)), body_part(HEAD));*/
	    You("%sに%sを巻きつけた．", body_part(HEAD), xname(otmp));
	on_msg(otmp);
	if (!already_blinded) {
	    if (Punished) set_bc(0);	/* Set ball&chain variables before */
					/* the hero goes blind.		   */
	    if (Telepat) see_monsters();/* sense monsters */
	    vision_full_recalc = 1;	/* recalc vision limits */
	    flags.botl = 1;
	}
}

void
Blindf_off(otmp)
register struct obj *otmp;
{
	setworn((struct obj *)0, otmp->owornmask);
	off_msg(otmp);
	if (!Blinded) {
	    if (Telepat) see_monsters();/* no longer sense monsters */
	    vision_full_recalc = 1;	/* recalc vision limits */
	    flags.botl = 1;
	} else
/*JP	    You("still cannot see.");*/
	    You("まだ目が見えない．");
}

/* called in main to set intrinsics of worn start-up items */
void
set_wear()
{
	if (uarm)  (void) Armor_on();
	if (uarmc) (void) Cloak_on();
	if (uarmf) (void) Boots_on();
	if (uarmg) (void) Gloves_on();
	if (uarmh) (void) Helmet_on();
/*	if (uarms) (void) Shield_on(); */
}

boolean
donning(otmp)
register struct obj *otmp;
{
    return((boolean)((otmp == uarmf && (afternmv == Boots_on || afternmv == Boots_off))
	|| (otmp == uarmh && (afternmv == Helmet_on || afternmv == Helmet_off))
	|| (otmp == uarmg && (afternmv == Gloves_on || afternmv == Gloves_off))
	|| (otmp == uarm && (afternmv == Armor_on || afternmv == Armor_off))));
}

void
cancel_don()
{
	/* the piece of armor we were donning/doffing has vanished, so stop
	 * wasting time on it (and don't dereference it when donning would
	 * otherwise finish)
	 */
	afternmv = 0;
	nomovemsg = (char *)0;
	multi = 0;
}

static NEARDATA const char clothes[] = {ARMOR_CLASS, 0};
static NEARDATA const char accessories[] = {RING_CLASS, AMULET_CLASS, TOOL_CLASS, 0};

int
dotakeoff()
{
	register struct obj *otmp = (struct obj *)0;
	int armorpieces = 0;
	const char *j;
	const char *m;

#define MOREARM(x) if (x) { armorpieces++; otmp = x; }
	MOREARM(uarmh);
	MOREARM(uarms);
	MOREARM(uarmg);
	MOREARM(uarmf);
	if (uarmc) {
		armorpieces++;
		otmp = uarmc;
	} else if (uarm) {
		armorpieces++;
		otmp = uarm;
#ifdef TOURIST
	} else if (uarmu) {
		armorpieces++;
		otmp = uarmu;
#endif
	}
	if (!armorpieces) {
	     /* assert( GRAY_DRAGON_SCALES > YELLOW_DRAGON_SCALE_MAIL ); */
		if (uskin)
/*JP		    pline_The("%s merged with your skin!",
			      uskin->otyp >= GRAY_DRAGON_SCALES ?
				"dragon scales are" : "dragon scale mail is");*/
		    pline("%sはあなたの肌と融合してしまっている！",
			      uskin->otyp >= GRAY_DRAGON_SCALES ?
				"ドラゴンの鱗" : "ドラゴンの鱗鎧");
		else
/*JP		    pline("Not wearing any armor.");*/
		    pline("鎧の類を装備していない．");
		return 0;
	}
	if (armorpieces > 1)
/*JP		otmp = getobj(clothes, "take off");*/
		otmp = getobj(clothes, "の装備を解く");
	if (otmp == 0) return(0);
	if (!(otmp->owornmask & W_ARMOR)) {
/*JP		You("are not wearing that.");*/
		You("それは装備していない．");
		return(0);
	}
	/* note: the `uskin' case shouldn't be able to happen here; dragons
	   can't wear any armor so will end up with `armorpieces == 0' above */
	if (otmp == uskin || ((otmp == uarm) && uarmc)
#ifdef TOURIST
			  || ((otmp == uarmu) && (uarmc || uarm))
#endif
		) {
/*JP	    You_cant("take that off.");*/
	    m = joffmsg(otmp, &j);
	    You("それ%s%sことはできない．", j, m);
	    return 0;
	}
	if (otmp == uarmg && welded(uwep)) {
/*JP	    You("seem unable to take off the gloves while holding your %s.",
		is_sword(uwep) ? c_sword : c_weapon);*/
	    You("%sを持ったまま小手をはずすことはできない．",
		is_sword(uwep) ? c_sword : c_weapon);
	    uwep->bknown = TRUE;
	    return 0;
	}
	if (otmp == uarmg && Glib) {
/*JP	    You_cant("remove the slippery gloves with your slippery fingers.");*/
	    You("滑りやすい小手を滑りやすい指からはずせない．");
	    return 0;
	}
	if (otmp == uarmf && u.utrap && (u.utraptype == TT_BEARTRAP ||
					u.utraptype == TT_INFLOOR)) { /* -3. */
	    if(u.utraptype == TT_BEARTRAP)
/*JP		pline_The("bear trap prevents you from pulling your %s out.",*/
		pline("%sが熊の罠につかまっているので脱ぐことができない．",
		      body_part(FOOT));
	    else
/*JP		You("are stuck in the %s, and cannot pull your %s out.",*/
		You("%sが%sにはまっているので脱ぐことができない．",
		    surface(u.ux, u.uy), makeplural(body_part(FOOT)));
		return(0);
	}
	reset_remarm();			/* since you may change ordering */
	(void) armoroff(otmp);
	return(1);
}

int
doremring()
{
	register struct obj *otmp = 0;
	int Accessories = 0;

#define MOREACC(x) if (x) { Accessories++; otmp = x; }
	MOREACC(uleft);
	MOREACC(uright);
	MOREACC(uamul);
	MOREACC(ublindf);

	if(!Accessories) {
/*JP		pline("Not wearing any accessories.");*/
		pline("装飾品は身につけていない．");
		return(0);
	}
/*JP	if (Accessories != 1) otmp = getobj(accessories, "take off");*/
	if (Accessories != 1) otmp = getobj(accessories, "はずす");
	if(!otmp) return(0);
	if(!(otmp->owornmask & (W_RING | W_AMUL | W_TOOL))) {
/*JP		You("are not wearing that.");*/
		You("それは身につけていない．");
		return(0);
	}
	if(cursed(otmp)) return(0);
	if(otmp->oclass == RING_CLASS) {
		if (nolimbs(uasmon)) {
/*JP			pline("It seems to be stuck.");*/
			pline("体に埋まってしまってはずせない．");
			return(0);
		}
		if (uarmg && uarmg->cursed) {
			uarmg->bknown = TRUE;
			You(
/*JP	    "seem unable to remove your ring without taking off your gloves.");*/
	    "小手を外さないことには指輪をはずせない．");
			return(0);
		}
		if (welded(uwep) && bimanual(uwep)) {
			uwep->bknown = TRUE;
			You(
/*JP	       "seem unable to remove the ring while your hands hold your %s.",*/
		"%sを手にしているので指輪をはずせない．",
			    is_sword(uwep) ? c_sword : c_weapon);
			return(0);
		}
		if (welded(uwep) && otmp==uright) {
			uwep->bknown = TRUE;
			You(
/*JP	 "seem unable to remove the ring while your right hand holds your %s.",*/
	 "右手が%sで塞がっているのではずせない．",
			    is_sword(uwep) ? c_sword : c_weapon);
			return(0);
		}
		/* Sometimes we want to give the off_msg before removing and
		 * sometimes after; for instance, "you were wearing a moonstone
		 * ring (on right hand)" is desired but "you were wearing a
		 * square amulet (being worn)" is not because of the redundant
		 * "being worn".
		 */
		off_msg(otmp);
		Ring_off(otmp);
	} else if(otmp->oclass == AMULET_CLASS) {
		Amulet_off();
		off_msg(otmp);
	} else Blindf_off(otmp); /* does its own off_msg */
	return(1);
}

/* Check if something worn is cursed _and_ unremovable. */
int
cursed(otmp)
register struct obj *otmp;
{
	/* Curses, like chickens, come home to roost. */
	if((otmp == uwep) ? welded(otmp) : (int)otmp->cursed) {
/*JP		You("can't.  %s to be cursed.",
			(is_boots(otmp) || is_gloves(otmp) || otmp->quan > 1L)
			? "They seem" : "It seems");*/
		pline("無理だ．%sは呪われているようだ．",
			(is_boots(otmp) || is_gloves(otmp) || otmp->quan > 1L)
			? "それら" : "それ");
		otmp->bknown = TRUE;
		return(1);
	}
	return(0);
}

int
armoroff(otmp)
register struct obj *otmp;
{
	register int delay = -objects[otmp->otyp].oc_delay;

	if(cursed(otmp)) return(0);
	if(delay) {
		nomul(delay);
		if (is_helmet(otmp)) {
/*JP			nomovemsg = "You finish taking off your helmet.";*/
			nomovemsg = "あなたは兜を脱ぎおえた．";
			afternmv = Helmet_off;
		     }
		else if (is_gloves(otmp)) {
			nomovemsg = "You finish taking off your gloves.";
			nomovemsg = "あなたは小手を脱ぎおえた．";
			afternmv = Gloves_off;
		     }
		else if (is_boots(otmp)) {
			nomovemsg = "You finish taking off your boots.";
			nomovemsg = "あなたは靴を脱ぎおえた．";
			afternmv = Boots_off;
		     }
		else {
			nomovemsg = "You finish taking off your suit.";
			nomovemsg = "あなたは着ている物を脱ぎおえた．";
			afternmv = Armor_off;
		}
	} else {
		/* Be warned!  We want off_msg after removing the item to
		 * avoid "You were wearing ____ (being worn)."  However, an
		 * item which grants fire resistance might cause some trouble
		 * if removed in Hell and lifesaving puts it back on; in this
		 * case the message will be printed at the wrong time (after
		 * the messages saying you died and were lifesaved).  Luckily,
		 * no cloak, shield, or fast-removable armor grants fire
		 * resistance, so we can safely do the off_msg afterwards.
		 * Rings do grant fire resistance, but for rings we want the
		 * off_msg before removal anyway so there's no problem.  Take
		 * care in adding armors granting fire resistance; this code
		 * might need modification.
		 * 3.2 (actually 3.1 even): this comment is obsolete since
		 * fire resistance is not needed for Gehennom.
		 */
		if(is_cloak(otmp))
			(void) Cloak_off();
		else if(is_shield(otmp))
			(void) Shield_off();
		else setworn((struct obj *)0, otmp->owornmask & W_ARMOR);
		off_msg(otmp);
	}
	takeoff_mask = taking_off = 0L;
	return(1);
}

/*
** 物によって動詞が変化するので otmpを追加
*/
static void
already_wearing(cc, otmp)
const char *cc;
struct obj *otmp;
{
  const char *j;
  const char *m;
  m = jconj(jonmsg(otmp, &j), "ている");

/*JP	You("are already wearing %s%c", cc, (cc == c_that_) ? '!' : '.');*/
	You("もう%s%s%s%s", cc, j,  m, (cc == c_that_) ? "！" : "．");
}

/* the 'W' command */
int
dowear()
{
	register struct obj *otmp;
	register int delay;
	register int err = 0;
	long mask = 0;
	const char *which;
	const char *j;
	const char *m;

	/* cantweararm checks for suits of armor */
	/* verysmall or nohands checks for shields, gloves, etc... */
	if ((verysmall(uasmon) || nohands(uasmon))) {
/*JP		pline("Don't even bother.");*/
		pline("そんなつまらないことにこだわるな．");
		return(0);
	}

/*JP	otmp = getobj(clothes, "wear");*/
	otmp = getobj(clothes, "身につける");
	if(!otmp) return(0);

	which = is_cloak(otmp) ? c_cloak :
#ifdef TOURIST
		is_shirt(otmp) ? c_shirt :
#endif
		is_suit(otmp)  ? c_suit  : 0;
	if (which && cantweararm(uasmon) &&
		/* same exception for cloaks as used in m_dowear() */
		(which != c_cloak || uasmon->msize != MZ_SMALL)) {
/*JP	    pline_The("%s will not fit on your body.", which);*/
	    pline("%sはあなたの体に合わない．", which);
	    return 0;
	} else if (otmp->owornmask & W_ARMOR) {
/*JP	    already_wearing(c_that_);*/
	    already_wearing(c_that_, otmp);
	    return 0;
	}

	if(is_helmet(otmp)) {
		if(uarmh) {
/*JP			already_wearing(an(c_helmet));*/
			already_wearing(an(c_helmet), uarmh);
			err++;
		} else
			mask = W_ARMH;
	} else if(is_shield(otmp)){
		if(uarms) {
/*JP			already_wearing(an(c_shield));*/
			already_wearing(an(c_shield), uarms);
			err++;
		}
		if(uwep && bimanual(uwep)) {
/*JP		    You("cannot wear a shield while wielding a two-handed %s.",
			is_sword(uwep) ? c_sword :
				uwep->otyp == BATTLE_AXE ? c_axe : c_weapon);*/
		    m = jconj(jonmsg(otmp, &j), "ている");
		    You("両手持ちの%s%s%sので盾で身を守れない．",
			is_sword(uwep) ? c_sword :
				uwep->otyp == BATTLE_AXE ? c_axe : c_weapon,
				j, m);
			err++;
		}
		if(!err) mask = W_ARMS;
	} else if(is_boots(otmp)) {
		if (uarmf) {
/*JP			already_wearing(c_boots);*/
			already_wearing(c_boots, uarmf);
			err++;
		} else if (Upolyd && slithy(uasmon)) {
/*JP			You("have no feet...");	*//* not body_part(FOOT) */
			You("足がない．．．");	/* not body_part(FOOT) */
			err++;
		} else if (u.utrap && (u.utraptype == TT_BEARTRAP ||
				  u.utraptype == TT_INFLOOR)) {
			if (u.utraptype == TT_BEARTRAP)
/*JP			    Your("%s is trapped!", body_part(FOOT));*/
			    Your("%sは罠にかかっている！", body_part(FOOT));
			else
/*JP			    Your("%s are stuck in the %s!",*/
			    Your("%sは%sにはまっている！",
				 makeplural(body_part(FOOT)),
				 surface(u.ux, u.uy));
			err++;
		} else
			mask = W_ARMF;
	} else if(is_gloves(otmp)) {
		if(uarmg) {
/*JP			already_wearing(c_gloves);*/
			already_wearing(c_gloves, uarmg);
			err++;
		} else if (welded(uwep)) {
/*JP			You("cannot wear gloves over your %s.",*/
			You("%sの上から小手を装備できない．",
			      is_sword(uwep) ? c_sword : c_weapon);
			err++;
		} else
			mask = W_ARMG;
#ifdef TOURIST
	} else if(is_shirt(otmp)) {
		if (uarm || uarmc || uarmu) {
			if(uarmu)
/*JP			   already_wearing(an(c_shirt));*/
			   already_wearing(an(c_shirt), uarmu);
			else
/*JP			   You_cant("wear that over your %s.",*/
			   You("%sの上から着ることはできない．",
				 (uarm && !uarmc) ? c_armor : c_cloak);
			err++;
		} else
			mask = W_ARMU;
#endif
	} else if(is_cloak(otmp)) {
		if(uarmc) {
/*JP			already_wearing(an(c_cloak));*/
			already_wearing(an(c_cloak), uarmc);
			err++;
		} else
			mask = W_ARMC;
	} else {
		if(uarmc) {
/*JP			You("cannot wear armor over a cloak.");*/
			You("クロークの上から着ることはできない．");
			err++;
		} else if(uarm) {
/*JP			already_wearing("some armor");*/
			already_wearing("鎧", uarm);
			err++;
		}
		if(!err) mask = W_ARM;
	}
/* Unnecessary since now only weapons and special items like pick-axes get
 * welded to your hand, not armor
	if(welded(otmp)) {
		if(!err++)
			weldmsg(otmp);
	}
 */
	if(err) return(0);

	if (otmp->oartifact && !touch_artifact(otmp, &youmonst))
	    return 1;	/* costs a turn even though it didn't get worn */

	if (otmp->otyp == HELM_OF_OPPOSITE_ALIGNMENT &&
			qstart_level.dnum == u.uz.dnum) {	/* in quest */
/*JP		You("narrowly avoid losing all chance at your goal.");*/
		You("目的達成のための可能性を失うことをなんとか回避した．");
		u.ublessed = 0; /* lose your god's protection */
		makeknown(otmp->otyp);
		flags.botl = 1;
		return 1;
	}

	otmp->known = TRUE;
	if(otmp == uwep)
		setuwep((struct obj *)0);
	setworn(otmp, mask);
	delay = -objects[otmp->otyp].oc_delay;
	if(delay){
		nomul(delay);
		if(is_boots(otmp)) afternmv = Boots_on;
		if(is_helmet(otmp)) afternmv = Helmet_on;
		if(is_gloves(otmp)) afternmv = Gloves_on;
		if(otmp == uarm) afternmv = Armor_on;
/*JP		nomovemsg = "You finish your dressing maneuver.";*/
		nomovemsg = "あなたは装備し終った．";
	} else {
		if(is_cloak(otmp)) (void) Cloak_on();
/*		if(is_shield(otmp)) (void) Shield_on(); */
#ifdef FIGHTER
		if(otmp == uarm) (void) Armor_on();
#endif
		on_msg(otmp);
	}
	takeoff_mask = taking_off = 0L;
	return(1);
}

int
doputon()
{
	register struct obj *otmp;
	long mask = 0L;

	if(uleft && uright && uamul && ublindf) {
/*JP    Your("%s%s are full, and you're already wearing an amulet and a blindfold.",
			humanoid(uasmon) ? "ring-" : "",
			makeplural(body_part(FINGER)));*/
	  Your("%s%sはふさがってるし，すでに魔除けと目隠しも身につけている．",
	       humanoid(uasmon) ? "薬" : "",
	       makeplural(body_part(FINGER)));
	  return(0);
	}
/*JP	otmp = getobj(accessories, "wear");*/
	otmp = getobj(accessories, "身につける");
	if(!otmp) return(0);
	if(otmp->owornmask & (W_RING | W_AMUL | W_TOOL)) {
		already_wearing(c_that_, otmp);
		return(0);
	}
	if(welded(otmp)) {
		weldmsg(otmp);
		return(0);
	}
	if(otmp == uwep)
		setuwep((struct obj *)0);
	if(otmp->oclass == RING_CLASS) {
		if(nolimbs(uasmon)) {
/*JP			You("cannot make the ring stick to your body.");*/
			You("指輪をはめれない体だ．");
			return(0);
		}
		if(uleft && uright){
/*JP			pline("There are no more %s%s to fill.",
				humanoid(uasmon) ? "ring-" : "",
				makeplural(body_part(FINGER)));*/
			pline("はめることのできる%s%sがない．",
				humanoid(uasmon) ? "薬" : "",
				makeplural(body_part(FINGER)));
			return(0);
		}
		if(uleft) mask = RIGHT_RING;
		else if(uright) mask = LEFT_RING;
		else do {
			char qbuf[QBUFSZ];
			char answer;

/*JP			Sprintf(qbuf, "What %s%s, Right or Left?",
				humanoid(uasmon) ? "ring-" : "",
				body_part(FINGER));*/
			Sprintf(qbuf, "どちらの%s%s，右(r)それとも左(l)？",
				humanoid(uasmon) ? "薬" : "",
				body_part(FINGER));
			if(!(answer = yn_function(qbuf, "rl", '\0')))
				return(0);
			switch(answer){
			case 'l':
			case 'L':
				mask = LEFT_RING;
				break;
			case 'r':
			case 'R':
				mask = RIGHT_RING;
				break;
			}
		} while(!mask);
		if (uarmg && uarmg->cursed) {
			uarmg->bknown = TRUE;
/*JP		    You("cannot remove your gloves to put on the ring.");*/
		    You("指輪をはめようとしたが小手が脱げない．");
			return(0);
		}
		if (welded(uwep) && bimanual(uwep)) {
			/* welded will set bknown */
/*JP	    You("cannot free your weapon hands to put on the ring.");*/
	    You("指輪をはめようとしたが利腕の自由がきかない．");
			return(0);
		}
		if (welded(uwep) && mask==RIGHT_RING) {
			/* welded will set bknown */
/*JP	    You("cannot free your weapon hand to put on the ring.");*/
	    You("指輪をはめようとしたが利腕の自由がきかない．");
			return(0);
		}
		setworn(otmp, mask);
		Ring_on(otmp);
	} else if (otmp->oclass == AMULET_CLASS) {
		if(uamul) {
/*JP			already_wearing("an amulet");*/
			already_wearing("魔除け", uamul);
			return(0);
		}
		setworn(otmp, W_AMUL);
		if (otmp->otyp == AMULET_OF_CHANGE) {
			Amulet_on();
			/* Don't do a prinv() since the amulet is now gone */
			return(1);
		}
		Amulet_on();
	} else {	/* it's a blindfold */
		if (ublindf) {
			if (ublindf->otyp == TOWEL)
/*JP				Your("%s is already covered by a towel.",
					body_part(FACE));*/
				You("既にタオルを身につけている．");
			else
/*JP				already_wearing("a blindfold");*/
				already_wearing("目隠", ublindf);
			return(0);
		}
		if (otmp->otyp != BLINDFOLD && otmp->otyp != TOWEL) {
/*JP			You_cant("wear that!");*/
			You("それを身につけることはできない！");
			return(0);
		}
		Blindf_on(otmp);
		return(1);
	}
	prinv((char *)0, otmp, 0L);
	return(1);
}

#endif /* OVLB */

#ifdef OVL0

void
find_ac()
{
	register int uac = 10;
	if (u.mtimedone) uac = mons[u.umonnum].ac;
	if(uarm) uac -= ARM_BONUS(uarm);
	if(uarmc) uac -= ARM_BONUS(uarmc);
	if(uarmh) uac -= ARM_BONUS(uarmh);
	if(uarmf) uac -= ARM_BONUS(uarmf);
	if(uarms) uac -= ARM_BONUS(uarms);
	if(uarmg) uac -= ARM_BONUS(uarmg);
#ifdef TOURIST
	if(uarmu) uac -= ARM_BONUS(uarmu);
#endif
	if(uleft && uleft->otyp == RIN_PROTECTION) uac -= uleft->spe;
	if(uright && uright->otyp == RIN_PROTECTION) uac -= uright->spe;
	if (Protection & INTRINSIC) uac -= u.ublessed;
	if(uac != u.uac){
		u.uac = uac;
		flags.botl = 1;
	}
}

#endif /* OVL0 */
#ifdef OVLB

void
glibr()
{
	register struct obj *otmp;
	int xfl = 0;
	boolean leftfall, rightfall;

	leftfall = (uleft && !uleft->cursed &&
		    (!uwep || !welded(uwep) || !bimanual(uwep)));
	rightfall = (uright && !uright->cursed && (!welded(uwep)));
	if (!uarmg && (leftfall || rightfall) && !nolimbs(uasmon)) {
		/* changed so cursed rings don't fall off, GAN 10/30/86 */
/*JP		Your("%s off your %s.",
			(leftfall && rightfall) ? "rings slip" : "ring slips",*/
		Your("指輪は%sから滑り落ちた．",
			makeplural(body_part(FINGER)));
		xfl++;
		if (leftfall) {
			otmp = uleft;
			Ring_off(uleft);
			dropx(otmp);
		}
		if (rightfall) {
			otmp = uright;
			Ring_off(uright);
			dropx(otmp);
		}
	}
	otmp = uwep;
	if (otmp && !welded(otmp)) {
		/* changed so cursed weapons don't fall, GAN 10/30/86 */
/*JP		Your("%s %sslips from your %s.",
			is_sword(otmp) ? c_sword :
				makesingular(oclass_names[(int)otmp->oclass]),
			xfl ? "also " : "",
			makeplural(body_part(HAND)));*/
		You("%s%s%sから滑り落とした．",
			is_sword(otmp) ? c_sword :
				jtrns_obj('S',oclass_names[(int)otmp->oclass]),
			xfl ? "もまた" : "を",
			makeplural(body_part(HAND)));
		setuwep((struct obj *)0);
		if (otmp->otyp != LOADSTONE || !otmp->cursed)
			dropx(otmp);
	}
}

struct obj *
some_armor()
{
	register struct obj *otmph = (uarmc ? uarmc : uarm);
	if(uarmh && (!otmph || !rn2(4))) otmph = uarmh;
	if(uarmg && (!otmph || !rn2(4))) otmph = uarmg;
	if(uarmf && (!otmph || !rn2(4))) otmph = uarmf;
	if(uarms && (!otmph || !rn2(4))) otmph = uarms;
#ifdef TOURIST
	if(!uarm && !uarmc && uarmu && (!otmph || !rn2(4))) otmph = uarmu;
#endif
	return(otmph);
}

void
erode_armor(acid_dmg)
boolean acid_dmg;
{
	register struct obj *otmph = some_armor();

	if (otmph && otmph != uarmf) {
	    if (otmph->greased) {
		grease_protect(otmph,(char *)0,FALSE);
		return;
	    }
	    if (otmph->oerodeproof ||
		(acid_dmg ? !is_corrodeable(otmph) : !is_rustprone(otmph))) {
		if (flags.verbose || !(otmph->oerodeproof && otmph->rknown))
/*JP			Your("%s not affected.", aobjnam(otmph, "are"));*/
			Your("%sは影響を受けなかった．", xname(otmph));
		if (otmph->oerodeproof) otmph->rknown = TRUE;
		return;
	    }
	    if (otmph->oeroded < MAX_ERODE) {
/*JP		Your("%s%s!", aobjnam(otmph, acid_dmg ? "corrode" : "rust"),
			otmph->oeroded+1 == MAX_ERODE ? " completely" :
			otmph->oeroded ? " further" : "");*/
		Your("%sは%s%s！", xname(otmph), 
			otmph->oeroded+1 == MAX_ERODE ? "完全に" :
			otmph->oeroded ? "さらに" : "",
		        acid_dmg ? "腐食した" : "錆びた");
		otmph->oeroded++;
		return;
	    }
	    if (flags.verbose)
/*JP		Your("%s completely %s.",
		     aobjnam(otmph, Blind ? "feel" : "look"),
		     acid_dmg ? "corroded" : "rusty");*/
		Your("%sは完全に%s%s．",
		     xname(otmph),
		     acid_dmg ? "腐食した" : "錆びた",
	             Blind ? "ようだ" : "ように見える");
	}
}

STATIC_PTR
int
select_off(otmp)
register struct obj *otmp;
{
	if(!otmp) return(0);
	if(cursed(otmp)) return(0);
	if(otmp->oclass==RING_CLASS && nolimbs(uasmon)) return(0);
	if(welded(uwep) && (otmp==uarmg || otmp==uright || (otmp==uleft
			&& bimanual(uwep))))
		return(0);
	if(uarmg && uarmg->cursed && (otmp==uright || otmp==uleft)) {
		uarmg->bknown = TRUE;
		return(0);
	}
	if(otmp == uarmf && u.utrap && (u.utraptype == TT_BEARTRAP ||
					u.utraptype == TT_INFLOOR)) {
		return (0);
	}
	if((otmp==uarm
#ifdef TOURIST
			|| otmp==uarmu
#endif
					) && uarmc && uarmc->cursed) {
		uarmc->bknown = TRUE;
		return(0);
	}
#ifdef TOURIST
	if(otmp==uarmu && uarm && uarm->cursed) {
		uarm->bknown = TRUE;
		return(0);
	}
#endif

	if(otmp == uarm) takeoff_mask |= WORN_ARMOR;
	else if(otmp == uarmc) takeoff_mask |= WORN_CLOAK;
	else if(otmp == uarmf) takeoff_mask |= WORN_BOOTS;
	else if(otmp == uarmg) takeoff_mask |= WORN_GLOVES;
	else if(otmp == uarmh) takeoff_mask |= WORN_HELMET;
	else if(otmp == uarms) takeoff_mask |= WORN_SHIELD;
#ifdef TOURIST
	else if(otmp == uarmu) takeoff_mask |= WORN_SHIRT;
#endif
	else if(otmp == uleft) takeoff_mask |= LEFT_RING;
	else if(otmp == uright) takeoff_mask |= RIGHT_RING;
	else if(otmp == uamul) takeoff_mask |= WORN_AMUL;
	else if(otmp == ublindf) takeoff_mask |= WORN_BLINDF;
	else if(otmp == uwep) takeoff_mask |= 1L;	/* WIELDED_WEAPON */

	else impossible("select_off: %s???", doname(otmp));

	return(0);
}

static struct obj *
do_takeoff()
{
	register struct obj *otmp = (struct obj *)0;

	if (taking_off == 1L) { /* weapon */
	  if(!cursed(uwep)) {
	    setuwep((struct obj *) 0);
/*JP	    You("are empty %s.", body_part(HANDED));*/
	    You("何も%sにしていない．", body_part(HAND));
	  }
	} else if (taking_off ==  WORN_ARMOR) {
	  otmp = uarm;
	  if(!cursed(otmp)) (void) Armor_off();
	} else if (taking_off == WORN_CLOAK) {
	  otmp = uarmc;
	  if(!cursed(otmp)) (void) Cloak_off();
	} else if (taking_off == WORN_BOOTS) {
	  otmp = uarmf;
	  if(!cursed(otmp)) (void) Boots_off();
	} else if (taking_off == WORN_GLOVES) {
	  otmp = uarmg;
	  if(!cursed(otmp)) (void) Gloves_off();
	} else if (taking_off == WORN_HELMET) {
	  otmp = uarmh;
	  if(!cursed(otmp)) (void) Helmet_off();
	} else if (taking_off == WORN_SHIELD) {
	  otmp = uarms;
	  if(!cursed(otmp)) (void) Shield_off();
#ifdef TOURIST
	} else if (taking_off == WORN_SHIRT) {
	  otmp = uarmu;
	  if(!cursed(otmp))
	    setworn((struct obj *)0, uarmu->owornmask & W_ARMOR);
#endif
	} else if (taking_off == WORN_AMUL) {
	  otmp = uamul;
	  if(!cursed(otmp)) Amulet_off();
	} else if (taking_off == LEFT_RING) {
	  otmp = uleft;
	  if(!cursed(otmp)) Ring_off(uleft);
	} else if (taking_off == RIGHT_RING) {
	  otmp = uright;
	  if(!cursed(otmp)) Ring_off(uright);
	} else if (taking_off == WORN_BLINDF) {
	  if(!cursed(ublindf)) {
	    setworn((struct obj *)0, ublindf->owornmask);
	    if(!Blinded) make_blinded(1L,FALSE); /* See on next move */
/*JP	    else	 You("still cannot see.");*/
	    else	 You("まだ見えない．");
	  }
	} else impossible("do_takeoff: taking off %lx", taking_off);

	return(otmp);
}

STATIC_PTR
int
take_off()
{
	register int i;
	register struct obj *otmp;

	if(taking_off) {
	    if(todelay > 0) {

		todelay--;
		return(1);	/* still busy */
	    } else if((otmp = do_takeoff())) off_msg(otmp);

	    takeoff_mask &= ~taking_off;
	    taking_off = 0L;
	}

	for(i = 0; takeoff_order[i]; i++)
	    if(takeoff_mask & takeoff_order[i]) {
		taking_off = takeoff_order[i];
		break;
	    }

	otmp = (struct obj *) 0;
	todelay = 0;

	if (taking_off == 0L) {
/*JP	  You("finish disrobing.");*/
	  You("装備を解きおえた．");
	  return 0;
	} else if (taking_off == 1L) {
	  todelay = 1;
	} else if (taking_off == WORN_ARMOR) {
	  otmp = uarm;
	  /* If a cloak is being worn, add the time to take it off and put
	   * it back on again.  Kludge alert! since that time is 0 for all
	   * known cloaks, add 1 so that it actually matters...
	   */
	  if (uarmc) todelay += 2 * objects[uarmc->otyp].oc_delay + 1;
	} else if (taking_off == WORN_CLOAK) {
	  otmp = uarmc;
	} else if (taking_off == WORN_BOOTS) {
	  otmp = uarmf;
	} else if (taking_off == WORN_GLOVES) {
	  otmp = uarmg;
	} else if (taking_off == WORN_HELMET) {
	  otmp = uarmh;
	} else if (taking_off == WORN_SHIELD) {
	  otmp = uarms;
#ifdef TOURIST
	} else if (taking_off == WORN_SHIRT) {
	  otmp = uarmu;
	  /* add the time to take off and put back on armor and/or cloak */
	  if (uarm)  todelay += 2 * objects[uarm->otyp].oc_delay;
	  if (uarmc) todelay += 2 * objects[uarmc->otyp].oc_delay + 1;
#endif
	} else if (taking_off == WORN_AMUL) {
	  todelay = 1;
	} else if (taking_off == LEFT_RING) {
	  todelay = 1;
	} else if (taking_off == RIGHT_RING) {
	  todelay = 1;
	} else if (taking_off == WORN_BLINDF) {
	  todelay = 2;
	} else {
	  impossible("take_off: taking off %lx", taking_off);
	  return 0;	/* force done */
	}

	if (otmp) todelay += objects[otmp->otyp].oc_delay;
/*JP	set_occupation(take_off, "disrobing", 0);*/
	set_occupation(take_off, "装備を解いている", 0);
	return(1);		/* get busy */
}

#endif /* OVLB */
#ifdef OVL1

void
reset_remarm()
{
	taking_off = takeoff_mask =0L;
}

#endif /* OVL1 */
#ifdef OVLB

int
doddoremarm()
{
    int result = 0;

    if (taking_off || takeoff_mask) {
/*JP	You("continue disrobing.");*/
	You("装備を解くのを再開した．");
	set_occupation(take_off, "装備を解いている", 0);
	return(take_off());
    } else if (!uwep && !uamul && !ublindf &&
		!uleft && !uright && !wearing_armor()) {
/*JP	You("are not wearing anything.");*/
	You("何も装備していない．");
	return 0;
    }

    add_valid_menu_class(0); /* reset */
    if (flags.menu_style != MENU_TRADITIONAL ||
/*JP	    (result = ggetobj("take off", select_off, 0, FALSE)) < -1)*/
	    (result = ggetobj("の装備を解く", select_off, 0, FALSE)) < -1)
	result = menu_remarm(result);

    return takeoff_mask ? take_off() : 0;
}

static int
menu_remarm(retry)
int retry;
{
    int n, i = 0;
    menu_item *pick_list;
    boolean all_worn_categories = TRUE;

    if (retry) {
	all_worn_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
	all_worn_categories = FALSE;
/*JP	n = query_category("What type of things do you want to take off?",*/
	n = query_category("どのタイプの物の装備を解きますか？",
			   invent, WORN_TYPES|ALL_TYPES, &pick_list, PICK_ANY);
	if (!n) return 0;
	for (i = 0; i < n; i++) {
	    if (pick_list[i].item.a_int == ALL_TYPES_SELECTED)
		all_worn_categories = TRUE;
	    else
		add_valid_menu_class(pick_list[i].item.a_int);
	}
	free((genericptr_t) pick_list);
    } else if (flags.menu_style == MENU_COMBINATION) {
	all_worn_categories = FALSE;
/*JP	if (ggetobj("take off", select_off, 0, TRUE) == -2)*/
	if (ggetobj("の装備を解く", select_off, 0, TRUE) == -2)
	    all_worn_categories = TRUE;
    }

/*JP    n = query_objlist("What do you want to take off?", invent,*/
    	n = query_objlist("どの装備を解きますか？", invent,
			SIGNAL_NOMENU|USE_INVLET|INVORDER_SORT,
			&pick_list, PICK_ANY,
			all_worn_categories ? is_worn : is_worn_by_type);
    if (n > 0) {
	for (i = 0; i < n; i++)
	    (void) select_off(pick_list[i].item.a_obj);
	free((genericptr_t) pick_list);
    } else if (n < 0) {
/*JP	pline("There is nothing else you can remove or unwield.");*/
	pline("装備を解けるものは何もない．");
    }
    return 0;
}

int
destroy_arm(atmp)
register struct obj *atmp;
{
	register struct obj *otmp;
#define DESTROY_ARM(o) ((otmp = (o)) != 0 && \
			(!atmp || atmp == otmp) && \
			(!obj_resists(otmp, 0, 90)))

	if (DESTROY_ARM(uarmc)) {
/*JP		Your("cloak crumbles and turns to dust!");*/
		Your("クロークは砕けて塵となった！");
		(void) Cloak_off();
		useup(otmp);
	} else if (DESTROY_ARM(uarm)) {
		/* may be disintegrated by spell or dragon breath... */
		if (donning(otmp)) cancel_don();
/*JP		Your("armor turns to dust and falls to the %s!",
			surface(u.ux,u.uy));*/
		Your("鎧は塵となり%sに落ちた！",
			surface(u.ux,u.uy));
		(void) Armor_gone();
		useup(otmp);
#ifdef TOURIST
	} else if (DESTROY_ARM(uarmu)) {
/*JP		Your("shirt crumbles into tiny threads and falls apart!");*/
		Your("シャツはズタズタに裂け，小さな糸屑となり落ちた！");
		useup(otmp);
#endif
	} else if (DESTROY_ARM(uarmh)) {
		if (donning(otmp)) cancel_don();
/*JP		Your("helmet turns to dust and is blown away!");*/
		Your("兜は塵となり吹きとんだ！");
		(void) Helmet_off();
		useup(otmp);
	} else if (DESTROY_ARM(uarmg)) {
		if (donning(otmp)) cancel_don();
/*JP		Your("gloves vanish!");*/
		Your("小手は消えた！");
		(void) Gloves_off();
		useup(otmp);
/*JP		selftouch("You");*/
		selftouch("そのときあなたは");
	} else if (DESTROY_ARM(uarmf)) {
		if (donning(otmp)) cancel_don();
/*JP		Your("boots disintegrate!");*/
		Your("靴は粉々に砕けた！");
		(void) Boots_off();
		useup(otmp);
	} else if (DESTROY_ARM(uarms)) {
/*JP		Your("shield crumbles away!");*/
		Your("盾は砕け散った！");
		(void) Shield_off();
		useup(otmp);
	} else	return(0);		/* could not destroy anything */

#undef DESTROY_ARM
	return(1);
}

void
adj_abon(otmp, delta)
register struct obj *otmp;
register schar delta;
{
	if (uarmg && uarmg == otmp && otmp->otyp == GAUNTLETS_OF_DEXTERITY) {
		if (delta) {
			makeknown(uarmg->otyp);
			ABON(A_DEX) += (delta);
		}
		flags.botl = 1;
	}
	if (uarmh && uarmh == otmp && otmp->otyp == HELM_OF_BRILLIANCE) {
		if (delta) {
			makeknown(uarmh->otyp);
			ABON(A_INT) += (delta);
			ABON(A_WIS) += (delta);
		}
		flags.botl = 1;
	}
}

#endif /* OVLB */

/*do_wear.c*/
