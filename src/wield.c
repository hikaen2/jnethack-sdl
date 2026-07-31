/*	SCCS Id: @(#)wield.c	3.2	96/07/04	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"

/* elven weapons vibrate warningly when enchanted beyond a limit */
#define is_elven_weapon(optr)	((optr)->otyp == ELVEN_ARROW\
				|| (optr)->otyp == ELVEN_SPEAR\
				|| (optr)->otyp == ELVEN_DAGGER\
				|| (optr)->otyp == ELVEN_SHORT_SWORD\
				|| (optr)->otyp == ELVEN_BROADSWORD\
				|| (optr)->otyp == ELVEN_BOW)

/* used by erode_weapon() and will_weld() */
#define erodeable_wep(optr)	((optr)->oclass == WEAPON_CLASS \
				|| is_weptool(optr) \
				|| (optr)->otyp == HEAVY_IRON_BALL \
				|| (optr)->otyp == IRON_CHAIN)

/* used by welded(), and also while wielding */
#define will_weld(optr)		((optr)->cursed \
				&& (erodeable_wep(optr) \
				   || (optr)->otyp == TIN_OPENER))

/* Note: setuwep() with a null obj, and uwepgone(), are NOT the same!
 * Sometimes unwielding a weapon can kill you, and lifesaving will then
 * put it back into your hand.  If lifesaving is permitted to do this,
 * use setwuep((struct obj *)0); otherwise use uwepgone().
 */
void
setuwep(obj)
register struct obj *obj;
{
	if (obj == uwep) return; /* necessary to not set unweapon */
	setworn(obj, W_WEP);
	/* Note: Explicitly wielding a pick-axe will not give a "bashing"
	 * message.  Wielding one via 'a'pplying it will.
	 * 3.2.2:  Wielding arbitrary objects will give bashing message too.
	 */
	if (obj) {
		int wepcat = objects[obj->otyp].oc_wepcat;
		unweapon = (obj->oclass == WEAPON_CLASS) ?
				(wepcat == WEP_BOW ||
				 wepcat == WEP_AMMO ||
				 wepcat == WEP_MISSILE) :
			   !is_weptool(obj);
	} else
		unweapon = TRUE;	/* for "bare hands" message */
	update_inventory();
}

void
uwepgone()
{
	if (uwep) {
		setworn((struct obj *)0, W_WEP);
		unweapon = TRUE;
		update_inventory();
	}
}

static NEARDATA const char wield_objs[] =
	{ ALL_CLASSES, ALLOW_NONE, WEAPON_CLASS, TOOL_CLASS, 0 };

