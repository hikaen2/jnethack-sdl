/*	SCCS Id: @(#)lock.c	3.2	96/05/31	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"

STATIC_PTR int NDECL(picklock);
STATIC_PTR int NDECL(forcelock);

/* at most one of `door' and `box' should be non-null at any given time */
STATIC_VAR NEARDATA struct xlock_s {
	struct rm  *door;
	struct obj *box;
	int picktyp, chance, usedtime;
} xlock;

#ifdef OVLB

static const char *NDECL(lock_action);
static boolean FDECL(obstructed,(int,int));
static void FDECL(chest_shatter_msg, (struct obj *));

boolean
picking_lock(x, y)
	int *x, *y;
{
	if (occupation == picklock) {
	    *x = u.ux + u.dx;
	    *y = u.uy + u.dy;
	    return TRUE;
	} else {
	    *x = *y = 0;
	    return FALSE;
	}
}

boolean
picking_at(x, y)
int x, y;
{
	return (boolean)(occupation == picklock && xlock.door == &levl[x][y]);
}

/* produce an occupation string appropriate for the current activity */
static const char *
lock_action()
{
	/* "unlocking"+2 == "locking" */
	static const char *actions[] = {
/* 
** 英語は un をつけるだけで逆の意味になるが，日本語はそうはいかない．
** 誰だ？こんな数バイトけちるコード書いたやつは？
*/
#if 0 /*JP*/
		/* [0] */	"unlocking the door",
		/* [1] */	"unlocking the chest",
		/* [2] */	"unlocking the box",
		/* [3] */	"picking the lock"
#endif /*JP*/
		/* [0] */	"扉の鍵をはずす", 
		/* [1] */	"宝箱の鍵をはずす",
		/* [2] */	"箱の鍵をはずす",  
		/* [3] */	"鍵をはずす"    
	};

	/* if the target is currently unlocked, we're trying to lock it now */
	if (xlock.door && !(xlock.door->doormask & D_LOCKED))
/*JP		return actions[0]+2;*/	/* "locking the door" */
		return "扉に鍵をかける";
	else if (xlock.box && !xlock.box->olocked)
/*JP		return xlock.box->otyp == CHEST ? actions[1]+2 : actions[2]+2;*/
		return xlock.box->otyp == CHEST ? "宝箱に鍵をかける" : "箱に鍵をかける";
	/* otherwise we're trying to unlock it */
	else if (xlock.picktyp == LOCK_PICK)
		return actions[3];	/* "picking the lock" */
#ifdef TOURIST
	else if (xlock.picktyp == CREDIT_CARD)
		return actions[3];	/* same as lock_pick */
#endif
	else if (xlock.door)
		return actions[0];	/* "unlocking the door" */
	else
		return xlock.box->otyp == CHEST ? actions[1] : actions[2];
}

STATIC_PTR
int
picklock()	/* try to open/close a lock */
{
#ifdef NEWBIE
	newbie.unlock = 1;
#endif
	if (xlock.box) {
	    if((xlock.box->ox != u.ux) || (xlock.box->oy != u.uy)) {
		return((xlock.usedtime = 0));		/* you or it moved */
	    }
	} else {		/* door */
	    if(xlock.door != &(levl[u.ux+u.dx][u.uy+u.dy])) {
		return((xlock.usedtime = 0));		/* you moved */
	    }
	    switch (xlock.door->doormask) {
		case D_NODOOR:
/*JP		    pline("This doorway has no door.");*/
		    pline("出入口には扉がない．");
		    return((xlock.usedtime = 0));
		case D_ISOPEN:
/*JP		    You("cannot lock an open door.");*/
		    pline("開いてる扉に鍵をかけられない．");
		    return((xlock.usedtime = 0));
		case D_BROKEN:
/*JP		    pline("This door is broken.");*/
		    pline("扉は壊れている");
		    return((xlock.usedtime = 0));
	    }
	}

	if (xlock.usedtime++ >= 50 || nohands(uasmon)) {
/*JP	    You("give up your attempt at %s.", lock_action());*/
	    pline("%sのをあきらめた．", lock_action());
	    exercise(A_DEX, TRUE);	/* even if you don't succeed */
	    return((xlock.usedtime = 0));
	}

	if(rn2(100) > xlock.chance) return(1);		/* still busy */

/*JP	You("succeed in %s.", lock_action());*/
	You("%sのに成功した．", lock_action());
	if (xlock.door) {
	    if(xlock.door->doormask & D_TRAPPED) {
/*JP		    b_trapped("door", FINGER);*/
		    b_trapped("扉", FINGER);
		    xlock.door->doormask = D_NODOOR;
		    unblock_point(u.ux+u.dx, u.uy+u.dy);
		    if (*in_rooms(u.ux+u.dx, u.uy+u.dy, SHOPBASE))
			add_damage(u.ux+u.dx, u.uy+u.dy, 0L);
		    newsym(u.ux+u.dx, u.uy+u.dy);
	    } else if (xlock.door->doormask & D_LOCKED)
		xlock.door->doormask = D_CLOSED;
	    else xlock.door->doormask = D_LOCKED;
	} else {
	    xlock.box->olocked = !xlock.box->olocked;
	    if(xlock.box->otrapped)	
		(void) chest_trap(xlock.box, FINGER, FALSE);
	}
	exercise(A_DEX, TRUE);
	return((xlock.usedtime = 0));
}

