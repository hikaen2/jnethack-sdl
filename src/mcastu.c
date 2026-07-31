/*	SCCS Id: @(#)mcastu.c	3.2	96/05/01	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"

STATIC_DCL void FDECL(cursetxt,(struct monst *));

#ifdef OVL0

extern const char *flash_types[];	/* from zap.c */

STATIC_OVL
void
cursetxt(mtmp)
	register struct monst *mtmp;
{
	if(canseemon(mtmp)) {
	    if ((Invis && !perceives(mtmp->data) &&
				(mtmp->mux != u.ux || mtmp->muy != u.uy))
			|| u.usym == S_MIMIC_DEF || u.uundetected)
/*JP		pline("%s points and curses in your general direction.",*/
		pline("%sはあなたのいるあたりを指差し，呪いをかけた．",
				Monnam(mtmp));
	    else if (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy))
/*JP		pline("%s points and curses at your displaced image.",*/
		pline("%sはあなた幻影を指差し，呪いをかけた．",
				Monnam(mtmp));
	    else
/*JP		pline("%s points at you, then curses.", Monnam(mtmp));*/
		pline("%sはあなたを指差し，呪いをかけた．", Monnam(mtmp));
	} else if((!(moves%4) || !rn2(4)) && flags.soundok)
/*JP		Norep("You hear a mumbled curse.");*/
		Norep("呪いの言葉をつぶやく声を聞いた．");
}

#endif /* OVL0 */
#ifdef OVLB