int
dowield()
{
	register struct obj *wep;
	register int res = 0;

	multi = 0;
	if (cantwield(uasmon)) {
/*JP
	    pline("Don't be ridiculous!");
*/
	    pline("ばかばかしい！");
	    return(0);
	}
/*JP
	if (!(wep = getobj(wield_objs, "wield"))) {
*/
	if (!(wep = getobj(wield_objs, body_part(HANDED)))) {
	    ;	/* nothing */
	} else if (uwep == wep) {
/*JP
	    You("are already wielding that!");
*/
	    You("もうそれを%sにしている！", body_part(HAND));
	    if (is_weptool(wep)) unweapon = FALSE;	/* [see setuwep()] */
	} else if (welded(uwep)) {
	    weldmsg(uwep);
	} else if (wep == &zeroobj) {
	    if (uwep == 0)
/*JP
		You("are already empty %s.", body_part(HANDED));
*/
		You("何も%sにしていない！", body_part(HAND));
	    else  {
/*JP
		You("are empty %s.", body_part(HANDED));
*/
	        You("%sを空けた．", body_part(HAND));
		setuwep((struct obj *) 0);
		res++;
	    }
	} else if (!uarmg && !resists_ston(&youmonst) &&
		   (wep->otyp == CORPSE && wep->corpsenm == PM_COCKATRICE)) {
	    /* Prevent wielding cockatrice when not wearing gloves --KAA */
/*JP
	    You("wield the cockatrice corpse in your bare %s.",
			makeplural(body_part(HAND)));
	    instapetrify("cockatrice corpse");
*/
	    You("コカトリスの死体を%sにした．",
			makeplural(body_part(HAND)));
	    instapetrify("コカトリスの死体に触れて");
	} else if (uarms && bimanual(wep))
/*JP
	    You("cannot wield a two-handed %s while wearing a shield.",
		is_sword(wep) ? "sword" :
		    wep->otyp == BATTLE_AXE ? "axe" : "weapon");
*/
	    pline("盾を装備しているときに両手持ちの%sを装備できない．",
		is_sword(wep) ? "剣" :
		    wep->otyp == BATTLE_AXE ? "斧" : "武器");
	else if (wep->owornmask & (W_ARMOR | W_RING | W_AMUL | W_TOOL))
/*JP
	    You("cannot wield that!");
*/
	    You("それを装備できない！");
	else if (wep->oartifact && !touch_artifact(wep, &youmonst))
	    res++;	/* takes a turn even though it doesn't get wielded */
	else {
	    res++;
	    if (will_weld(wep)) {
		const char *tmp = xname(wep), *thestr = "The ";
		if (strncmp(tmp, thestr, 4) && !strncmp(The(tmp),thestr,4))
		    tmp = thestr;
		else tmp = "";
/*JP		pline("%s%s %s to your %s!",
		      tmp, aobjnam(wep, "weld"),
		      (wep->quan == 1L) ? "itself" : "themselves", / * a3 * /
		      body_part(HAND));*/
	        pline("%sは勝手にあなたの%sに装備された．",
		      xname(wep), 
		      body_part(HAND));
		wep->bknown = TRUE;
	    } else {
		/* The message must be printed before setuwep (since
		 * you might die and be revived from changing weapons),
		 * and the message must be before the death message and
		 * Lifesaved rewielding.  Yet we want the message to
		 * say "weapon in hand", thus this kludge.
		 */
		long dummy = wep->owornmask;
		wep->owornmask |= W_WEP;
		prinv((char *)0, wep, 0L);
		wep->owornmask = dummy;
	    }
	    setuwep(wep);
	    if (wep->unpaid) {
		struct monst *this_shkp;

		if ((this_shkp = shop_keeper(inside_shop(u.ux, u.uy))) !=
		    (struct monst *)0) {
/*JP		    pline("%s says \"You be careful with my %s!\"",*/
		    pline("%sは述べた「%sの扱いは気をつけてくれよ！」",
			  shkname(this_shkp),
			  xname(wep));
		}
	    }
	}
	return(res);
}

/* maybe rust weapon, or corrode it if acid damage is called for */
void
erode_weapon(acid_dmg)
boolean acid_dmg;
{
	if (!uwep || !erodeable_wep(uwep))
		return;

	if (uwep->greased) {
		grease_protect(uwep,(char *)0,FALSE);
	} else if (uwep->oerodeproof ||
		    (acid_dmg ? !is_corrodeable(uwep) : !is_rustprone(uwep))) {
		if (flags.verbose || !(uwep->oerodeproof && uwep->rknown))
/*JP
		    Your("%s not affected.", aobjnam(uwep, "are"));
*/
		    Your("%sは影響を受けない．", xname(uwep));
		if (uwep->oerodeproof) uwep->rknown = TRUE;
	} else if (uwep->oeroded < MAX_ERODE) {
/*JP
		Your("%s%s!", aobjnam(uwep, acid_dmg ? "corrode" : "rust"),
		     uwep->oeroded+1 == MAX_ERODE ? " completely" :
		     uwep->oeroded ? " further" : "");
*/
		Your("%sは%s%s！", xname(uwep),
		     uwep->oeroded+1 == MAX_ERODE ? "完全に" :
		     uwep->oeroded ? "さらに" : "",
		     acid_dmg ? "腐食した" : "錆びた");
		uwep->oeroded++;
	} else {
		if (flags.verbose)
/*JP
		    Your("%s completely %s.",
			 aobjnam(uwep, Blind ? "feel" : "look"),
			 acid_dmg ? "corroded" : "rusty");
*/
		    Your("%sは完全に%s%s．",
			 xname(uwep),
			 acid_dmg ? "腐食した" : "錆びた",
			 Blind ? "ようだ" : "ように見える");
	}
}