STATIC_PTR
int
forcelock()	/* try to force a locked chest */
{

	register struct obj *otmp;

	if((xlock.box->ox != u.ux) || (xlock.box->oy != u.uy))
		return((xlock.usedtime = 0));		/* you or it moved */

	if (xlock.usedtime++ >= 50 || !uwep || nohands(uasmon)) {
/*JP	    You("give up your attempt to force the lock.");*/
	    pline("鍵をこじ開けるのをあきらめた．");
	    if(xlock.usedtime >= 50)		/* you made the effort */
	      exercise((xlock.picktyp) ? A_DEX : A_STR, TRUE);
	    return((xlock.usedtime = 0));
	}

	if(xlock.picktyp) {	/* blade */

	    if(rn2(1000-(int)uwep->spe) > (992-(int)uwep->oeroded*10) &&
	       !uwep->cursed && !obj_resists(uwep, 0, 99)) {
		/* for a +0 weapon, probability that it survives an unsuccessful
		 * attempt to force the lock is (.992)^50 = .67
		 */
/*JP
		pline("%sour %s broke!",
		      (uwep->quan > 1L) ? "One of y" : "Y", xname(uwep));
*/
	        pline("あなたの%sは壊れてしまった！",xname(uwep));
		useup(uwep);
/*JP		You("give up your attempt to force the lock.");*/
		pline("鍵をこじ開けるのをあきらめた．");
		exercise(A_DEX, TRUE);
		return((xlock.usedtime = 0));
	    }
	} else			/* blunt */
	    wake_nearby();	/* due to hammering on the container */

	if(rn2(100) > xlock.chance) return(1);		/* still busy */

/*JP	You("succeed in forcing the lock.");*/
	pline("鍵をこじ開けた．");
	xlock.box->olocked = 0;
	xlock.box->obroken = 1;
	if(!xlock.picktyp && !rn2(3)) {
	    struct monst *shkp;
	    boolean costly;
	    long loss = 0L;

	    costly = (*u.ushops && costly_spot(u.ux, u.uy));
	    shkp = costly ? shop_keeper(*u.ushops) : 0;

/*JP	    pline("In fact, you've totally destroyed %s.",
		  the(xname(xlock.box)));*/
	    pline("%sを完全に壊してしまった．",
		  the(xname(xlock.box)));

	    /* Put the contents on ground at the hero's feet. */
	    while ((otmp = xlock.box->cobj) != 0) {
		obj_extract_self(otmp);
		if(!rn2(3) || otmp->oclass == POTION_CLASS) {
		    chest_shatter_msg(otmp);
		    if (costly)
		        loss += stolen_value(otmp, u.ux, u.uy,
					     (boolean)shkp->mpeaceful, TRUE);
		    if (otmp->quan == 1L) {
			obfree(otmp, (struct obj *) 0);
			continue;
		    }
		    useup(otmp);
		}
		if (xlock.box->otyp == ICE_BOX && otmp->otyp == CORPSE) {
		    otmp->age = monstermoves - otmp->age; /* actual age */
		    start_corpse_timeout(otmp);
		}
		place_object(otmp, u.ux, u.uy);
		stackobj(otmp);
	    }

	    if (costly)
		loss += stolen_value(xlock.box, u.ux, u.uy,
					     (boolean)shkp->mpeaceful, TRUE);
/*JP	    if(loss) You("owe %ld zorkmids for objects destroyed.", loss);*/
	    if(loss) You("器物破損で%ldゴールドの借りをつくった．", loss);
	    delobj(xlock.box);
	}
	exercise((xlock.picktyp) ? A_DEX : A_STR, TRUE);
	return((xlock.usedtime = 0));
}

#endif /* OVLB */
#ifdef OVL0

void
reset_pick()
{
	xlock.usedtime = xlock.chance = xlock.picktyp = 0;
	xlock.door = 0;
	xlock.box = 0;
}

#endif /* OVL0 */
#ifdef OVLB

