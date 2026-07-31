/*	SCCS Id: @(#)artifact.c	3.2	96/07/08	*/
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
#ifdef OVLB
#include "artilist.h"
#else
STATIC_DCL struct artifact artilist[];
#endif
/*
 * Note:  both artilist[] and artiexist[] have a dummy element #0,
 *	  so loops over them should normally start at #1.  The primary
 *	  exception is the save & restore code, which doesn't care about
 *	  the contents, just the total size.
 */

extern boolean notonhead;	/* for long worms */

#define get_artifact(o) \
		(((o)&&(o)->oartifact) ? &artilist[(int) (o)->oartifact] : 0)
STATIC_DCL int FDECL(spec_applies, (const struct artifact *,struct monst *));
STATIC_DCL int FDECL(arti_invoke, (struct obj*));

/* The amount added to normal damage which should guarantee that the
   victim will be killed even after damage bonus/penalty adjustments.
   It needs to be big enough so that halving will still kill, but not
   so big that doubling ends up overflowing 15 bits.  This value used
   to be 1234, but it was possible for players to accumulate enough
   hit points so that taking (uhp + 1234)/2 damage was survivable. */
#define FATAL_DAMAGE 9999

#ifndef OVLB
STATIC_DCL int spec_dbon_applies;

#else	/* OVLB */
/* coordinate effects from spec_dbon() with messages in artifact_hit() */
STATIC_OVL int spec_dbon_applies = 0;

/* flags including which artifacts have already been created */
static boolean artiexist[1+NROFARTIFACTS+1];

static void NDECL(hack_artifacts);
static boolean FDECL(attacks, (int,struct obj *));

/* handle some special cases; must be called after u_init() */
static void
hack_artifacts()
{
	/* Excalibur can be used by any lawful character, not just knights */
	if (!Role_is('K'))
	    artilist[ART_EXCALIBUR].class = '\0';
	/* Mitre of Holiness has same alignment as priest starts out with */
	if (Role_is('P'))
	    artilist[ART_MITRE_OF_HOLINESS].alignment = u.ualignbase[1];
}

/* zero out the artifact existence list */
void
init_artifacts()
{
	(void) memset((genericptr_t) artiexist, 0, sizeof artiexist);
	hack_artifacts();
}

void
save_artifacts(fd)
int fd;
{
	bwrite(fd, (genericptr_t) artiexist, sizeof artiexist);
}

void
restore_artifacts(fd)
int fd;
{
	mread(fd, (genericptr_t) artiexist, sizeof artiexist);
	hack_artifacts();	/* redo non-saved special cases */
}

const char *
artiname(artinum)
int artinum;
{
	if (artinum <= 0 || artinum > NROFARTIFACTS) return("");
	return(artilist[artinum].name);
}

/*
   Make an artifact.  If a specific alignment is specified, then an object of
   the appropriate alignment is created from scratch, or 0 is returned if
   none is available.  If no alignment is given, then 'otmp' is converted
   into an artifact of matching type, or returned as-is if that's not possible.
   For the 2nd case, caller should use ``obj = mk_artifact(obj, A_NONE);''
   for the 1st, ``obj = mk_artifact((struct obj *)0, some_alignment);''.
 */
struct obj *
mk_artifact(otmp, alignment)
struct obj *otmp;	/* existing object; ignored if alignment specified */
aligntyp alignment;	/* target alignment, or A_NONE */
{
	register const struct artifact *a;
	register int n = 0, m;
	register boolean by_align = (alignment != A_NONE);
	register short o_typ = (by_align || !otmp) ? 0 : otmp->otyp;
	boolean unique = !by_align && otmp && objects[o_typ].oc_unique;

	/* count eligible artifacts */
	for (a = artilist+1,m = 1; a->otyp; a++,m++)
	    if ((by_align ? a->alignment == alignment : a->otyp == o_typ) &&
		(!(a->spfx & SPFX_NOGEN) || unique) && !artiexist[m]) {
		if (by_align && a->class == u.role)
		    goto make_artif;	/* 'a' points to the desired one */
		else
		    n++;
	    }

	if (n) {		/* found at least one candidate */
	    /* select one, then find it again */
	    if (n > 1) n = rnd(n);	/* [1..n] */
	    for (a = artilist+1,m = 1; a->otyp; a++,m++)
		if ((by_align ? a->alignment == alignment : a->otyp == o_typ)&&
		    (!(a->spfx & SPFX_NOGEN) || unique) && !artiexist[m]) {
		    if (!--n) break;	/* stop when chosen one reached */
		}

	    /* make an appropriate object if necessary, then christen it */
make_artif: if (by_align) otmp = mksobj((int)a->otyp, TRUE, FALSE);
	    otmp = oname(otmp, a->name);
	    otmp->oartifact = m;
	    artiexist[m] = TRUE;
	} else {
	    /* nothing appropriate could be found; return the original object */
	    if (by_align) otmp = 0;	/* (there was no original object) */
	}
	return otmp;
}

/*
 * Returns the full name (with articles and correct capitalization) of an
 * artifact named "name" if one exists, or NULL, it not.
 * The given name must be rather close to the real name for it to match.
 * The object type of the artifact is returned in otyp if the return value
 * is non-NULL.
 */
const char*
artifact_name(name, otyp)
const char *name;
short *otyp;
{
    register const struct artifact *a;
    register const char *aname;

    if(!strncmpi(name, "the ", 4)) name += 4;

    for (a = artilist+1; a->otyp; a++) {
	aname = a->name;
	if(!strncmpi(aname, "the ", 4)) aname += 4;
	if(!strcmpi(name, aname)) {
	    *otyp = a->otyp;
	    return a->name;
	}
    }

    return (char *)0;
}