int
chwepon(otmp, amount)
register struct obj *otmp;
register int amount;
{
	register const char *color = hcolor((amount < 0) ? Black : blue);
	register const char *xtime;

	if(!uwep || (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep))) {
		char buf[BUFSZ];

/*JP		Sprintf(buf, "Your %s %s.", makeplural(body_part(HAND)),
			(amount >= 0) ? "twitch" : "itch");*/
		Sprintf(buf, "あなたの%sは%s．", makeplural(body_part(HAND)),
			(amount >= 0) ? "ひきつった" : "ムズムズした");
		strange_feeling(otmp, buf);
		exercise(A_DEX, (boolean) (amount >= 0));
		return(0);
	}

	if(uwep->otyp == WORM_TOOTH && amount >= 0) {
		uwep->otyp = CRYSKNIFE;
/*JP		Your("weapon seems sharper now.");*/
		Your("武器はより鋭さを増したようだ．");
		uwep->cursed = 0;
		return(1);
	}

	if(uwep->otyp == CRYSKNIFE && amount < 0) {
		uwep->otyp = WORM_TOOTH;
/*JP		Your("weapon seems duller now.");*/
		Your("武器はより鈍くなったようだ．");
		return(1);
	}

	if (amount < 0 && uwep->oartifact && restrict_name(uwep, ONAME(uwep))) {
	    if (!Blind)
/*JP		Your("%s %s.", aobjnam(uwep, "faintly glow"), color);*/
		Your("%sはわずかに%s輝いた．", xname(uwep),jconj_adj(color));
	    return(1);
	}
	/* there is a (soft) upper and lower limit to uwep->spe */
	if(((uwep->spe > 5 && amount >= 0) || (uwep->spe < -5 && amount < 0))
								&& rn2(3)) {
	    if (!Blind)
/*JP	    Your("%s %s for a while and then evaporate%s.",
		 aobjnam(uwep, "violently glow"), color,
		 uwep->quan == 1L ? "s" : "");*/
	    Your("%sはしばらく激しく%s輝き，蒸発した．",
		 xname(uwep), jconj_adj(color));
	    else
/*JP		Your("%s.", aobjnam(uwep, "evaporate"));*/
		Your("%sは蒸発した", xname(uwep));

	    while(uwep)		/* let all of them disappear */
				/* note: uwep->quan = 1 is nogood if unpaid */
		useup(uwep);
	    return(1);
	}
	if (!Blind) {
/*JP	    xtime = (amount*amount == 1) ? "moment" : "while";*/
	    xtime = (amount*amount == 1) ? "一瞬" : "しばらくの間";
/*JP	    Your("%s %s for a %s.",
		 aobjnam(uwep, amount == 0 ? "violently glow" : "glow"),
		 color, xtime);*/
	    Your("%sは%s%s%s輝いた．",
		 xname(uwep), xtime, jconj_adj(color), 
		 amount == 0 ? "激しく" : "");
	}
	uwep->spe += amount;
	if(amount > 0) uwep->cursed = 0;

	/*
	 * Enchantment, which normally improves a weapon, has an
	 * addition adverse reaction on Magicbane whose effects are
	 * spe dependent.  Give an obscure clue here.
	 */
	if (uwep->oartifact == ART_MAGICBANE && uwep->spe >= 0) {
/*JP		Your("right %s %sches!",
			body_part(HAND),
			(((amount > 1) && (uwep->spe > 1)) ? "flin" : "it"));*/
		Your("右%sは%s！",
			body_part(HAND),
			(((amount > 1) && (uwep->spe > 1)) ? "ひりひりした" : "ムズムズした"));
	}

	/* an elven magic clue, cookie@keebler */
	if ((uwep->spe > 5)
		&& (is_elven_weapon(uwep) || uwep->oartifact || !rn2(7)))
/*JP	    Your("%s unexpectedly.",
		aobjnam(uwep, "suddenly vibrate"));*/
	    Your("%sは突然震えだした．",
		xname(uwep));

	return(1);
}

int
welded(obj)
register struct obj *obj;
{
	if (obj && obj == uwep && will_weld(obj)) {
		obj->bknown = TRUE;
		return 1;
	}
	return 0;
}

void
weldmsg(obj)
register struct obj *obj;
{
	long savewornmask;

	savewornmask = obj->owornmask;
/*JP
	Your("%s %s welded to your %s!",
		xname(obj), (obj->quan == 1L) ? "is" : "are",
		bimanual(obj) ? (const char *)makeplural(body_part(HAND))
				: body_part(HAND));
*/
	You("%sを%sに構えた！", 
		xname(obj), body_part(HAND));
	obj->owornmask = savewornmask;
}

/*wield.c*/
