/*	SCCS Id: @(#)sounds.c	3.2	96/05/04	*/
/*	Copyright (c) 1989 Janet Walz, Mike Threepoint */
/* NetHack may be freely redistributed.  See license for details. */

/*
**	Japanese version Copyright
**	(c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"
#include "edog.h"

#ifdef OVLB

static int FDECL(domonnoise,(struct monst *));
static int NDECL(dochat);

#endif /* OVLB */

#ifdef OVL0

static int FDECL(mon_in_room, (struct monst *,int));

/* this easily could be a macro, but it might overtax dumb compilers */
static int
mon_in_room(mon, rmtyp)
struct monst *mon;
int rmtyp;
{
    int rno = levl[mon->mx][mon->my].roomno;

    return rooms[rno - ROOMOFFSET].rtype == rmtyp;
}

void
dosounds()
{
    register struct mkroom *sroom;
    register int hallu, vx, vy;
#if defined(AMIGA) && defined(AZTEC_C_WORKAROUND)
    int xx;
#endif
    struct monst *mtmp;

    if (!flags.soundok || u.uswallow || Underwater) return;

    hallu = Hallucination ? 1 : 0;

    if (level.flags.nfountains && !rn2(400)) {
	static const char *fountain_msg[4] = {
/*JP
		"bubbling water.",
		"water falling on coins.",
		"the splashing of a naiad.",
		"a soda fountain!",
*/
	        "ゴボゴボという音を聞いた．",
		"ピチャピチャという音を聞いた．",
		"バシャバシャという音を聞いた．",
		"炭酸入り噴水のシューという音を聞いた．"
	};
	You_hear(fountain_msg[rn2(3)+hallu]);
    }
#ifdef SINK
    if (level.flags.nsinks && !rn2(300)) {
	static const char *sink_msg[3] = {
/*JP
		"a slow drip.",
		"a gurgling noise.",
		"dishes being washed!",
*/
	        "水がぽたぽたと落ちる音を聞いた．",
		"がらがらという音を聞いた．",
		"皿を洗う音を聞いた．",
	};
	You_hear(sink_msg[rn2(2)+hallu]);
    }
#endif
    if (level.flags.has_court && !rn2(200)) {
	static const char *throne_msg[4] = {
/*JP
		"the tones of courtly conversation.",
		"a sceptre pounded in judgment.",
		"Someone shouts \"Off with %s head!\"",
		"Queen Beruthiel's cats!",
*/
	        "上品な話し声を聞いた．",
		"裁判で笏を突く音を聞いた．",
		"だれかが「そのものの首を跳ねよ」と叫ぶ声を聞いた．",
		"ベルサイユ女王の猫の声を聞いた．",
	};
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if ((mtmp->msleep ||
			is_lord(mtmp->data) || is_prince(mtmp->data)) &&
		!is_animal(mtmp->data) &&
		mon_in_room(mtmp, COURT)) {
		/* finding one is enough, at least for now */
		int which = rn2(3)+hallu;

		if (which != 2) You_hear(throne_msg[which]);
		else		pline(throne_msg[2], his[flags.female]);
		return;
	    }
    }
    if (level.flags.has_swamp && !rn2(200)) {
	static const char *swamp_msg[3] = {
/*JP		"hear mosquitoes!",
		"smell marsh gas!",	* so it's a smell...*
		"hear Donald Duck!",*/
		"蚊の羽音を聞いた．",
		"腐った匂いがした！",	/* so it's a smell...*/
		"ドナルドダックの声を聞いた！",
	};
	You_hear(swamp_msg[rn2(2)+hallu]);
	return;
    }
    if (level.flags.has_vault && !rn2(200)) {
	if (!(sroom = search_special(VAULT))) {
	    /* strange ... */
	    level.flags.has_vault = 0;
	    return;
	}
	if(gd_sound())
	    switch (rn2(2)+hallu) {
		case 1: {
		    boolean gold_in_vault = FALSE;

		    for (vx = sroom->lx;vx <= sroom->hx; vx++)
			for (vy = sroom->ly; vy <= sroom->hy; vy++)
			    if (g_at(vx, vy))
				gold_in_vault = TRUE;
#if defined(AMIGA) && defined(AZTEC_C_WORKAROUND)
		    /* Bug in aztec assembler here. Workaround below */
		    xx = ROOM_INDEX(sroom) + ROOMOFFSET;
		    xx = (xx != vault_occupied(u.urooms));
		    if(xx)
#else
		    if (vault_occupied(u.urooms) !=
			 (ROOM_INDEX(sroom) + ROOMOFFSET))
#endif /* AZTEC_C_WORKAROUND */
		    {
			if (gold_in_vault)
/*JP			    You_hear(!hallu ? "someone counting money." :
				"the quarterback calling the play.");*/
			    You_hear(!hallu ? "誰かがお金を数えている音を聞いた．" :
				"クォータバックが指示をする声を聞いた．");
			else
/*JP			    You_hear("someone searching.");*/
			    You_hear("誰かが捜索している音を聞いた．");
			break;
		    }
		    /* fall into... (yes, even for hallucination) */
		}
		case 0:
/*JP		    You_hear("the footsteps of a guard on patrol.");*/
		    You_hear("警備員のパトロールする音を聞いた．");
		    break;
		case 2:
/*JP		    You_hear("Ebenezer Scrooge!");*/
		    You_hear("こち亀の両さんの声を聞いた！");
		    break;
	    }
	return;
    }
    if (level.flags.has_beehive && !rn2(200)) {
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if ((mtmp->data->mlet == S_ANT && is_flyer(mtmp->data)) &&
		mon_in_room(mtmp, BEEHIVE)) {
		switch (rn2(2)+hallu) {
		    case 0:
/*JP			You_hear("a low buzzing.");*/
			You_hear("ぶーんという音を聞いた．");
			break;
		    case 1:
/*JP			You_hear("an angry drone.");*/
			You_hear("興奮した雄バチの音を聞いた．");
			break;
		    case 2:
/*JP			You_hear("bees in your %sbonnet!",
			    uarmh ? "" : "(nonexistent) ");*/
			You_hear("ハチがあなたの帽子%sの中にいる音を聞いた！",
			    uarmh ? "" : "(被ってないけど)");
			break;
		}
		return;
	    }
    }
    if (level.flags.has_morgue && !rn2(200)) {
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (is_undead(mtmp->data) &&
		mon_in_room(mtmp, MORGUE)) {
		switch (rn2(2)+hallu) {
		    case 0:
/*JP			You("suddenly realize it is unnaturally quiet.");*/
			You("不自然なくらい静かなのに気づいた．");
			break;
		    case 1:
/*JP			pline_The("%s on the back of your %s stands up.",
				body_part(HAIR), body_part(NECK));*/
			pline("あなたの%sのうしろの%sが逆だった．",
				body_part(NECK), body_part(HAIR));
			break;
		    case 2:
/*JP			pline_The("%s on your %s seems to stand up.",
				body_part(HAIR), body_part(HEAD));*/
			pline("あなたの%sの%sは逆だった．",
				body_part(NECK), body_part(HAIR));
			break;
		}
		return;
	    }
    }
    if (level.flags.has_barracks && !rn2(200)) {
	static const char *barracks_msg[4] = {
/*JP		"blades being honed.",
		"loud snoring.",
		"dice being thrown.",
		"General MacArthur!",*/
	        "刃物を研ぐ音を聞いた．",
		"大きないびきを聞いた．",
		"ダイスが振られる音を聞いた．",
		"マッカーサー提督の声を聞いた．",
	};
	int count = 0;

	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (is_mercenary(mtmp->data) &&
#if 0		/* don't bother excluding these */
		!strstri(mtmp->data->mname, "watch") &&
		!strstri(mtmp->data->mname, "guard") &&
#endif
		mon_in_room(mtmp, BARRACKS) &&
		/* sleeping implies not-yet-disturbed (usually) */
		(mtmp->msleep || ++count > 5)) {
		You_hear(barracks_msg[rn2(3)+hallu]);
		return;
	    }
    }
    if (level.flags.has_zoo && !rn2(200)) {
	static const char *zoo_msg[3] = {
/*JP		"a sound reminiscent of an elephant stepping on a peanut.",
		"a sound reminiscent of a seal barking.",
		"Doctor Doolittle!",*/
	        "象がピーナッツの上で踊るような音を聞いた．",
		"アシカが吠えるような音を聞いた．",
		"ドリトル先生の声を聞いた．",
	};
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if ((mtmp->msleep || is_animal(mtmp->data)) &&
		mon_in_room(mtmp, ZOO)) {
		You_hear(zoo_msg[rn2(2)+hallu]);
		return;
	    }
    }
    if (level.flags.has_shop && !rn2(200)) {
	if (!(sroom = search_special(ANY_SHOP))) {
	    /* strange... */
	    level.flags.has_shop = 0;
	    return;
	}
	if (tended_shop(sroom) &&
		!index(u.ushops, ROOM_INDEX(sroom) + ROOMOFFSET)) {
	    static const char *shop_msg[3] = {
/*JP		    "someone cursing shoplifters.",
		    "the chime of a cash register.",
		    "Neiman and Marcus arguing!",*/
	      "誰かが泥棒をののしる声を聞いた．",
	      "レジのチーンという音を聞いた．",
	      "イトーとヨーカドーの議論を聞いた！",
	    };
	    You_hear(shop_msg[rn2(2)+hallu]);
	}
	return;
    }
    if (Is_oracle_level(&u.uz) && !rn2(400)) {
	/* make sure the Oracle is still here */
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (mtmp->data == &mons[PM_ORACLE])
		break;
	/* and don't produce silly effects when she's clearly visible */
	if (mtmp && (hallu || !canseemon(mtmp))) {
	    static const char *ora_msg[5] = {
#if 0 /*JP*/
		    "a strange wind.",		/* Jupiter at Dodona */
		    "convulsive ravings.",	/* Apollo at Delphi */
		    "snoring snakes.",		/* AEsculapius at Epidaurus */
		    "someone say \"No more woodchucks!\"",
		    "a loud ZOT!"		/* both rec.humor.oracle */
#endif /*JP*/
		"奇妙な風の音を聞いた．",
		"半狂乱の声を聞いた．",
		"蛇の咆哮を聞いた．",
		"誰かが「もうウッドチャックはいらない」と言っている声を聞いた．",
		"大きなＺＯＴを聞いた！"
	    };
	    You_hear(ora_msg[rn2(3)+hallu*2]);
	}
	return;
    }
}