boolean
exist_artifact(otyp, name)
register int otyp;
register const char *name;
{
	register const struct artifact *a;
	register boolean *arex;

	if (otyp && *name)
	    for (a = artilist+1,arex = artiexist+1; a->otyp; a++,arex++)
		if ((int) a->otyp == otyp && !strcmp(a->name, name))
		    return *arex;
	return FALSE;
}

void
artifact_exists(otmp, name, mod)
register struct obj *otmp;
register const char *name;
register boolean mod;
{
	register const struct artifact *a;

	if (otmp && *name)
	    for (a = artilist+1; a->otyp; a++)
		if (a->otyp == otmp->otyp && !strcmp(a->name, name)) {
		    register int m = a - artilist;
		    otmp->oartifact = (char)(mod ? m : 0);
		    otmp->age = 0;
		    if(otmp->otyp == RIN_INCREASE_DAMAGE)
			otmp->spe = 0;
		    artiexist[m] = mod;
		    break;
		}
	return;
}

int
nartifact_exist()
{
    int a = 0;
    int n = SIZE(artiexist);

    while(n > 1)
	if(artiexist[--n]) a++;

    return a;
}
#endif /* OVLB */
#ifdef OVL0

boolean
spec_ability(otmp, abil)
struct obj *otmp;
unsigned long abil;
{
	const struct artifact *arti = get_artifact(otmp);

	return((boolean)(arti && (arti->spfx & abil)));
}

#endif /* OVL0 */
#ifdef OVLB

boolean
restrict_name(otmp, name)  /* returns 1 if name is restricted for otmp->otyp */
register struct obj *otmp;
register const char *name;
{
	register const struct artifact *a;
	register const char *aname;

	if (!*name) return FALSE;
	if (!strncmpi(name, "the ", 4)) name += 4;

		/* Since almost every artifact is SPFX_RESTR, it doesn't cost
		   us much to do the string comparison before the spfx check.
		   Bug fix:  don't name multiple elven daggers "Sting".
		 */
	for (a = artilist+1; a->otyp; a++) {
	    if (a->otyp != otmp->otyp) continue;
	    aname = a->name;
	    if (!strncmpi(aname, "the ", 4)) aname += 4;
	    if (!strcmp(aname, name))
		return ((boolean)((a->spfx & (SPFX_NOGEN|SPFX_RESTR)) != 0 ||
			otmp->quan > 1L));
	}

	return FALSE;
}

static boolean
attacks(adtyp, otmp)
register int adtyp;
register struct obj *otmp;
{
	register const struct artifact *weap;

	if ((weap = get_artifact(otmp)) != 0)
		return((boolean)(weap->attk.adtyp == adtyp));
	return FALSE;
}

boolean
defends(adtyp, otmp)
register int adtyp;
register struct obj *otmp;
{
	register const struct artifact *weap;

	if ((weap = get_artifact(otmp)) != 0)
		return((boolean)(weap->defn.adtyp == adtyp));
	return FALSE;
}

/* used for monsters */
boolean
protects(adtyp, otmp)
int adtyp;
struct obj *otmp;
{
	register const struct artifact *weap;

	if ((weap = get_artifact(otmp)) != 0)
		return (boolean)(weap->cary.adtyp == adtyp);
	return FALSE;
}

/*
 * a potential artifact has just been worn/wielded/picked-up or
 * unworn/unwielded/dropped.  Pickup/drop only set/reset the W_ART mask.
 */
void
set_artifact_intrinsic(otmp,on,wp_mask)
register struct obj *otmp;
boolean on;
long wp_mask;
{
	long *mask = 0;
	register const struct artifact *oart = get_artifact(otmp);
	uchar dtyp;
	long spfx;
	
	if (!oart) return;

	/* effects from the defn field */
	dtyp = (wp_mask != W_ART) ? oart->defn.adtyp : oart->cary.adtyp;

	if (dtyp == AD_FIRE)
	    mask = &HFire_resistance;
	else if (dtyp == AD_COLD)
	    mask = &HCold_resistance;
	else if (dtyp == AD_ELEC)
	    mask = &HShock_resistance;
	else if (dtyp == AD_MAGM)
	    mask = &HAntimagic;
	else if (dtyp == AD_DISN)
	    mask = &HDisint_resistance;

	if(mask && wp_mask == W_ART && !on) {
	    /* find out if some other artifact also confers this intrinsic */
	    /* if so, leave the mask alone */
	    register struct obj* obj;
	    for(obj = invent; obj; obj = obj->nobj)
		if(obj != otmp && obj->oartifact) {
		    register const struct artifact *art = get_artifact(obj);
		    if(art->cary.adtyp == dtyp) {
			mask = (long *) 0;
			break;
		    }
		}
	}
	if(mask) {
	    if (on) *mask |= wp_mask;
	    else *mask &= ~wp_mask;
	}

	/* intrinsics from the spfx field; there could be more than one */
	spfx = (wp_mask != W_ART) ? oart->spfx : oart->cspfx;
	if(spfx && wp_mask == W_ART && !on) {
	    /* don't change any spfx also conferred by other artifacts */
	    register struct obj* obj;
	    for(obj = invent; obj; obj = obj->nobj)
		if(obj != otmp && obj->oartifact) {
		    register const struct artifact *art = get_artifact(obj);
		    spfx &= ~art->cspfx;
		}
	}

	if (spfx & SPFX_SEARCH) {
	    if(on) Searching |= wp_mask;
	    else Searching &= ~wp_mask;
	}
	if (spfx & SPFX_HALRES) {
	    /* make_hallucinated must (re)set the mask itself to get
	     * the display right */
	    make_hallucinated((long)!on, TRUE, wp_mask);
	}
	if (spfx & SPFX_ESP) {
	    if(on) HTelepat |= wp_mask;
	    else HTelepat &= ~wp_mask;
	    see_monsters();
	}
	if (spfx & SPFX_STLTH) {
	    if (on) Stealth |= wp_mask;
	    else Stealth &= ~wp_mask;
	}
	if (spfx & SPFX_REGEN) {
	    if (on) HRegeneration |= wp_mask;
	    else HRegeneration &= ~wp_mask;
	}
	if (spfx & SPFX_TCTRL) {
	    if (on) HTeleport_control |= wp_mask;
	    else HTeleport_control &= ~wp_mask;
	}
	if (spfx & SPFX_WARN) {
	    if (on) Warning |= wp_mask;
	    else Warning &= ~wp_mask;
	}
	if (spfx & SPFX_EREGEN) {
	    if (on) Energy_regeneration |= wp_mask;
	    else Energy_regeneration &= ~wp_mask;
	}
	if (spfx & SPFX_HSPDAM) {
	    if (on) Half_spell_damage |= wp_mask;
	    else Half_spell_damage &= ~wp_mask;
	}
	if (spfx & SPFX_HPHDAM) {
	    if (on) Half_physical_damage |= wp_mask;
	    else Half_physical_damage &= ~wp_mask;
	}
/*JP*/
	if (spfx & SPFX_PCTRL) {
	    if (on) Polymorph_control |= wp_mask;
	    else Polymorph_control &= ~wp_mask;
	}

	if(wp_mask == W_ART && !on && oart->inv_prop) {
	    /* might have to turn off invoked power too */
	    if (oart->inv_prop <= LAST_PROP &&
		(u.uprops[oart->inv_prop].p_flgs & W_ARTI))
		(void) arti_invoke(otmp);
	}
}