int
pick_lock(pick) /* pick a lock with a given object */
	register struct	obj	*pick;
{
	int x, y, picktyp, c, ch;
	struct rm	*door;
	struct obj	*otmp;
	char qbuf[QBUFSZ];

	picktyp = pick->otyp;

	/* check whether we're resuming an interrupted previous attempt */
	if (xlock.usedtime && picktyp == xlock.picktyp) {
/*JP	    static char no_longer[] = "Unfortunately, you can no longer %s %s.";*/
	    static char no_longer[] = "不幸にも，あなたは%s%s";

	    if (nohands(uasmon)) {
/*JP		const char *what = (picktyp == LOCK_PICK) ? "pick" : "key";*/
		const char *what = (picktyp == LOCK_PICK) ? "鍵開け器具" : "鍵";
#ifdef TOURIST
/*JP		if (picktyp == CREDIT_CARD) what = "card";*/
		if (picktyp == CREDIT_CARD) what = "カード";
#endif
/*JP		pline(no_longer, "hold the", what);*/
		pline(no_longer, what, "をつかめない");
		reset_pick();
		return 0;
	    } else if (xlock.box && !can_reach_floor()) {
/*JP		pline(no_longer, "reach the", "lock");*/
		pline(no_longer, "鍵に", "届かない");
		reset_pick();
		return 0;
	    } else {
		const char *action = lock_action();
/*JP		You("resume your attempt at %s.", action);*/
		pline("%sを再開した．", action);
		set_occupation(picklock, action, 0);
		return(1);
	    }
	}

	if(nohands(uasmon)) {
/*JP		You_cant("hold %s -- you have no hands!", doname(pick));*/
		You("%sをつかむことができない！--あなたには手がない！", xname(pick));
		return(0);
	}

	if((picktyp != LOCK_PICK &&
#ifdef TOURIST
	    picktyp != CREDIT_CARD &&
#endif
	    picktyp != SKELETON_KEY)) {
		impossible("picking lock with object %d?", picktyp);
		return(0);
	}
	if(!getdir((char *)0)) return(0);

	ch = 0;		/* lint suppression */
	x = u.ux + u.dx;
	y = u.uy + u.dy;
	if (x == u.ux && y == u.uy) {	/* pick lock on a container */
	    const char *verb;
	    boolean it;
	    int count;

	    if (u.dz < 0) {
/*JP		pline("There isn't any sort of lock up %s.",
		      Levitation ? "here" : "there");*/
		pline("%sには鍵をかけるような物はない．",
		      Levitation ? "ここ" : "下方");
		return 0;
	    } else if (is_lava(u.ux, u.uy)) {
/*JP		pline("Doing that would probably melt your %s.",*/
		pline("そんなことをしたら%sが溶けてしまう．",
		      xname(pick));
		return 0;
	    } else if (is_pool(u.ux, u.uy) && !Underwater) {
/*JP		pline_The("water has no lock.");*/
		pline("水に錠前はない");
		return 0;
	    }

	    count = 0;
	    c = 'n';			/* in case there are no boxes here */
	    for(otmp = level.objects[x][y]; otmp; otmp = otmp->nexthere)
		if (Is_box(otmp)) {
		    ++count;
		    if (!can_reach_floor()) {
/*JP			You_cant("reach %s from up here.", the(xname(otmp)));*/
			You("ここにある%sに届かない．", the(xname(otmp)));
			return 0;
		    }
		    it = 0;
#if 0 /*JP*/
		    if (otmp->obroken) verb = "fix";
		    else if (!otmp->olocked) verb = "lock", it = 1;
		    else if (picktyp != LOCK_PICK) verb = "unlock", it = 1;
		    else verb = "pick";
#endif /*JP*/
		    if (otmp->obroken) verb = "修復する";
		    else if (!otmp->olocked) verb = "鍵をかける", it = 1;
		    else if (picktyp != LOCK_PICK) verb = "鍵をはずす", it = 1;
		    else verb = "こじあける";
/*JP		    Sprintf(qbuf, "There is %s here, %s %s?",
			    doname(otmp), verb, it ? "it" : "its lock");*/
		    Sprintf(qbuf, "ここには%sがある．%s？",
			    doname(otmp), jconj(verb,"ますか"));

		    c = ynq(qbuf);
		    if(c == 'q') return(0);
		    if(c == 'n') continue;

		    if (otmp->obroken) {
/*JP			You_cant("fix its broken lock with %s.", doname(pick));*/
			You("壊れた鍵を%sで修復できない．", doname(pick));
			return 0;
		    }
#ifdef TOURIST
		    else if (picktyp == CREDIT_CARD && !otmp->olocked) {
			/* credit cards are only good for unlocking */
/*JP			You_cant("do that with %s.", doname(pick));*/
			pline("%sじゃそんなことはできない．", doname(pick));
			return 0;
		    }
#endif
		    switch(picktyp) {
#ifdef TOURIST
			case CREDIT_CARD:
			    ch = ACURR(A_DEX) + 20*Role_is('R');
			    break;
#endif
			case LOCK_PICK:
			    ch = 4*ACURR(A_DEX) + 25*Role_is('R');
			    break;
			case SKELETON_KEY:
			    ch = 75 + ACURR(A_DEX);
			    break;
			default:	ch = 0;
		    }
		    if(otmp->cursed) ch /= 2;

		    xlock.picktyp = picktyp;
		    xlock.box = otmp;
		    xlock.door = 0;
		    break;
		}
	    if (c != 'y') {
		if (!count)
/*JP		    pline("There doesn't seem to be any sort of lock here.");*/
		    pline("ここには鍵をかけるような物はないようだ．");
		return(0);		/* decided against all boxes */
	    }
	} else {			/* pick the lock in a door */
	    struct monst *mtmp;

	    door = &levl[x][y];
	    if ((mtmp = m_at(x, y)) && canseemon(mtmp)
			&& mtmp->m_ap_type != M_AP_FURNITURE
			&& mtmp->m_ap_type != M_AP_OBJECT) {
#ifdef TOURIST
		if (picktyp == CREDIT_CARD &&
		    (mtmp->isshk || mtmp->data == &mons[PM_ORACLE]))
/*JP		    verbalize("No checks, no credit, no problem.");*/
		    verbalize("いつもニコニコ現金払い．");
		else
#endif
/*JP		    pline("I don't think %s would appreciate that.", mon_nam(mtmp));*/
		    pline("%sがその価値を認めるとは思えない．", mon_nam(mtmp));
		return(0);
	    }
	    if(!IS_DOOR(door->typ)) {
		if (is_drawbridge_wall(x,y) >= 0)
/*JP		    You("%s no lock on the drawbridge.",
				Blind ? "feel" : "see");*/
		    pline("跳ね橋には鍵がない%s．",
				Blind ? "ようだ" : "ように見える");
		else
/*JP		    You("%s no door there.",
				Blind ? "feel" : "see");*/
		    pline("ここには扉がない%s．",
				Blind ? "ようだ" : "ように見える");
		return(0);
	    }
	    switch (door->doormask) {
		case D_NODOOR:
/*JP		    pline("This doorway has no door.");*/
	            pline("出入口には扉がない．");
		    return(0);
		case D_ISOPEN:
/*JP		    You("cannot lock an open door.");*/
		    pline("開いてる扉に鍵をかけれない．");
		    return(0);
		case D_BROKEN:
/*JP		    pline("This door is broken.");*/
		    pline("扉は壊れている");
		    return(0);
		default:
#ifdef TOURIST
		    /* credit cards are only good for unlocking */
		    if(picktyp == CREDIT_CARD && !(door->doormask & D_LOCKED)) {
/*JP			You_cant("lock a door with a credit card.");*/
		        You("クレジットカードで鍵をかけることはできない．");
			return(0);
		    }
#endif

/*JP
	これこそ真に利用価値のないテクニカルなコーディングだ！
							--issei
		    Sprintf(qbuf,"%sock it?",
			(door->doormask & D_LOCKED) ? "Unl" : "L" );*/
		    Sprintf(qbuf,"%s？",
			(door->doormask & D_LOCKED) ? "はずしますか" : "かけますか" );


		    c = yn(qbuf);
		    if(c == 'n') return(0);

		    switch(picktyp) {
#ifdef TOURIST
			case CREDIT_CARD:
			    ch = 2*ACURR(A_DEX) + 20*Role_is('R');
			    break;
#endif
			case LOCK_PICK:
			    ch = 3*ACURR(A_DEX) + 30*Role_is('R');
			    break;
			case SKELETON_KEY:
			    ch = 70 + ACURR(A_DEX);
			    break;
			default:    ch = 0;
		    }
		    xlock.door = door;
		    xlock.box = 0;
	    }
	}
	flags.move = 0;
	xlock.chance = ch;
	xlock.picktyp = picktyp;
	xlock.usedtime = 0;
	set_occupation(picklock, lock_action(), 0);
	return(1);
}