int
castmu(mtmp, mattk)	/* monster casts spell at you */
	register struct monst *mtmp;
	register struct attack *mattk;
{
	int	dmg, ml = mtmp->m_lev;

	if(mtmp->mcan || mtmp->mspec_used || !ml) {  /* could not attack */
	    cursetxt(mtmp);
	    return(0);
	} else {
	    nomul(0);
	    if(rn2(ml*10) < (mtmp->mconf ? 100 : 20)) {	/* fumbled attack */
		if (canseemon(mtmp) && flags.soundok)
/*JP		    pline_The("air crackles around %s.", mon_nam(mtmp));*/
		    pline("%sの回りの空気はピリピリしている．", mon_nam(mtmp));
		return(0);
	    }
	}
/*
 *	As these are spells, the damage is related to the level
 *	of the monster casting the spell.
 */
	if (mattk->damd)
		dmg = d((int)((ml/3) + mattk->damn), (int)mattk->damd);
	else dmg = d((int)((ml/3) + 1), 6);
	if (Half_spell_damage) dmg = (dmg+1) / 2;

	switch(mattk->adtyp)   {

	    case AD_FIRE:
/*JP		pline("You're enveloped in flames.");*/
		You("炎につつまれた．");
		if(Fire_resistance) {
			shieldeff(u.ux, u.uy);
/*JP			pline("But you resist the effects.");*/
			pline("しかし，あなたは影響を受けない．");
			dmg = 0;
		}
		break;
	    case AD_COLD:
/*JP		pline("You're covered in frost.");*/
		You("氷に覆われた．");
		if(Cold_resistance) {
			shieldeff(u.ux, u.uy);
/*JP			pline("But you resist the effects.");*/
			pline("しかし，あなたは影響を受けない．");
			dmg = 0;
		}
		break;
	    case AD_MAGM:
/*JP		You("are hit by a shower of missiles!");*/
		You("魔法の矢をくらった！");
		if(Antimagic) {
			shieldeff(u.ux, u.uy);
/*JP			pline_The("missiles bounce off!");*/
			pline("魔法の矢は反射した！");
			dmg = 0;
		} else dmg = d((int)mtmp->m_lev/2 + 1,6);
		break;
	    case AD_SPEL:	/* random spell */

		mtmp->mspec_used = 10 - mtmp->m_lev;
		if (mtmp->mspec_used < 2) mtmp->mspec_used = 2;
		switch(rn2((int)mtmp->m_lev)) {
		    case 22:
		    case 21:
		    case 20:
/*JP			pline("Oh no, %s's using the touch of death!",
			      humanoid(mtmp->data)
				  ? (mtmp->female ? "she" : "he")
				  : "it"
			     );*/
			pline("なんてこったい，%sは死の宣告を使っている！",
			      Monnam(mtmp));
			if (nonliving(uasmon) || is_demon(uasmon))
/*JP			    You("seem no deader than before.");*/
			    You("死なない体のようだ．");
			else if (!Antimagic && rn2(ml) > 12) {

			    if(Hallucination)
/*JP				You("have an out of body experience.");*/
				You("幽体離脱の体験をした．");
			    else  {
				killer_format = KILLED_BY_AN;
/*JP				killer = "touch of death";*/
				killer = "死の宣告で";
				done(DIED);
			    }
			} else {
				if(Antimagic) shieldeff(u.ux, u.uy);
/*JP				pline("Lucky for you, it didn't work!");*/
				pline("運のよいことになんともなかった！");
			}
			dmg = 0;
			break;
		    case 19:
		    case 18:
			if(mtmp->iswiz && flags.no_of_wizards == 1) {
/*JP				pline("Double Trouble...");*/
				pline("二重苦だ．．．");
				clonewiz();
				dmg = 0;
				break;
			} /* else fall into the next case */
		    case 17:
		    case 16:
		    case 15:
			if(mtmp->iswiz)
/*JP			    verbalize("Destroy the thief, my pets!");*/
			    verbalize("盗賊を殺せ！下僕よ！");
			nasty(mtmp);	/* summon something nasty */
			/* fall into the next case */
		    case 14:		/* aggravate all monsters */
		    case 13:
			aggravate();
			dmg = 0;
			break;
		    case 12:		/* curse random items */
		    case 11:
		    case 10:
			rndcurse();
			dmg = 0;
			break;
		    case 9:
		    case 8:		/* destroy armor */
			if (Antimagic) {
				shieldeff(u.ux, u.uy);
/*JP				pline("A field of force surrounds you!");*/
				pline("不思議な力があなたをとりかこんだ！");
			} else if(!destroy_arm(some_armor()))
/*JP				Your("skin itches.");*/
				Your("皮膚はムズムズした．");
			dmg = 0;
			break;
		    case 7:
		    case 6:		/* drain strength */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
/*JP			    You_feel("momentarily weakened.");*/
			    You("一瞬弱くなったような気がした．");
			} else {
/*JP			    You("suddenly feel weaker!");*/
			    You("突然弱くなったような気がした．");
			    dmg = ml - 6;
			    if(Half_spell_damage) dmg = (dmg+1) / 2;
			    losestr(rnd(dmg));
			    if(u.uhp < 1)
				done_in_by(mtmp);
			}
			dmg = 0;
			break;
		    case 5:		/* make invisible if not */
		    case 4:
			if (!mtmp->minvis && !mtmp->invis_blkd) {
			    if(canseemon(mtmp) && !See_invisible)
/*JP				pline("%s suddenly disappears!", Monnam(mtmp));*/
				pline("%sは突然消えた！", Monnam(mtmp));
			    mon_set_minvis(mtmp);
			    dmg = 0;
			    break;
			} /* else fall into the next case */
		    case 3:		/* stun */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
			    if(!Stunned)
/*JP				You_feel("momentarily disoriented.");*/
				You("一瞬方向感覚を失った．");
			    make_stunned(1L, FALSE);
			} else {
			    if (Stunned)
/*JP				You("struggle to keep your balance.");*/
				You("バランスを取ろうともがいた．");
			    else
/*JP				You("reel...");*/
				You("よろめいた．．．");
			    dmg = d(ACURR(A_DEX) < 12 ? 6 : 4, 4);
			    if(Half_spell_damage) dmg = (dmg+1) / 2;
			    make_stunned(HStun + dmg, FALSE);
			}
			dmg = 0;
			break;
		    case 2:		/* haste self */
			if(mtmp->mspeed == MSLOW)	mtmp->mspeed = 0;
			else				mtmp->mspeed = MFAST;
			dmg = 0;
			break;
		    case 1:		/* cure self */
			if(mtmp->mhp < mtmp->mhpmax) {
			    if((mtmp->mhp += rnd(8)) > mtmp->mhpmax)
				mtmp->mhp = mtmp->mhpmax;
			    dmg = 0;
			    break;
			} /* else fall through to default case */
		    default:		/* psi bolt */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
/*JP			    You("get a slight %sache.",body_part(HEAD));*/
			    You("ちょっと%s痛がした．",body_part(HEAD));
			    dmg = 1;
			} else {
			    if (dmg <= 10)
/*JP				Your("brain is on fire!");*/
			      You("怒りにつつまれた！");
/*JP			    else Your("%s suddenly aches!", body_part(HEAD));*/
			    else Your("%sは突然痛みを感じた！", body_part(HEAD));
			}
			break;
		}
		break;
		
	    case AD_CLRC:	/* clerical spell */

		mtmp->mspec_used = 10 - mtmp->m_lev;
		if (mtmp->mspec_used < 2) mtmp->mspec_used = 2;
		switch(rn2((int)mtmp->m_lev)) {
		    /* Other ideas: lightning bolts, towers of flame,
				    gush of water  -3. */

		    default:		/* confuse */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
/*JP			    You_feel("momentarily dizzy.");*/
			    You("一瞬めまいがした．");
			} else {
			    dmg = (int)mtmp->m_lev;
			    if(Half_spell_damage) dmg = (dmg+1) / 2;
			    make_confused(HConfusion + dmg, TRUE);
			}
			dmg = 0;
			break;
		    case 12:		/* curse random items */
		    case 11:
		    case 10:
			rndcurse();
			dmg = 0;
			break;
		    case 9:
		    case 8:		/* insects */
			/* Try for insects, and if there are none
			   left, go for (sticks to) snakes.  -3. */
			{
			int i;
			struct permonst *pm = mkclass(S_ANT,0);
			struct monst *mtmp2;
			char let = (pm ? S_ANT : S_SNAKE);

			for (i = 0; i <= (int) mtmp->m_lev; i++)
			   if ((pm = mkclass(let,0)) &&
			(mtmp2 = makemon(pm, u.ux, u.uy, NO_MM_FLAGS))) {
				mtmp2->msleep = mtmp2->mpeaceful =
					mtmp2->mtame = 0;
				set_malign(mtmp2);
			    }
			}			
			dmg = 0;
			break;
		    case 6:
		    case 7:		/* blindness */
			/* note: resists_blnd() doesn't apply here */
			if (!Blinded) {
/*JP			    pline("Scales cover your eyes!");*/
			    pline("鱗があなたの目を覆った！");
			    make_blinded(Half_spell_damage ? 100L:200L, FALSE);
			    dmg = 0;
			    break;
			}
		    case 4:
		    case 5:		/* wound */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