/*
 * creature (usually player) tries to touch (pick up or wield) an artifact obj.
 * Returns 0 if the object refuses to be touched.
 * This routine does not change any object chains.
 * Ignores such things as gauntlets, assuming the artifact is not
 * fooled by such trappings.
 */
int
touch_artifact(obj,mon)
    struct obj *obj;
    struct monst *mon;
{
    register const struct artifact *oart = get_artifact(obj);
    boolean badclass, badalign, self_willed, yours;

    if(!oart) return 1;

    yours = (mon == &youmonst);
    /* all quest artifacts are self-willed; it this ever changes, `badclass'
       will have to be extended to explicitly include quest artifacts */
    self_willed = ((oart->spfx & SPFX_INTEL) != 0);
    if (yours || !(mon->data->mflags3 & M3_WANTSALL)) {
	badclass = (oart->class && self_willed &&
		    (!yours || oart->class != u.role));
	badalign = (oart->spfx & SPFX_RESTR) &&
	    ((oart->alignment !=
	      (yours ? u.ualign.type : sgn(mon->data->maligntyp))) ||
	     (yours && u.ualign.record < 0));
    } else {	/* an M3_WANTSxxx monster */
	/* special monsters trying to take the Amulet, invocation tools or
	   quest item can touch anything except for `spec_applies' artifacts */
	badclass = badalign = FALSE;
    }
    /* weapons which attack specific categories of monsters are
       bad for them even if their alignments happen to match */
    if (!badalign && (oart->spfx & SPFX_DBONUS) != 0) {
	struct artifact tmp;

	tmp = *oart;
	tmp.spfx &= SPFX_DBONUS;
	badalign = !!spec_applies(&tmp, mon);
    }

    if (((badclass || badalign) && self_willed) ||
       (badalign && (!yours || !rn2(4))))  {
	int dmg;
	char buf[BUFSZ];

	if (!yours) return 0;
/*JP	You("are blasted by %s power!", s_suffix(the(xname(obj))));*/
	You("%sの力を浴びた！", s_suffix(the(xname(obj))));
	dmg = d((Antimagic ? 2 : 4), (self_willed ? 10 : 4));
/*JP	Sprintf(buf, "touching %s", oart->name);*/
	Sprintf(buf, "%sに触れて", jtrns_obj('A',oart->name));
	losehp(dmg, buf, KILLED_BY);
	exercise(A_WIS, FALSE);
    }

    /* can pick it up unless you're totally non-synch'd with the artifact */
    if (badclass && badalign && self_willed) {
/*JP	if (yours) pline("%s evades your grasp!", The(xname(obj)));*/
	if (yours) pline("%sは握ろうとするとするりと抜けた！", The(xname(obj)));
	return 0;
    }

    return 1;
}

#endif /* OVLB */
#ifdef OVL1

/* decide whether an artifact's special attacks apply against `ptr' */
STATIC_OVL int
spec_applies(weap, mtmp)
register const struct artifact *weap;
struct monst *mtmp;
{
	struct permonst *ptr;
	boolean yours;

	if(!(weap->spfx & (SPFX_DBONUS | SPFX_ATTK)))
	    return(weap->attk.adtyp == AD_PHYS);

	yours = (mtmp == &youmonst);
	ptr = mtmp->data;