int
doforce()		/* try to force a chest with your weapon */
{
	register struct obj *otmp;
	register int c, picktyp;
	char qbuf[QBUFSZ];

	if(!uwep ||	/* proper type test */
	   (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep) &&
	    uwep->oclass != ROCK_CLASS) ||
	   (uwep->oclass != ROCK_CLASS &&
	    (objects[uwep->otyp].oc_wepcat == WEP_BOW ||
	     objects[uwep->otyp].oc_wepcat == WEP_AMMO ||
	     objects[uwep->otyp].oc_wepcat == WEP_MISSILE))
	   || (uwep->otyp > QUARTERSTAFF && uwep->otyp < BULLWHIP)
#ifdef KOPS
	   || uwep->otyp == RUBBER_HOSE
#endif
	  ) {
/*JP	    You_cant("force anything without a %sweapon.",
		  (uwep) ? "proper " : "");*/
	    pline("%s武器なしで鍵をこじあけることはできない．",
		  (uwep) ? "適切な" : "");
	    return(0);
	}

	picktyp = is_blade(uwep);
	if(xlock.usedtime && xlock.box && picktyp == xlock.picktyp) {
/*JP	    You("resume your attempt to force the lock.");*/
	    pline("鍵をこじあけるのを再開した．");
/*JP	    set_occupation(forcelock, "forcing the lock", 0);*/
	    set_occupation(forcelock, "鍵をこじあける", 0);
	    return(1);
	}

	/* A lock is made only for the honest man, the thief will break it. */
	xlock.box = (struct obj *)0;
	for(otmp = level.objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere)
	    if(Is_box(otmp)) {
		if (otmp->obroken || !otmp->olocked) {
/*JP		    pline("There is %s here, but its lock is already %s.",*/
		    pline("ここには%sがある，しかしその鍵はもう%s．",
			  doname(otmp), otmp->obroken ? "壊れている" : "はずされている");
		    continue;
		}
/*JP		Sprintf(qbuf,"There is %s here, force its lock?", doname(otmp));*/
		Sprintf(qbuf,"ここには%sがある，鍵をこじあけますか？", doname(otmp));

		c = ynq(qbuf);
		if(c == 'q') return(0);
		if(c == 'n') continue;

		if(picktyp)
/*JP		    You("force your %s into a crack and pry.", xname(uwep));*/
		    You("%sを鍵穴に入れてカチャカチャした．",xname(uwep));
		else
/*JP		    You("start bashing it with your %s.", xname(uwep));*/
		    pline("%sで殴りつけた．", xname(uwep));
		xlock.box = otmp;
		xlock.chance = objects[otmp->otyp].oc_wldam * 2;
		xlock.picktyp = picktyp;
		xlock.usedtime = 0;
		break;
	    }