#endif /* OVL0 */
#ifdef OVLB

static const char *h_sounds[] = {
/*JP    "beep", "boing", "sing", "belche", "creak", "cough", "rattle",
    "ululate", "pop", "jingle", "sniffle", "tinkle", "eep"*/
    "ピーッと鳴いた","騒ぎたてた","歌った","キーキーと鳴いた",
    "せき込んだ","ゴロゴロ鳴った","ホーホー鳴いた","ポンと鳴いた",
    "ガランガランと鳴いた","クンクン鳴いた","チリンチリンと鳴いた",
    "イーッと鳴いた"
};

const char *
growl_sound(mtmp)
register struct monst *mtmp;
{
	const char *ret;

	switch (mtmp->data->msound) {
	case MS_MEW:
	case MS_HISS:
/*JP	    ret = "hiss";*/
	    ret = "シーッと鳴いた";
	    break;
	case MS_BARK:
	case MS_GROWL:
/*JP	    ret = "growl";*/
	    ret = "はげしく吠えた";
	    break;
	case MS_ROAR:
/*JP	    ret = "roar";*/
	    ret = "吠えた";
	    break;
	case MS_BUZZ:
/*JP	    ret = "buzz";*/
	    ret = "ブーッと鳴いた";
	    break;
	case MS_SQEEK:
/*JP	    ret = "squeal";*/
	    ret = "キーキー鳴いた";
	    break;
	case MS_SQAWK:
/*JP	    ret = "screech";*/
	    ret = "金切り声を立てた";
	    break;
	case MS_NEIGH:
/*JP	    ret = "neigh";*/
	    ret = "いなないた";
	    break;
	case MS_WAIL:
/*JP	    ret = "wail";*/
	    ret = "悲しく鳴いた";
	    break;
	case MS_SILENT:
/*JP		ret = "commotion";*/
		ret = "興奮した";
		break;
	default:
/*JP		ret = "scream";*/
		ret = "金切り声をあげた";
	}
	return ret;
}