	if (weap->spfx & SPFX_DMONS) {
#if 0	/* (we don't have anything like "PlayerBane" or "HealerSmasher" :-) */
	    if (weap->mtype == PM_PLAYERMON)
		return yours;
	    else if (player_mon() == &mons[(int)weap->mtype])
		return 1;
#endif
	    return (ptr == &mons[(int)weap->mtype]);
	} else if (weap->spfx & SPFX_DCLAS) {
	    return (weap->mtype == (unsigned long)ptr->mlet);
	} else if (weap->spfx & SPFX_DFLAG1) {
	    return ((ptr->mflags1 & weap->mtype) != 0L);
	} else if (weap->spfx & SPFX_DFLAG2) {
	    if (yours) {
		if ((weap->mtype & M2_HUMAN) != 0 &&
		    (Upolyd ? is_human(uasmon) : human_role()))
			return 1;
		if ((weap->mtype & M2_ELF) != 0 &&
		    (Upolyd ? is_elf(uasmon) : Role_is('E')))
			return 1;
	    }
	    return ((ptr->mflags2 & weap->mtype) != 0L);
	} else if (weap->spfx & SPFX_DALIGN) {
	    return yours ? (u.ualign.type != weap->alignment) :
			   (ptr->maligntyp == A_NONE ||
				sgn(ptr->maligntyp) != weap->alignment);
	} else if (weap->spfx & SPFX_ATTK) {
	    struct obj *defending_weapon = (yours ? uwep : MON_WEP(mtmp));

	    if (defending_weapon && defending_weapon->oartifact &&
		    defends((int)weap->attk.adtyp, defending_weapon))
		return FALSE;
	    switch(weap->attk.adtyp) {
		case AD_FIRE:
			return !(yours ? Fire_resistance : resists_fire(mtmp));
		case AD_COLD:
			return !(yours ? Cold_resistance : resists_cold(mtmp));
		case AD_ELEC:
			return !(yours ? Shock_resistance : resists_elec(mtmp));
		case AD_MAGM:
		case AD_STUN:
			return !(yours ? Antimagic : (rn2(100) < ptr->mr));
		case AD_DRLI:
			return !resists_drli(mtmp);
		case AD_STON:
			return !resists_ston(mtmp);
		default:	impossible("Weird weapon special attack.");
	    }
	}
	return(0);
}

/* special attack bonus */
int
spec_abon(otmp, mon)
struct obj *otmp;
struct monst *mon;
{
	register const struct artifact *weap = get_artifact(otmp);

	if (weap && spec_applies(weap, mon))
	    return weap->attk.damn ? rnd((int)weap->attk.damn) : 0;
	return 0;
}

/* special damage bonus */
int
spec_dbon(otmp, mon, tmp)
struct obj *otmp;
struct monst *mon;
int tmp;
{
	register const struct artifact *weap = get_artifact(otmp);

	if ((spec_dbon_applies = (weap && spec_applies(weap, mon))) != 0)
	    return weap->attk.damd ? rnd((int)weap->attk.damd) : max(tmp,1);
	return 0;
}

#endif /* OVL1 */

#ifdef OVLB

/* Function used when someone attacks someone else with an artifact
 * weapon.  Only adds the special (artifact) damage, and returns a 1 if it
 * did something special (in which case the caller won't print the normal
 * hit message).  This should be called once upon every artifact attack;
 * dmgval() no longer takes artifact bonuses into account.  Possible
 * extension: change the killer so that when an orc kills you with
 * Stormbringer it's "killed by Stormbringer" instead of "killed by an orc".
 */