/*JP	if(xlock.box)	set_occupation(forcelock, "forcing the lock", 0);
	else		You("decide not to force the issue.");*/
	if(xlock.box)	set_occupation(forcelock, "鍵をこじあける", 0);
	else		pline("それは無意味な行為だ．");
	return(1);
}

int
doopen()		/* try to open a door */
{
	register int x, y;
	register struct rm *door;
	struct monst *mtmp;

	if (u.utrap && u.utraptype == TT_PIT) {
/*JP	    You_cant("reach over the edge of the pit.");*/
	    pline("落し穴の中から届かない．");
	    return 0;
	}
#ifdef NEWBIE
	newbie.open = 1;
#endif

	if(!getdir((char *)0)) return(0);

	x = u.ux + u.dx;
	y = u.uy + u.dy;
	if((x == u.ux) && (y == u.uy)) return(0);

	if ((mtmp = m_at(x,y))				&&
		mtmp->m_ap_type == M_AP_FURNITURE	&&
		(mtmp->mappearance == S_hcdoor ||
			mtmp->mappearance == S_vcdoor)	&&
		!Protection_from_shape_changers)	 {

	    stumble_onto_mimic(mtmp);
	    return(1);
	}

	door = &levl[x][y];

	if(!IS_DOOR(door->typ)) {
		if (is_db_wall(x,y)) {
/*JP
		    pline("There is no obvious way to open the drawbridge.");
*/
		    pline("自明な方法じゃ跳ね橋は降りない．");
		    return(0);
		}
/*JP
		You("%s no door there.",
				Blind ? "feel" : "see");
*/
		pline("そこには扉はない%s．",
				Blind ? "ようだ" : "ように見える");
		return(0);
	}

	if (!(door->doormask & D_CLOSED)) {
/*JP
	    const char *mesg;


	    switch (door->doormask) {
	    case D_BROKEN: mesg = " is broken"; break;
	    case D_NODOOR: mesg = "way has no door"; break;
	    case D_ISOPEN: mesg = " is already open"; break;
	    default:	   mesg = " is locked"; break;
	    }
	    pline("This door%s.", mesg);
*/
	    switch(door->doormask) {
		case D_BROKEN:
		  pline("扉は壊れている．"); 
		  break;
		case D_NODOOR:
		  pline("出入口には扉がない．");
		  break;
		case D_ISOPEN:
		  pline("扉はもう開いている．");
		  break;
		default:
		  pline("扉には鍵が掛かっている"); 
#ifdef NEWBIE 
		  ++newbie.try_open;
		  if(newbie.try_open == 10){
		       pline("ヒント: 鍵を開けるのに使えそうな物を探そう。'a'で使える．");
		  }
		  if(newbie.try_open == 30){
		       pline("ヒント: 最後の手段として'C-d'で蹴りやぶれ！");
		  }
#endif
		  break;
	    }
	    if (Blind) feel_location(x,y);
	    return(0);
	}

	if(verysmall(uasmon)) {
/*JP
	    pline("You're too small to pull the door open.");
*/
	    You("小さすぎて扉を開けられない．");
	    return(0);
	}

	/* door is known to be CLOSED */
	if (rnl(20) < (ACURRSTR+ACURR(A_DEX)+ACURR(A_CON))/3) {
/*JP
	    pline_The("door opens.");
*/
	    pline("扉は開いた．");
	    if(door->doormask & D_TRAPPED) {
/*JP
		b_trapped("door", FINGER);
*/
		b_trapped("扉", FINGER);
		door->doormask = D_NODOOR;
		if (*in_rooms(x, y, SHOPBASE)) add_damage(x, y, 0L);
	    } else
		door->doormask = D_ISOPEN;
	    if (Blind)
		feel_location(x,y);	/* the hero knows she opened it  */
	    else
		newsym(x,y);
	    unblock_point(x,y);		/* vision: new see through there */
	} else {
	    exercise(A_STR, TRUE);
/*JP	    pline_The("door resists!");*/
	    pline("なかなか開かない！");
	}

	return(1);
}