/* the sounds of a seriously abused pet, including player attacking it */
void
growl(mtmp)
register struct monst *mtmp;
{
    register const char *growl_verb = 0;

    if (mtmp->msleep || !mtmp->mcanmove || !mtmp->data->msound) return;

    /* presumably nearness and soundok checks have already been made */
    if (Hallucination)
	growl_verb = h_sounds[rn2(SIZE(h_sounds))];
    else
	growl_verb = growl_sound(mtmp);
    if (growl_verb) {
/*JP	pline("%s %s!", Monnam(mtmp), makeplural(growl_verb));*/
	pline("%sは%s！", Monnam(mtmp), makeplural(growl_verb));
	if(flags.run) nomul(0);
	wake_nearto(mtmp->mx, mtmp->my, mtmp->data->mlevel * 18);
    }
}

/* the sounds of mistreated pets */
void
yelp(mtmp)
register struct monst *mtmp;
{
    register const char *yelp_verb = 0;

    if (mtmp->msleep || !mtmp->mcanmove || !mtmp->data->msound) return;

    /* presumably nearness and soundok checks have already been made */
    if (Hallucination)
	yelp_verb = h_sounds[rn2(SIZE(h_sounds))];
    else switch (mtmp->data->msound) {
	case MS_MEW:
/*JP	    yelp_verb = "yowl";*/
	    yelp_verb = "悲しく鳴いた";
	    break;
	case MS_BARK:
	case MS_GROWL:
/*JP	    yelp_verb = "yelp";*/
	    yelp_verb = "キャンキャン鳴いた";
	    break;
	case MS_ROAR:
/*JP	    yelp_verb = "snarl";*/
	    yelp_verb = "うなった";
	    break;
	case MS_SQEEK:
/*JP	    yelp_verb = "squeal";*/
	    yelp_verb = "キーキー鳴いた";
	    break;
	case MS_SQAWK:
/*JP	    yelp_verb = "screak";*/
	    yelp_verb = "金切り声を立てた";
	    break;
	case MS_WAIL:
/*JP	    yelp_verb = "wail";*/
	    yelp_verb = "悲しく鳴いた";
	    break;
    }
    if (yelp_verb) {
/*JP	pline("%s %ss!", Monnam(mtmp), yelp_verb);*/
	pline("%sは%s！", Monnam(mtmp), yelp_verb);
	if(flags.run) nomul(0);
	wake_nearto(mtmp->mx, mtmp->my, mtmp->data->mlevel * 12);
    }
}