boolean
artifact_hit(magr, mdef, otmp, dmgptr, dieroll)
struct monst *magr, *mdef;
struct obj *otmp;
int *dmgptr;
int dieroll; /* needed for Magicbane and vorpal blades */
{
	boolean youattack = (magr == &youmonst);
	boolean youdefend = (mdef == &youmonst);
	boolean vis = (!youattack && magr && cansee(magr->mx, magr->my))
		|| (!youdefend && cansee(mdef->mx, mdef->my));
	boolean realizes_damage;

/*JP	static const char you[] = "you";*/
	static const char you[] = "あなた";
	const char *hittee = youdefend ? you : mon_nam(mdef);

	/* The following takes care of most of the damage, but not all--
	 * the exception being for level draining, which is specially
	 * handled.  Messages are done in this function, however.
	 */
	*dmgptr += spec_dbon(otmp, mdef, *dmgptr);

	if (youattack && youdefend) {
		impossible("attacking yourself with weapon?");
		return FALSE;
	} else if (!spec_dbon_applies) {
		/* since damage bonus didn't apply, nothing more to do */
		return FALSE;
	}

	realizes_damage = (youdefend || vis);

	/* the four basic attacks: fire, cold, shock and missiles */
	if (attacks(AD_FIRE, otmp)) {
		if (realizes_damage) {
/*JP			pline_The("fiery blade burns %s!", hittee);*/
			pline("猛火が%sを焼いた！", hittee);
			return TRUE;
		}
	}
	if (attacks(AD_COLD, otmp)) {
		if (realizes_damage) {
/*JP			pline_The("ice-cold blade freezes %s!", hittee);*/
			pline("猛吹雪が%sを覆いつくした！",hittee);
			return TRUE;
		}
	}
	if (attacks(AD_ELEC, otmp)) {
		if (realizes_damage) {
			if(youattack && otmp != uwep)
/*JP			    pline("%s hits %s!", The(xname(otmp)), hittee);*/
			    pline("%sは%sに命中した！", The(xname(otmp)), hittee);
/*JP			pline("Lightning strikes %s!", hittee);*/
			pline("雷が%sに命中した！", hittee);
			return TRUE;
		}
	}
	if (attacks(AD_MAGM, otmp)) {
		if (realizes_damage) {
			if(youattack && otmp != uwep)
/*JP			    pline("%s hits %s!", The(xname(otmp)), hittee);*/
			    pline("%sは%sに命中した！", The(xname(otmp)), hittee);
/*JP			pline("A hail of magic missiles strikes %s!", hittee);*/
			pline("魔法の矢が雨あられと%sに命中した！", hittee);
			return TRUE;
		}
	}

	/*
	 * Magicbane's intrinsic magic is incompatible with normal
	 * enchantment magic.  Thus, its effects have a negative
	 * dependence on spe.  Against low mr victims, it typically
	 * does "double athame" damage, 2d4.  Occasionally, it will
	 * cast unbalancing magic which effectively averages out to
	 * 4d4 damage (2.5d4 against high mr victims), for spe = 0.
	 */

#define MB_MAX_DIEROLL		8    /* rolls above this aren't magical */
#define MB_INDEX_INIT		(-1)
#define MB_INDEX_PROBE		0
#define MB_INDEX_STUN		1
#define MB_INDEX_SCARE		2
#define MB_INDEX_PURGE		3
#define MB_RESIST_ATTACK	(resist_index = attack_index)
#define MB_RESISTED_ATTACK	(resist_index == attack_index)
#define MB_UWEP_ATTACK		(youattack && (otmp == uwep))

	if (attacks(AD_STUN, otmp) && (dieroll <= MB_MAX_DIEROLL)) {
		int attack_index = MB_INDEX_INIT;
		int resist_index = MB_INDEX_INIT;
		int scare_dieroll = MB_MAX_DIEROLL / 2;

		if (otmp->spe >= 3)
			scare_dieroll /= (1 << (otmp->spe / 3));

		*dmgptr += rnd(4);			/* 3d4 */

		if (otmp->spe > rn2(10))		/* probe */
			attack_index = MB_INDEX_PROBE;
		else {					/* stun */
			attack_index = MB_INDEX_STUN;
			*dmgptr += rnd(4);		/* 4d4 */

			if (youdefend)
				make_stunned((HStun + 3), FALSE);
			else
				mdef->mstun = 1;
		}
		if (dieroll <= scare_dieroll) {		/* scare */
			attack_index = MB_INDEX_SCARE;
			*dmgptr += rnd(4);		/* 5d4 */

			if (youdefend) {
				if (Antimagic)
					MB_RESIST_ATTACK;
				else {
					nomul(-3);
					nomovemsg = "";
					if ((magr == u.ustuck)
						&& sticks(uasmon)) {
					    u.ustuck = (struct monst *)0;
/*JP					    You("release %s!", mon_nam(magr));*/
					    You("%sを解放した！", mon_nam(magr));
					}
				}
			} else if (youattack) {
				if (rn2(2) && resist(mdef,SPBOOK_CLASS,0,0)) {
				    MB_RESIST_ATTACK;
				} else {
				    if (mdef == u.ustuck) {
					if (u.uswallow)
					    expels(mdef,mdef->data,TRUE);
					else {
					    if (!sticks(uasmon)) {
						u.ustuck = (struct monst *)0;
/*JP						You("get released!");*/
						You("解放された！");
					    }
					}
				    }
				    mdef->mflee = 1;
				    mdef->mfleetim += 3;
				}
			}
		}
		if (dieroll <= (scare_dieroll / 2)) {	/* purge */
			struct obj *ospell;
			struct permonst *old_uasmon = uasmon;

			attack_index = MB_INDEX_PURGE;
			*dmgptr += rnd(4);		/* 6d4 */

			/* Create a fake spell object, ala spell.c */
			ospell = mksobj(SPE_CANCELLATION, FALSE, FALSE);
			ospell->blessed = ospell->cursed = 0;
			ospell->quan = 20L;

			cancel_monst(mdef, ospell, youattack, FALSE, FALSE);

			if (youdefend) {
				if (old_uasmon != uasmon)
					/* rehumanized, no more damage */
					*dmgptr = 0;
				if (Antimagic)
					MB_RESIST_ATTACK;
			} else {
				if (!mdef->mcan)
					MB_RESIST_ATTACK;

				/* cancelled clay golems will die ... */
				else if (mdef->data == &mons[PM_CLAY_GOLEM])
					mdef->mhp = 1;
			}

			obfree(ospell, (struct obj *)0);
		}

		if (youdefend || mdef->mhp > 0) {  /* ??? -dkh- */
			static const char *mb_verb[4] =
/*JP				{"probe", "stun", "scare", "purge"};*/
				{"調べ", "くらくらさせ", "怯えさせ", "浄化し"};

			if (youattack || youdefend || vis) {
/*JP				pline_The("magic-absorbing blade %ss %s!",
					mb_verb[attack_index], hittee);*/
				pline("魔力を吸いとる刃が%sを%sた！",
					hittee, mb_verb[attack_index] );

				if (MB_RESISTED_ATTACK) {
/*JP					pline("%s resist%s!",*/
					pline("%sは防いだ！",
/*JP					youdefend ? "You" : Monnam(mdef),*/
					youdefend ? "あなた" : Monnam(mdef));
/*JP					youdefend ? "" : "s");*/

					shieldeff(youdefend ? u.ux : mdef->mx,
						youdefend ? u.uy : mdef->my);
				}
			}

			/* Much ado about nothing.  More magic fanfare! */
			if (MB_UWEP_ATTACK) {
				if (attack_index == MB_INDEX_PURGE) {
				    if (!MB_RESISTED_ATTACK &&
					attacktype(mdef->data, AT_MAGC)) {
/*JP					You("absorb magical energy!");*/
					You("魔法のエネルギーを吸いとった！");
					u.uenmax++;
					u.uen++;
					flags.botl = 1;
				    }
				} else if (attack_index == MB_INDEX_PROBE) {
				    if (!rn2(4 * otmp->spe)) {
/*JP					pline_The("probe is insightful!");*/
					pline("識別できた！");
					/* pre-damage status */
					probe_monster(mdef);
				    }
				}
			} else if (youdefend && !MB_RESISTED_ATTACK
				   && (attack_index == MB_INDEX_PURGE)) {
/*JP				You("lose magical energy!");*/
				You("魔法のエネルギーを失った！");
				if (u.uenmax > 0) u.uenmax--;
				if (u.uen > 0) u.uen--;
					flags.botl = 1;
			}

			/* all this magic is confusing ... */
			if (!rn2(12)) {
			    if (youdefend)
				make_confused((HConfusion + 4), FALSE);
			    else
				mdef->mconf = 1;

			    if (youattack || youdefend || vis)
/*JP				pline("%s %s confused.",
				      youdefend ? "You" : Monnam(mdef),
				      youdefend ? "are" : "is");*/
				pline("%sは混乱した．",
				      youdefend ? "あなた" : Monnam(mdef));
			}
		}
		return TRUE;
	}
	/* end of Magicbane code */

	/* We really want "on a natural 20" but Nethack does it in */
	/* reverse from AD&D. */
	if (spec_ability(otmp, SPFX_BEHEAD)) {
	    if (otmp->oartifact == ART_TSURUGI_OF_MURAMASA && dieroll == 1) {
		/* not really beheading, but so close, why add another SPFX */
		if (youattack && u.uswallow && mdef == u.ustuck) {
/*JP		    You("slice %s wide open!", mon_nam(mdef));*/
		    You("%sを輪切りにした！", mon_nam(mdef));
		    *dmgptr = mdef->mhp + FATAL_DAMAGE;
		    return TRUE;
		}
		if (!youdefend) {
			/* allow normal cutworm() call to add extra damage */
			if(notonhead)
			    return FALSE;

			if (bigmonst(mdef->data)) {
				if (youattack)
/*JP					You("slice deeply into %s!",*/
					You("%sを細切れにした！",
						mon_nam(mdef));
				else if (vis)
/*JP					pline("%s cuts deeply into %s!",*/
					pline("%sは%sを細切れにした！",
					      Monnam(magr), mon_nam(mdef));
				*dmgptr *= 2;
				return TRUE;
			}
			*dmgptr = mdef->mhp + FATAL_DAMAGE;
/*JP			pline_The("razor-sharp blade cuts %s in half!",*/
			pline("斬鉄剣が%sを真っ二つにした！",
			      mon_nam(mdef));
			otmp->dknown = TRUE;
			return TRUE;
		} else {
			if (bigmonst(uasmon)) {
/*JP				pline("%s cuts deeply into you!",*/
				pline("%sはあなたを細切れにした！",
					Monnam(magr));
				*dmgptr *= 2;
				return TRUE;
			}

			/* Players with negative AC's take less damage instead
			 * of just not getting hit.  We must add a large enough
			 * value to the damage so that this reduction in
			 * damage does not prevent death.
			 */
			*dmgptr = (Upolyd ? u.mh : u.uhp) + FATAL_DAMAGE;
/*JP
			pline_The("razor-sharp blade cuts you in half!");
*/
			pline("斬鉄剣があなたを真っ二つにした！");
			otmp->dknown = TRUE;
			return TRUE;
		}
	    } else if (otmp->oartifact == ART_VORPAL_BLADE &&
			(dieroll == 1 || mdef->data == &mons[PM_JABBERWOCK])) {
		static const char *behead_msg[2] = {
/*JP
		     "%s beheads %s!",
		     "%s decapitates %s!"
*/
		     "%sは%sの首を切った！",
		     "%sは%sの首を切り落した！"
		};

		if (youattack && u.uswallow && mdef == u.ustuck)
			return FALSE;
		if (!youdefend) {
			if (!has_head(mdef->data) || notonhead || u.uswallow) {
				if (youattack)
/*JP
					pline("Somehow, you miss %s wildly.",
*/
					pline("しかしながら，%sへの攻撃ははずれた．",
						mon_nam(mdef));
				else if (vis)
/*JP
					pline("Somehow, %s misses wildly.",
*/
					pline("しかしながら，%sの攻撃ははずれた．",
						mon_nam(magr));
				*dmgptr = 0;
				return ((boolean)(youattack || vis));
			}
			if (noncorporeal(mdef->data) || amorphous(mdef->data)) {
/*JP
				pline("%s slices through %s neck.",
*/
				pline("%sは%sの首を切り落した．",
/*JP
				      artilist[ART_VORPAL_BLADE].name,
*/
				      jtrns_obj('A', artilist[ART_VORPAL_BLADE].name),
				      s_suffix(mon_nam(mdef)));
				return TRUE;
			}
			*dmgptr = mdef->mhp + FATAL_DAMAGE;
			pline(behead_msg[rn2(SIZE(behead_msg))],
/*JP
			      artilist[ART_VORPAL_BLADE].name,
*/
			      jtrns_obj('A',artilist[ART_VORPAL_BLADE].name),
			      mon_nam(mdef));
			otmp->dknown = TRUE;
			return TRUE;
		} else {
			if (!has_head(uasmon)) {
/*JP
				pline("Somehow, %s misses you wildly.",
*/
				pline("しかしながら，%sの攻撃ははずれた．",
					mon_nam(magr));
				*dmgptr = 0;
				return TRUE;
			}
			if (noncorporeal(uasmon) || amorphous(uasmon)) {
/*JP
				pline("%s slices through your neck.",
				      artilist[ART_VORPAL_BLADE].name);
*/
				pline("%sはあなたの首を切り落した．",
				      jtrns_obj('A',artilist[ART_VORPAL_BLADE].name));
				return TRUE;
			}
			*dmgptr = (Upolyd ? u.mh : u.uhp) + FATAL_DAMAGE;
			pline(behead_msg[rn2(SIZE(behead_msg))],
/*JP
			      artilist[ART_VORPAL_BLADE].name, "you");
*/
			      jtrns_obj('A',artilist[ART_VORPAL_BLADE].name), "あなた");
			otmp->dknown = TRUE;
			/* Should amulets fall off? */
			return TRUE;
		}
	    }
	}
	if (spec_ability(otmp, SPFX_DRLI)) {
		if (!youdefend) {
			if (vis) {
			    if(otmp->oartifact == ART_STORMBRINGER)
/*JP				pline_The("%s blade draws the life from %s!",*/
				pline("%s刃が%sの生命力を奪った！",
				      hcolor(Black),
				      mon_nam(mdef));
			    else
/*JP				pline("%s draws the life from %s!",*/
				pline("%sは%sの生命力を奪った！",
				      The(distant_name(otmp, xname)),
				      mon_nam(mdef));
			}
			if (mdef->m_lev == 0) {
			    *dmgptr = mdef->mhp + FATAL_DAMAGE;
			} else {
			    int drain = rnd(8);
			    *dmgptr += drain;
			    mdef->mhpmax -= drain;
			    mdef->m_lev--;
			    drain /= 2;
			    if (drain) healup(drain, 0, FALSE, FALSE);
			}
			return vis;
		} else { /* youdefend */
			int oldhpmax = u.uhpmax;

			if (Blind)
/*JP				You_feel("an %s drain your life!",*/
				pline("%sに生命力を奪われたような気がした！",
				    otmp->oartifact == ART_STORMBRINGER ?
/*JP				    "unholy blade" : "object");*/
				    "不浄な刃" : "何か");
			else if (otmp->oartifact == ART_STORMBRINGER)
/*JP				pline_The("%s blade drains your life!",*/
				pline("%s刃があなたの生命力を奪った！",
				      hcolor(Black));
			else
/*JP				pline("%s drains your life!",*/
				pline("%sがあなたの生命力を奪った！",
				      The(distant_name(otmp, xname)));
			losexp();
			if (magr->mhp < magr->mhpmax) {
			    magr->mhp += (u.uhpmax - oldhpmax)/2;
			    if (magr->mhp > magr->mhpmax) magr->mhp = magr->mhpmax;
			}
			return TRUE;
		}
	}
	return FALSE;
}