static
boolean
obstructed(x,y)
register int x, y;
{
	register struct monst *mtmp = m_at(x, y);

	if(mtmp && mtmp->m_ap_type != M_AP_FURNITURE) {
		if (mtmp->m_ap_type == M_AP_OBJECT) goto objhere;
/*JP		pline("%s stands in the way!", Blind ?
			"Some creature" : Monnam(mtmp));*/
		pline("%sが立ちふさがっている．", Blind ?
			"何者か" : Monnam(mtmp));
		return(TRUE);
	}
	if (OBJ_AT(x, y)) {
/*JPobjhere:	pline("%s's in the way.", Something);*/
objhere:	pline("何か出入口にある．");
		return(TRUE);
	}
	return(FALSE);
}

int
doclose()		/* try to close a door */
{
	register int x, y;
	register struct rm *door;
	struct monst *mtmp;

	if (u.utrap && u.utraptype == TT_PIT) {
/*JP	    You_cant("reach over the edge of the pit.");*/
	    pline("落し穴の中から届かない．");
	    return 0;
	}

	if(!getdir((char *)0)) return(0);

	x = u.ux + u.dx;
	y = u.uy + u.dy;
	if((x == u.ux) && (y == u.uy)) {
/*JP		You("are in the way!");*/
		pline("あなたが出入口にいるので閉まらない．");
		return(1);
	}

	if ((mtmp = m_at(x,y))				&&
		mtmp->m_ap_type == M_AP_FURNITURE	&&
		(mtmp->mappearance == S_hcdoor ||
			mtmp->mappearance == S_vcdoor)	&&
		!Protection_from_shape_changers)	 {

	    stumble_onto_mimic(mtmp);
	    return(1);
	}

	door = &levl[x][y];

	if(!IS_DOOR(door->typ)) {
		if (door->typ == DRAWBRIDGE_DOWN)
/*JP		    pline("There is no obvious way to close the drawbridge.");*/
		    pline("自明な方法じゃ跳ね橋は上がらい．");
		else
/*JP		    You("%s no door there.",
				Blind ? "feel" : "see");*/
		    pline("そこに扉はない%s．",
				Blind ? "ようだ" : "ように見える");
		return(0);
	}

	if(door->doormask == D_NODOOR) {
/*JP	    pline("This doorway has no door.");*/
	    pline("出入口には扉がない．");
	    return(0);
	}

	if(obstructed(x, y)) return(0);

	if(door->doormask == D_BROKEN) {
/*JP	    pline("This door is broken.");*/
	    pline("扉は壊れている．");
	    return(0);
	}

	if(door->doormask & (D_CLOSED | D_LOCKED)) {
/*JP	    pline("This door is already closed.");*/
	    pline("扉はもう閉じている．");
	    return(0);
	}

	if(door->doormask == D_ISOPEN) {
	    if(verysmall(uasmon)) {
/*JP		 pline("You're too small to push the door closed.");*/
		 You("小さすぎて扉を閉められない．");
		 return(0);
	    }
	    if (rn2(25) < (ACURRSTR+ACURR(A_DEX)+ACURR(A_CON))/3) {
/*JP		pline_The("door closes.");*/
		pline("扉は閉じた．");
		door->doormask = D_CLOSED;
		if (Blind)
		    feel_location(x,y);	/* the hero knows she closed it */
		else
		    newsym(x,y);
		block_point(x,y);	/* vision:  no longer see there */
	    }
	    else {
	        exercise(A_STR, TRUE);
/*JP	        pline_The("door resists!");*/
	        pline("なかなか閉まらない！");
	    }
	}

	return(1);
}