/*JP			    Your("skin itches badly for a moment.");*/
			    Your("皮膚は一瞬，ムスムスっとした．");
			    dmg = 0;
			} else {
/*JP			    pline("Wounds appear on your body!");*/
			    pline("傷があなたの体に出来た！");
			    dmg = d(2,8) + 1;
			    if (Half_spell_damage) dmg = (dmg+1) / 2;
			}
			break;
		    case 3:		/* hold */
			if(Antimagic) {
			    shieldeff(u.ux, u.uy);
			    if(multi >= 0)
/*JP				You("stiffen briefly.");*/
				You("一瞬硬直した．");
			    nomul(-1);
			} else {
			    if (multi >= 0)
/*JP				You("are frozen in place!");*/
				You("その場で動けなくなった！");
			    dmg = 4 + (int)mtmp->m_lev;
			    if (Half_spell_damage) dmg = (dmg+1) / 2;
			    nomul(-dmg);
			}
			dmg = 0;
			break;
		    case 2:
		    case 1:		/* cure self */
			if(mtmp->mhp < mtmp->mhpmax) {
			    if((mtmp->mhp += rnd(8)) > mtmp->mhpmax)
				mtmp->mhp = mtmp->mhpmax;
			    dmg = 0;
			    break;
			} /* else fall through to default case */
		}
	}
	if(dmg) mdamageu(mtmp, dmg);
	return(1);
}

#endif /* OVLB */
#ifdef OVL0

/* convert 1..10 to 0..9; add 10 for second group (spell casting) */
#define ad_to_typ(k) (10 + (int)k - 1)

int
buzzmu(mtmp, mattk)		/* monster uses spell (ranged) */
	register struct monst *mtmp;
	register struct attack  *mattk;
{
	if(mtmp->mcan || mattk->adtyp > AD_SPC2) {
	    cursetxt(mtmp);
	    return(0);
	}
	if(lined_up(mtmp) && rn2(3)) {
	    nomul(0);
	    if(mattk->adtyp && (mattk->adtyp < 11)) { /* no cf unsigned >0 */
		if(canseemon(mtmp))
/*JP		    pline("%s zaps you with a %s!", Monnam(mtmp),*/
		    pline("%sは%sをあなたに向けて放った．", Monnam(mtmp),
			  flash_types[ad_to_typ(mattk->adtyp)]);
		buzz(-ad_to_typ(mattk->adtyp), (int)mattk->damn,
		     mtmp->mx, mtmp->my, sgn(tbx), sgn(tby));
	    } else impossible("Monster spell %d cast", mattk->adtyp-1);
	}
	return(1);
}

#endif /* OVL0 */

/*mcastu.c*/