static NEARDATA const char recharge_type[] = { ALLOW_COUNT, ALL_CLASSES, 0 };
static NEARDATA const char invoke_types[] = { ALL_CLASSES, 0 };
		/* #invoke: an "ugly check" filters out most objects */

int
doinvoke()
{
    register struct obj *obj;

/*JP    obj = getobj(invoke_types, "invoke");*/
    obj = getobj(invoke_types, "の魔力を使う");
    if(!obj) return 0;
    return arti_invoke(obj);
}

STATIC_OVL int
arti_invoke(obj)
    register struct obj *obj;
{
    register const struct artifact *oart = get_artifact(obj);

    if(!oart || !oart->inv_prop) {
	if(obj->otyp == CRYSTAL_BALL)
	    use_crystal_ball(obj);
	else
/*JP	    pline("Nothing happens.");*/
	    pline("何も起きなかった．");
	return 1;
    }

    if(oart->inv_prop > LAST_PROP) {
	/* It's a special power, not "just" a property */
	if(obj->age > monstermoves) {
	    /* the artifact is tired :-) */
/*JP	    You_feel("that %s is ignoring you.", the(xname(obj)));*/
	    pline("%sが無視しているように感じた．", the(xname(obj)));
	    /* and just got more so; patience is essential... */
	    obj->age += (long) d(3,10);
	    return 1;
	}
	obj->age = monstermoves + rnz(100);

	switch(oart->inv_prop) {
	case TAMING: {
	    struct obj *pseudo = mksobj(SPE_CHARM_MONSTER, FALSE, FALSE);
	    pseudo->blessed = pseudo->cursed = 0;
	    pseudo->quan = 20L;			/* do not let useup get it */
	    (void) seffects(pseudo);
	    obfree(pseudo, (struct obj *)0);	/* now, get rid of it */
	    break;
	  }
	case HEALING: {
	    int healamt = (u.uhpmax + 1 - u.uhp) / 2;
	    if (Upolyd) healamt = (u.mhmax + 1 - u.mh) / 2;
	    if(healamt || Sick || (Blinded > 1))
/*JP		You_feel("better.");*/
		You("気分がよくなった．");
	    else
		goto nothing_special;
	    if (healamt > 0) {
		if (Upolyd) u.mh += healamt;
		else u.uhp += healamt;
	    }
	    if(Sick) make_sick(0L,(char *)0,FALSE,SICK_ALL);
	    if(Blinded > 1) make_blinded(0L,FALSE);
	    flags.botl = 1;
	    break;
	  }
	case ENERGY_BOOST: {
	    int epboost = (u.uenmax + 1 - u.uen) / 2;
	    if (epboost > 120) epboost = 120;		/* arbitrary */
	    else if (epboost < 12) epboost = u.uenmax - u.uen;
	    if(epboost) {
/*JP		You_feel("re-energized.");*/
		You("エネルギーが満たされた．");
		u.uen += epboost;
		flags.botl = 1;
	    } else
		goto nothing_special;
	    break;
	  }
	case UNTRAP: {
	    if(!untrap(TRUE)) {
		obj->age = 0; /* don't charge for changing their mind */
		return 0;
	    }
	    break;
	  }
	case CHARGE_OBJ: {
/*JP	    struct obj *otmp = getobj(recharge_type, "charge");*/
	    struct obj *otmp = getobj(recharge_type, "充填する");
	    boolean b_effect;

	    if (!otmp) {
		obj->age = 0;
		return 0;
	    }
	    b_effect = obj->blessed && (u.role == oart->class || !oart->class);
	    recharge(otmp, b_effect ? 1 : obj->cursed ? -1 : 0);
	    break;
	  }
	case LEV_TELE:
	    level_tele();
	    break;
	case CREATE_PORTAL: {
	    int i, num_ok_dungeons, last_ok_dungeon = 0;
	    d_level newlev;
	    extern int n_dgns; /* from dungeon.c */
	    winid tmpwin = create_nhwindow(NHW_MENU);
	    anything any;

	    any.a_void = 0;	/* set all bits to zero */
	    start_menu(tmpwin);
	    /* use index+1 (cant use 0) as identifier */
	    for (i = num_ok_dungeons = 0; i < n_dgns; i++) {
		if (!dungeons[i].dunlev_ureached) continue;
		any.a_int = i+1;
		add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
/*JP			 dungeons[i].dname, MENU_UNSELECTED);*/
			 jtrns_obj('d', dungeons[i].dname), MENU_UNSELECTED);
		num_ok_dungeons++;
		last_ok_dungeon = i;
	    }
/*JP	    end_menu(tmpwin, "Open a portal to which dungeon?");*/
	    end_menu(tmpwin, "どの迷宮への魔法の入口を開きますか？");
	    if (num_ok_dungeons > 1) {
		/* more than one entry; display menu for choices */
		menu_item *selected;
		int n;

		n = select_menu(tmpwin, PICK_ONE, &selected);
		if (n <= 0) {
		    destroy_nhwindow(tmpwin);
		    goto nothing_special;
		}
		i = selected[0].item.a_int - 1;
		free((genericptr_t)selected);
	    } else
		i = last_ok_dungeon;	/* also first & only OK dungeon */
	    destroy_nhwindow(tmpwin);

	    /*
	     * i is now index into dungeon structure for the new dungeon.
	     * Find the closest level in the given dungeon, open
	     * a use-once portal to that dungeon and go there.
	     * The closest level is either the entry or dunlev_ureached.
	     */
	    newlev.dnum = i;
	    if(dungeons[i].depth_start >= depth(&u.uz))
		newlev.dlevel = dungeons[i].entry_lev;
	    else
		newlev.dlevel = dungeons[i].dunlev_ureached;
	    if(u.uhave.amulet || In_endgame(&u.uz) || In_endgame(&newlev) ||
	       newlev.dnum == u.uz.dnum) {
/*JP		You_feel("very disoriented for a moment.");*/
		You("一瞬方向感覚を失った．");
	    } else {
/*JP		if(!Blind) You("are surrounded by a shimmering sphere!");
		else You_feel("weightless for a moment.");*/
		if(!Blind) You("チカチカ光る球体に覆われた！");
		else You("一瞬，無重力感を感じた！");
		goto_level(&newlev, FALSE, FALSE, FALSE);
	    }
	    break;
	  }
	}
    } else {
	long cprop = (u.uprops[oart->inv_prop].p_flgs ^= W_ARTI);
	boolean on = (cprop & W_ARTI) != 0; /* true if invoked prop just set */

	if(on && obj->age > monstermoves) {
	    /* the artifact is tired :-) */
	    u.uprops[oart->inv_prop].p_flgs ^= W_ARTI;
/*JP	    You_feel("that %s is ignoring you.", the(xname(obj)));*/
	    pline("あなたをは%sが無視しているように感じた．", the(xname(obj)));
	    return 1;
	} else if(!on) {
	    /* when turning off property, determine downtime */
	    /* arbitrary for now until we can tune this -dlc */
	    obj->age = monstermoves + rnz(100);
	}

	if(cprop & ~W_ARTI) {
nothing_special:
	    /* you had the property from some other source too */
	    if (carried(obj))
/*JP		You_feel("a surge of power, but nothing seems to happen.");*/
		You("力が渦巻いたような気がした，しかし何も起きなかった．");
	    return 1;
	}
	switch(oart->inv_prop) {
	case CONFLICT:
/*JP	    if(on) You_feel("like a rabble-rouser.");*/
	    if(on) You("民衆扇動家のような気がした．");
/*JP	    else You_feel("the tension decrease around you.");*/
	    else pline("まわりの緊張感がなくなったような気がした．");
	    break;
	case LEVITATION:
	    if(on) float_up();
	    else (void) float_down(I_SPECIAL|W_ARTI|TIMEOUT);
	    break;
	case INVIS:
	    if (!See_invisible && !Blind) {
		newsym(u.ux,u.uy);
		if (on) {
/*JP		    Your("body takes on a %s transparency...",*/
		    pline("%s，体は透過性をもった．．．",
/*JP			 Hallucination ? "normal" : "strange");*/
			 Hallucination ? "あたりまえのことだが" : "奇妙なことに");
		} else {
/*JP		    Your("body seems to unfade...");*/
		    Your("体は次第に現れてきた．．．");
		}
	    } else goto nothing_special;
	    break;
	}
    }

    return 1;
}

#endif /* OVLB */

/*artifact.c*/