boolean			/* box obj was hit with spell effect otmp */
boxlock(obj, otmp)	/* returns true if something happened */
register struct obj *obj, *otmp;	/* obj *is* a box */
{
	register boolean res = 0;

	switch(otmp->otyp) {
	case WAN_LOCKING:
	case SPE_WIZARD_LOCK:
	    if (!obj->olocked) {	/* lock it; fix if broken */
/*JP		pline("Klunk!");*/
		pline("カチ！");
		obj->olocked = 1;
		obj->obroken = 0;
		res = 1;
	    } /* else already closed and locked */
	    break;
	case WAN_OPENING:
	case SPE_KNOCK:
	    if (obj->olocked) {		/* unlock; couldn't be broken */
/*JP		pline("Klick!");*/
		pline("コンコン！");
		obj->olocked = 0;
		res = 1;
	    } else			/* silently fix if broken */
		obj->obroken = 0;
	    break;
	case WAN_POLYMORPH:
	case SPE_POLYMORPH:
	    /* maybe start unlocking chest, get interrupted, then zap it;
	       we must avoid any attempt to resume unlocking it */
	    if (xlock.box == obj)
		reset_pick();
	    break;
	}
	return res;
}

boolean			/* Door/secret door was hit with spell effect otmp */
doorlock(otmp,x,y)	/* returns true if something happened */
struct obj *otmp;
int x, y;
{
	register struct rm *door = &levl[x][y];
	boolean res = TRUE;
	int loudness = 0;
	const char *msg = (const char *)0;
/*JP	const char *dustcloud = "A cloud of dust";
	const char *quickly_dissipates = "quickly dissipates";*/
	const char *dustcloud = "ほこり";
	const char *quickly_dissipates = "あっと言うまに飛び散った";
	
	if (door->typ == SDOOR) {
	    switch (otmp->otyp) {
	    case WAN_OPENING:
	    case SPE_KNOCK:
	    case WAN_STRIKING:
	    case SPE_FORCE_BOLT:
		door->typ = DOOR;
		door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
		newsym(x,y);
/*JP		if (cansee(x,y)) pline("A door appears in the wall!");*/
		if (cansee(x,y)) pline("壁の一部が開いた！");
		if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK)
		    return TRUE;
		break;		/* striking: continue door handling below */
	    case WAN_LOCKING:
	    case SPE_WIZARD_LOCK:
	    default:
		return FALSE;
	    }
	}

	switch(otmp->otyp) {
	case WAN_LOCKING:
	case SPE_WIZARD_LOCK:
#ifdef REINCARNATION
	    if (Is_rogue_level(&u.uz)) {
		/* Can't have real locking in Rogue, so just hide doorway */
/*JP		pline("%s springs up in the older, more primitive doorway.",*/
		pline("古くさい，原始的な出入口に%sが立ちこめた．",
			dustcloud);
		if (obstructed(x,y)) {
/*JP			pline_The("cloud %s.",quickly_dissipates);*/
			pline("ほこりは%s．",quickly_dissipates);
			return FALSE;
		}
		block_point(x, y);
		door->typ = SDOOR;
/*JP		if (cansee(x,y)) pline_The("doorway vanishes!");*/
		if (cansee(x,y)) pline("出入口は消えた！");
		newsym(x,y);
		return TRUE;
	    }
#endif
	    if (obstructed(x,y)) return FALSE;
	    /* Don't allow doors to close over traps.  This is for pits */
	    /* & trap doors, but is it ever OK for anything else? */
	    if (t_at(x,y)) {
		/* maketrap() clears doormask, so it should be NODOOR */
		pline(
/*JP		"%s springs up in the doorway, but %s.",*/
		"%sが出入口に立ちこめた，しかし%s",
		dustcloud, quickly_dissipates);
		return FALSE;
	    }

	    switch (door->doormask & ~D_TRAPPED) {
	    case D_CLOSED:
/*JP		msg = "The door locks!";*/
	        msg = "扉に鍵がかかった！";
		break;
	    case D_ISOPEN:
/*JP		msg = "The door swings shut, and locks!";*/
		msg = "扉は勢いよく閉まり，鍵がかかった！";
		break;
	    case D_BROKEN:
/*JP		msg = "The broken door reassembles and locks!";*/
		msg = "壊れた扉が集まって，鍵がかかった！";
		break;
	    case D_NODOOR:
		msg =
/*JP		"A cloud of dust springs up and assembles itself into a door!";*/
		"ほこりがたちこめ，集まって扉になった！";
		break;
	    default:
		res = FALSE;
		break;
	    }
	    block_point(x, y);
	    door->doormask = D_LOCKED | (door->doormask & D_TRAPPED);
	    newsym(x,y);
	    break;
	case WAN_OPENING:
	case SPE_KNOCK:
	    if (door->doormask & D_LOCKED) {
/*JP		msg = "The door unlocks!";*/
		msg = "扉の鍵ははずれた！";
		door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
	    } else res = FALSE;
	    break;
	case WAN_STRIKING:
	case SPE_FORCE_BOLT:
	    if (door->doormask & (D_LOCKED | D_CLOSED)) {
		if (door->doormask & D_TRAPPED) {
		    if (MON_AT(x, y))
			(void) mb_trapped(m_at(x,y));
		    else if (flags.verbose) {
			if (cansee(x,y))
/*JP			    pline("KABOOM!!  You see a door explode.");*/
			    pline("ちゅどーん！あなたは扉が爆発したのを見た．");
			else if (flags.soundok)
/*JP			    You_hear("a distant explosion.");*/
			    You_hear("遠くの爆発音を聞いた．");
		    }
		    door->doormask = D_NODOOR;
		    unblock_point(x,y);
		    newsym(x,y);
		    loudness = 40;
		    break;
		}
		door->doormask = D_BROKEN;
		if (flags.verbose) {
		    if (cansee(x,y))
/*JP			pline_The("door crashes open!");*/
			pline("扉は壊れ開いた！");
		    else if (flags.soundok)
/*JP			You_hear("a crashing sound.");*/
			You_hear("何かが壊れる音を聞いた．");
		}
		unblock_point(x,y);
		newsym(x,y);
		loudness = 20;
	    } else res = FALSE;
	    break;
	default: impossible("magic (%d) attempted on door.", otmp->otyp);
	    break;
	}
	if (msg && cansee(x,y)) pline(msg);
	if (loudness > 0) {
	    /* door was destroyed */
	    wake_nearto(x, y, loudness);
	    if (*in_rooms(x, y, SHOPBASE)) add_damage(x, y, 0L);
	}

	if (res && picking_at(x, y)) {
	    /* maybe unseen monster zaps door you're unlocking */
	    stop_occupation();
	    reset_pick();
	}
	return res;
}