/* the sounds of distressed pets */
void
whimper(mtmp)
register struct monst *mtmp;
{
    register const char *whimper_verb = 0;

    if (mtmp->msleep || !mtmp->mcanmove || !mtmp->data->msound) return;

    /* presumably nearness and soundok checks have already been made */
    if (Hallucination)
	whimper_verb = h_sounds[rn2(SIZE(h_sounds))];
    else switch (mtmp->data->msound) {
	case MS_MEW:
	case MS_GROWL:
/*JP	    whimper_verb = "whimper";*/
	    whimper_verb = "クンクン鳴いた";
	    break;
	case MS_BARK:
/*JP	    whimper_verb = "whine";*/
	    whimper_verb = "クーンと鳴いた";
	    break;
	case MS_SQEEK:
/*JP	    whimper_verb = "squeal";*/
	    whimper_verb = "キーキー鳴いた";
	    break;
    }
    if (whimper_verb) {
/*JP	pline("%s %ss.", Monnam(mtmp), whimper_verb);*/
	pline("%sは%s．", Monnam(mtmp), whimper_verb);
	if(flags.run) nomul(0);
	wake_nearto(mtmp->mx, mtmp->my, mtmp->data->mlevel * 6);
    }
}

/* pet makes "I'm hungry" noises */
void
beg(mtmp)
register struct monst *mtmp;
{
    if (mtmp->msleep || !mtmp->mcanmove ||
	!(carnivorous(mtmp->data) || herbivorous(mtmp->data))) return;
    /* presumably nearness and soundok checks have already been made */
    if (mtmp->data->msound != MS_SILENT && mtmp->data->msound <= MS_ANIMAL)
	(void) domonnoise(mtmp);
    else if (mtmp->data->msound >= MS_HUMANOID)
	verbalize("はらぺこだ．");
}

static int
domonnoise(mtmp)
register struct monst *mtmp;
{
    register const char *pline_msg = 0,	/* Monnam(mtmp) will be prepended */
			*verbl_msg = 0;	/* verbalize() */
    struct permonst *ptr = mtmp->data;

    /* presumably nearness and sleep checks have already been made */
    if (!flags.soundok) return(0);

    switch (ptr->msound) {
	case MS_ORACLE:
	    return doconsult(mtmp);
	case MS_PRIEST:
	    priest_talk(mtmp);
	    break;
	case MS_LEADER:
	case MS_NEMESIS:
	case MS_GUARDIAN:
	    quest_chat(mtmp);
	    break;
	case MS_SELL: /* pitch, pay, total */
	    shk_chat(mtmp);
	    break;
	case MS_VAMPIRE:
	    /* [bug: vampire case missing here] */
	    break;
	case MS_WERE:
	    if (flags.moonphase == FULL_MOON && (night() ^ !rn2(13))) {
/*JP		pline("%s throws back %s head and lets out a blood curdling %s!",
		      Monnam(mtmp), his[pronoun_gender(mtmp)],
		      ptr == &mons[PM_HUMAN_WERERAT] ? "shriek" : "howl");*/
		pline("%sは頭をのけぞらし背筋が氷るような%sをあげた！",
		      Monnam(mtmp),
		      ptr == &mons[PM_HUMAN_WERERAT] ? "金切り声" : "咆哮");
		wake_nearto(mtmp->mx, mtmp->my, 11*11);
	    }else
/*JP		pline_msg =
		     "whispers inaudibly.  All you can make out is \"moon\".";*/
	      pline_msg = "聞きとれないような声でささやいた．かろうじて『月』という言葉だけが聞きとれた．";
	    break;
	case MS_SILENT:
	    break;
	case MS_BARK:
	    if (flags.moonphase == FULL_MOON && night()) {
/*JP		pline_msg = "howls.";*/
		pline_msg = "吠えた．";
	    } else if (mtmp->mpeaceful) {
		if (mtmp->mtame &&
			(mtmp->mconf || mtmp->mflee || mtmp->mtrapped ||
			 moves > EDOG(mtmp)->hungrytime || mtmp->mtame < 5))
/*JP		    pline_msg = "whines.";*/
		    pline_msg = "クンクン鳴いた．";
		else if (mtmp->mtame && EDOG(mtmp)->hungrytime > moves + 1000)
/*JP		    pline_msg = "yips.";*/
		    pline_msg = "キャンキャン鳴いた．";
		else
/*JP		    pline_msg = "barks.";*/
		    pline_msg = "ワンワン吠えた．";
	    } else {
/*JP		pline_msg = "growls.";*/
		pline_msg = "激しく吠えた．";
	    }
	    break;
	case MS_MEW:
	    if (mtmp->mtame) {
		if (mtmp->mconf || mtmp->mflee || mtmp->mtrapped ||
			mtmp->mtame < 5)
/*JP		    pline_msg = "yowls.";*/
		    pline_msg = "悲しく鳴いた．";
		else if (moves > EDOG(mtmp)->hungrytime)
/*JP		    pline_msg = "miaos.";*/
		    pline_msg = "ニャーンと鳴いた．";
		else if (EDOG(mtmp)->hungrytime > moves + 1000)
/*JP		    pline_msg = "purrs.";*/
		    pline_msg = "ゴロゴロと鳴いた．";
		else
/*JP		    pline_msg = "mews.";*/
		    pline_msg = "ニャーニャー鳴いた．";
		break;
	    } /* else FALLTHRU */
	case MS_GROWL:
/*JP	    pline_msg = mtmp->mpeaceful ? "snarls." : "growls!";*/
	    pline_msg = mtmp->mpeaceful ? "うなった．" : "激しく吠えた．";
	    break;
	case MS_ROAR:
/*JP	    pline_msg = mtmp->mpeaceful ? "snarls." : "roars!";*/
	    pline_msg = mtmp->mpeaceful ? "うなった．" : "とても激しく吠えた．";
	    break;
	case MS_SQEEK:
/*JP	    pline_msg = "squeaks.";*/
	    pline_msg = "キーキー鳴いた．";
	    break;
	case MS_SQAWK:
/*JP	    pline_msg = "squawks.";*/
	    pline_msg = "キーキー鳴いた．";
	    break;
	case MS_HISS:
	    if (!mtmp->mpeaceful)
/*JP		pline_msg = "hisses!";*/
		pline_msg = "シーッと鳴いた！";
	    else return 0;	/* no sound */
	    break;
	case MS_BUZZ:
/*JP	    pline_msg = mtmp->mpeaceful ? "drones." : "buzzes angrily.";*/
	    pline_msg = mtmp->mpeaceful ? "ぶーんと鳴った．" : "ぶんぶん鳴った．";
	    break;
	case MS_GRUNT:
/*JP	    pline_msg = "grunts.";*/
	    pline_msg = "ぶーぶー鳴いた．";
	    break;
	case MS_NEIGH:
	    if (mtmp->mtame < 5)
/*JP		pline_msg = "neighs.";*/
	        pline_msg = "いなないた．";
	    else if (moves > EDOG(mtmp)->hungrytime)
/*JP		pline_msg = "whinnies.";*/
		pline_msg = "ヒヒーンと鳴いた．";
	    else
/*JP		pline_msg = "whickers.";*/
		pline_msg = "ヒヒヒーンと鳴いた．";
	    break;
	case MS_WAIL:
/*JP	    pline_msg = "wails mournfully.";*/
	    pline_msg = "悲しげに鳴いた．";
	    break;
	case MS_GURGLE:
/*JP	    pline_msg = "gurgles.";*/
	    pline_msg = "ごろごろ喉を鳴らした．";
	    break;
	case MS_BURBLE:
/*JP	    pline_msg = "burbles.";*/
	    pline_msg = "ぺちゃくちゃしゃべった．";
	    break;
	case MS_SHRIEK:
/*JP	    pline_msg = "shrieks.";*/
	    pline_msg = "金切り声をあげた．";
	    aggravate();
	    break;
	case MS_IMITATE:
/*JP	    pline_msg = "imitates you.";*/
	    pline_msg = "あなたの真似をした．";
	    break;
	case MS_BONES:
/*JP	    pline("%s rattles noisily.", Monnam(mtmp));
	    You("freeze for a moment.");*/
	    pline("%sはガタガタと騒ぎだした．",Monnam(mtmp));
	    You("しばらく動けない．");
	    nomul(-2);
	    break;
	case MS_LAUGH:
	    {
		static const char *laugh_msg[4] = {
/*JP		    "giggles.", "chuckles.", "snickers.", "laughs.",*/
		    "くすくす笑った．", "くすっすと笑った．",
		    "ばかにしたように笑った．", "笑った．",
		};
		pline_msg = laugh_msg[rn2(4)];
	    }
	    break;
	case MS_MUMBLE:
/*JP	    pline_msg = "mumbles incomprehensibly.";*/
	    pline_msg = "不可解な言葉をつぶやいた．";
	    break;
	case MS_DJINNI:
/*JP	    if (mtmp->mtame) verbl_msg = "Thank you for freeing me!";
	    else if (mtmp->mpeaceful) verbl_msg = "I'm free!";
	    else verbl_msg = "This will teach you not to disturb me!";*/
	    if (mtmp->mtame) verbl_msg = "私を助けてくれたことを感謝する！";
	    else if (mtmp->mpeaceful) verbl_msg = "やっと自由になった！";
	    else verbl_msg = "じゃまをしないでくれ！";
	    break;
	case MS_BOAST:	/* giants */
	    if (!mtmp->mpeaceful) {
		switch (rn2(4)) {
/*JP		case 0: pline("%s boasts about %s gem collection.",
			      Monnam(mtmp), his[pronoun_gender(mtmp)]);*/
		case 0: pline("%sは自分の宝石のコレクションを自慢した．",
			      Monnam(mtmp));
			break;
/*JP		case 1: pline_msg = "complains about a diet of mutton.";*/
		case 1: pline_msg = "羊のダイエットについての不満を述べた．";
			break;
/*JP	       default: pline_msg = "shouts \"Fee Fie Foe Foo!\" and guffaws.";*/
	       default: pline_msg = "『わっはっはっは！』とばか笑いした．";
			wake_nearto(mtmp->mx, mtmp->my, 7*7);
			break;
		}
		break;
	    }
	    /* else FALLTHRU */
	case MS_HUMANOID:
	    if (!mtmp->mpeaceful) {
		if (In_endgame(&u.uz) && is_mplayer(ptr)) {
		    mplayer_talk(mtmp);
		    break;
		} else {
		    return 0;	/* no sound */
		}
	    }
	    /* Generic peaceful humanoid behaviour. */
	    if (mtmp->mflee)
/*JP		pline_msg = "wants nothing to do with you.";*/
		pline_msg = "あなたと関係を持ちたくないようだ．";
	    else if (mtmp->mhp < mtmp->mhpmax/4)
/*JP		pline_msg = "moans.";*/
		pline_msg = "うめき声をあげた．";
	    else if (mtmp->mconf || mtmp->mstun)
/*JP		verbl_msg = !rn2(3) ? "Huh?" : rn2(2) ? "What?" : "Eh?";*/
		verbl_msg = !rn2(3) ? "へ？" : rn2(2) ? "何？" : "え？";
	    else if (!mtmp->mcansee)
/*JP		verbl_msg = "I can't see!";*/
		verbl_msg = "何も見えない！";
	    else if (mtmp->mtrapped)
/*JP		verbl_msg = "I'm trapped!";*/
	        verbl_msg = "罠にはまってしまった！";
	    else if (mtmp->mhp < mtmp->mhpmax/2)
/*JP		pline_msg = "asks for a potion of healing.";*/
		pline_msg = "回復の薬を持ってないか尋ねた．";
	    else if (mtmp->mtame && moves > EDOG(mtmp)->hungrytime)
/*JP		verbl_msg = "I'm hungry.";*/
		verbl_msg = "腹が減ったな．";
	    /* Specific monsters' interests */
	    else if (is_elf(ptr))
/*JP		pline_msg = "curses orcs.";*/
		pline_msg = "オークを呪った．";
	    else if (is_dwarf(ptr))
/*JP		pline_msg = "talks about mining.";*/
		pline_msg = "採掘について話した．";
	    else if (likes_magic(ptr))
/*JP		pline_msg = "talks about spellcraft.";*/
		pline_msg = "魔力について話した．";
	    else if (ptr->mlet == S_CENTAUR)
/*JP		pline_msg = "discusses hunting.";*/
		pline_msg = "猟について議論した．";

	    else switch (monsndx(ptr)) {
		case PM_HOBBIT:
		    pline_msg = (mtmp->mhpmax - mtmp->mhp >= 10) ?
/*JP				"complains about unpleasant dungeon conditions."
				: "asks you about the One Ring.";*/
				"不愉快な迷宮の状態について不満を述べた．"
				: "「一つの指輪」について尋ねた．";
		    break;
		case PM_ARCHEOLOGIST:
/*JP    pline_msg = "describes a recent article in \"Spelunker Today\" magazine.";*/
		    pline_msg = "「日刊洞窟」の最新の記事を執筆している．";
		    break;
#ifdef TOURIST
		case PM_TOURIST:
/*JP		    verbl_msg = "Aloha.";*/
		    verbl_msg = "アローハ．";
		    break;
#endif
		default:
/*JP		    pline_msg = "discusses dungeon exploration.";*/
		    pline_msg = "迷宮探検について議論した．";
		    break;
	    }
	    break;
	case MS_SEDUCE:
#ifdef SEDUCE
	    if (ptr->mlet != S_NYMPH &&
		could_seduce(mtmp, &youmonst, (struct attack *)0) == 1) {
			(void) doseduce(mtmp);
			break;
	    }
	    switch ((poly_gender() != (int) mtmp->female) ? rn2(3) : 0) {
#else
/*	    switch ((poly_gender() == 0) ? rn2(3) : 0) {*/
#endif
		case 2:
/*JP			verbl_msg = "Hello, sailor.";*/
			switch(poly_gender()){
			case 0:	      
			  verbl_msg = "こんにちは．あらいい男ね．";
			  break;
			case 1:
			  verbl_msg = "こんにちは，お嬢さん．";
			  break;
			default:
			  verbl_msg = "こんにちは．";
			  break;
			}
			break;
		case 1:
/*JP			pline_msg = "comes on to you.";*/
			pline_msg = "あなたのほうへやってきた．";
			break;
		default:
/*JP			pline_msg = "cajoles you.";*/
			pline_msg = "あなたをおだてた．";
	    }
	    break;
#ifdef KOPS
	case MS_ARREST:
	    if (mtmp->mpeaceful)
/*JP		verbalize("Just the facts, %s.",
		      flags.female ? "Ma'am" : "Sir");*/
		verbalize("これが現実だよ%s．",
		      flags.female ? "お嬢さん" : "きみ");
	    else {
		static const char *arrest_msg[3] = {
/*JP		    "Anything you say can be used against you.",
		    "You're under arrest!",
		    "Stop in the name of the Law!",*/
		    "おまえの言うことはおまえにとって不利な証拠となることがある！",
		    "おまえを逮捕する！",
		    "法の名のもと直ちに中止せよ！",
		};
		verbl_msg = arrest_msg[rn2(3)];
	    }
	    break;
#endif
	case MS_BRIBE:
	    if (mtmp->mpeaceful && !mtmp->mtame) {
		(void) demon_talk(mtmp);
		break;
	    }
	    /* fall through */
	case MS_CUSS:
	    if (!mtmp->mpeaceful)
		cuss(mtmp);
	    break;
	case MS_SPELL:
	    /* deliberately vague, since it's not actually casting any spell */
/*JP	    pline_msg = "seems to mutter a cantrip.";*/
	    pline_msg = "ぶつぶつとつぶやいている．";
	    break;
	case MS_NURSE:
	    if (uwep && (uwep->oclass == WEAPON_CLASS || is_weptool(uwep)))
/*JP		verbl_msg = "Put that weapon away before you hurt someone!";*/
		verbl_msg = "武器をおさめなさい！それは人を傷つけるものよ！";
	    else if (uarmc || uarm || uarmh || uarms || uarmg || uarmf)
		verbl_msg = Role_is('H') ?
/*JP			  "Doc, I can't help you unless you cooperate." :
			  "Please undress so I can examine you.";*/
			  "先生，あなたの協力なしではどうしようもありませんわ．" :
			  "服を脱いでください．あなたを診察しますわ．";
#ifdef TOURIST
	    else if (uarmu)
/*JP		verbl_msg = "Take off your shirt, please.";*/
		verbl_msg = "シャツを脱いでください．";
#endif
/*JP	    else verbl_msg = "Relax, this won't hurt a bit.";*/
	    else verbl_msg = "おちついて．ちっとも痛くないわよ．";
	    break;
	case MS_GUARD:
	    if (u.ugold)
/*JP		verbl_msg = "Please drop that gold and follow me.";*/
 		verbl_msg = "金を置いてついてこい．";
	    else
/*JP		verbl_msg = "Please follow me.";*/
		verbl_msg = "ついてこい．";
	    break;
	case MS_SOLDIER:
	    {
		static const char *soldier_foe_msg[3] = {
/*JP		    "Resistance is useless!",
		    "You're dog meat!",
		    "Surrender!",*/
		    "抵抗はやめろ！",
		    "犬に喰われちまえ！",
		    "降伏しろ！",
		},		  *soldier_pax_msg[3] = {
/*JP		    "What lousy pay we're getting here!",
		    "The food's not fit for Orcs!",
		    "My feet hurt, I've been on them all day!",*/
		    "たっぷり金を置いてゆけ！",
		    "なんだい？オークでも喰えねぇぜ！",
 		    "足を怪我した，どうしてくれる！",
		};
		verbl_msg = mtmp->mpeaceful ? soldier_pax_msg[rn2(3)]
					    : soldier_foe_msg[rn2(3)];
	    }
	    break;
	case MS_RIDER:
	    if (ptr == &mons[PM_DEATH] && mtmp->mpeaceful)
/*JP		pline_msg = "is busy reading a copy of Sandman #9.";*/
		pline_msg = "Sandmanの9章を読むのに忙しい";
/*JP	    else verbl_msg = "Who do you think you are, War?";*/
	    else verbl_msg = "自分が何物か考えたことがあるか？";
	    break;
    }

/*JP    if (pline_msg) pline("%s %s", Monnam(mtmp), pline_msg);*/
    if (pline_msg) pline("%sは%s", Monnam(mtmp), pline_msg);
    else if (verbl_msg) verbalize(verbl_msg);
    return(1);
}