static void
chest_shatter_msg(otmp)
struct obj *otmp;
{
/*JP	const char *disposition, *article = (otmp->quan > 1L) ? "A" : "The";*/
	const char *disposition;
	const char *thing;
	long save_Blinded;

	if (otmp->oclass == POTION_CLASS) {
/*JP		You("%s a flask shatter!", Blind ? "hear" : "see");*/
		You("薬瓶が割れる%s！", Blind ? "音を聞いた" : "のを見た");
		potionbreathe(otmp);
		return;
	}
	/* We have functions for distant and singular names, but not one */
	/* which does _both_... */
	save_Blinded = Blinded;
	Blinded = 1;
	thing = singular(otmp, xname);
	Blinded = save_Blinded;
	switch (objects[otmp->otyp].oc_material) {
/*JP	case PAPER:	disposition = "is torn to shreds";*/
	case PAPER:	disposition = "は寸断された";
		break;
/*JP	case WAX:	disposition = "is crushed";*/
	case WAX:	disposition = "を床にぶちまけた";
		break;
/*JP	case VEGGY:	disposition = "is pulped";*/
	case VEGGY:	disposition = "はどろどろになった";
		break;
/*JP	case FLESH:	disposition = "is mashed";*/
	case FLESH:	disposition = "はどろどろになった";
		break;
/*JP	case GLASS:	disposition = "shatters";*/
	case GLASS:	disposition = "は割れた";
		break;
/*JP	case WOOD:	disposition = "splinters to fragments";*/
	case WOOD:	disposition = "はかけらになった";
		break;
/*JP	default:	disposition = "is destroyed";*/
	default:	disposition = "は壊れた";
		break;
	}
/*JP	pline("%s %s %s!", article, thing, disposition);*/
	pline("%s%s！", thing, disposition);
}

#endif /* OVLB */

/*lock.c*/