int
dotalk()
{
    int result;
    boolean save_soundok = flags.soundok;
    flags.soundok = 1;	/* always allow sounds while chatting */
    result = dochat();
    flags.soundok = save_soundok;
    return result;
}

static int
dochat()
{
    register struct monst *mtmp;
    register int tx,ty;
    struct obj *otmp;

    if (uasmon->msound == MS_SILENT) {
/*JP	pline("As %s, you cannot speak.", an(uasmon->mname));*/
	pline("あなたは%sなので，話すことができない．", jtrns_mon(uasmon->mname, flags.female));
	return(0);
    }
    if (Strangled) {
/*JP	You_cant("speak.  You're choking!");*/
	You("話せない．あなたは首を絞められている！");
	return(0);
    }
    if (u.uswallow) {
/*JP	pline("They won't hear you out there.");*/
        You("外へ向って話をしたが，誰も聞きいれなかった．");
	return(0);
    }
    if (Underwater) {
/*JP	Your("speech is unintelligible underwater.");*/
	pline("水面下では，あなたの話はろくに理解されない．");
	return(0);
    }

    if (!Blind && (otmp = shop_object(u.ux, u.uy)) != (struct obj *)0) {
	/* standing on something in a shop and chatting causes the shopkeeper
	   to describe the price(s).  This can inhibit other chatting inside
	   a shop, but that shouldn't matter much.  shop_object() returns an
	   object iff inside a shop and the shopkeeper is present and willing
	   (not angry) and able (not asleep) to speak and the position contains
	   any objects other than just gold.
	*/
	price_quote(otmp);
	return(1);
    }

/*JP    (void) getdir("Talk to whom? [in what direction]");*/
    (void) getdir("誰と話しますか？[どの方向]");

    if (u.dz) {
/*JP	pline("They won't hear you %s there.", u.dz < 0 ? "up" : "down");*/
	pline("%s向って話をしても意味がない．",
	      u.dz < 0 ? "上へ" : "下へ");
	return(0);
    }

    if (u.dx == 0 && u.dy == 0) {
/*
 * Let's not include this.  It raises all sorts of questions: can you wear
 * 2 helmets, 2 amulets, 3 pairs of gloves or 6 rings as a marilith,
 * etc...  --KAA
	if (u.umonnum == PM_ETTIN) {
JP	    You("discover that your other head makes boring conversation.");
	    You("他の頭が会話をしているのに気がついた．");
	    return(1);
	}
*/
/*JP	pline("Talking to yourself is a bad habit for a dungeoneer.");*/
	pline("迷宮探検者にとって一人言は悪い癖だ．");
	return(0);
    }

    tx = u.ux+u.dx; ty = u.uy+u.dy;
    mtmp = m_at(tx, ty);
    if ((Blind && !Telepat) || !mtmp || mtmp->mundetected ||
		mtmp->m_ap_type == M_AP_FURNITURE ||
		mtmp->m_ap_type == M_AP_OBJECT) {
/*JP	pline("I see nobody there.");*/
	pline("そこには何もない．");
	return(0);
    }
    /* sleeping monsters won't talk, except priests (who wake up) */
    if ((!mtmp->mcanmove || mtmp->msleep) && !mtmp->ispriest) {
/*JP	pline("%s seems not to notice you.", Monnam(mtmp));*/
	pline("%sはあなたに気がついていないようだ．", Monnam(mtmp));
	return(0);
    }

    if (mtmp->mtame && mtmp->meating) {
/*JP	pline("%s is eating noisily.", Monnam(mtmp));*/
	pline("%sはバリバリと物を食べている．", Monnam(mtmp));
	return (0);
    }

    return domonnoise(mtmp);
}

#endif /* OVLB */

/*sounds.c*/
