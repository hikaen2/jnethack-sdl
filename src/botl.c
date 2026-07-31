/*	SCCS Id: @(#)botl.c	3.2	96/07/15	*/
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

#ifdef OVL0
extern const char *hu_stat[];	/* defined in eat.c */

const char *enc_stat[] = {
/*JP	"",
	"Burdened",
	"Stressed",
	"Strained",
	"Overtaxed",
	"Overloaded"*/
	"",
	"よろめき",
	"圧迫",
	"限界",
	"荷重",
	"超過"
};

static void NDECL(bot1);
static void NDECL(bot2);
#endif /* OVL0 */

/* MAXCO must hold longest uncompressed status line, and must be larger
 * than COLNO
 *
 * longest practical second status line at the moment is
 *	Astral Plane $:12345 HP:700(700) Pw:111(111) AC:-127 Xp:30/123456789
 *	T:123456 Satiated Conf FoodPois Ill Blind Stun Hallu Overloaded
 * -- or somewhat over 130 characters
 */
#if COLNO <= 140
#define MAXCO 160
#else
#define MAXCO (COLNO+20)
#endif

#ifndef OVLB
STATIC_DCL int mrank_sz;
#else /* OVLB */
STATIC_OVL NEARDATA int mrank_sz = 0; /* loaded by max_rank_sz (from u_init) */
#endif /* OVLB */

struct rank_title {
#ifdef _DCC
    const char *m;		/* male title */
    const char *f;		/* female title, or 0 if same as male */
#else
    char const * const	m;	/* male title */
    char const * const	f;	/* female title, or 0 if same as male */
#endif
};
struct class_ranks {
    char		plclass, fill_;
    short		mplayer_class;
    struct rank_title	titles[9];
};

STATIC_DCL const struct rank_title *FDECL(rank_array, (CHAR_P));
STATIC_DCL const char *NDECL(rank);

#ifdef OVL1

/* 9 pairs of ranks for each class */

static const
struct class_ranks all_classes[] = {
  {					'A',0,	PM_ARCHEOLOGIST, {
/*JP*/
#if 0
	{"Digger",	0},
	{"Field Worker",0},
	{"Investigator",0},
	{"Exhumer",	0},
	{"Excavator",	0},
	{"Spelunker",	0},
	{"Speleologist",0},
	{"Collector",	0},
	{"Curator",	0}
#endif
	{"鉱員",	0},
	{"労働者",      0},
	{"調査者",      0},
	{"発掘者",	0},
	{"掘削者",	0},
	{"探検者",	0},
	{"洞窟学者",    0},
	{"美術収集者",	0},
	{"館長",	0}
  } },
  {					'B',0,	PM_BARBARIAN, {
/*JP*/
#if 0
	{"Plunderer",	"Plunderess"},
	{"Pillager",	0},
	{"Bandit",	0},
	{"Brigand",	0},
	{"Raider",	0},
	{"Reaver",	0},
	{"Slayer",	0},
	{"Chieftain",	"Chieftainess"},
	{"Conqueror",	"Conqueress"}
#endif
	{"盗賊",	"女盗賊"},
	{"略奪者",	0},
	{"悪漢",	0},
	{"山賊",	0},
	{"侵略者",	0},
	{"強盗",	0},
	{"殺戮者",	0},
	{"首領",	"女首領"},
	{"征服者",	0}
  } },
  {					'C',0,	PM_CAVEMAN, {
/*JP*/
#if 0
	{"Troglodyte",	0},
	{"Aborigine",	0},
	{"Wanderer",	0},
	{"Vagrant",	0},
	{"Wayfarer",	0},
	{"Roamer",	0},
	{"Nomad",	0},
	{"Rover",	0},
	{"Pioneer",	0}
#endif
	{"穴居人",	0},
	{"原住民",	0},
	{"放浪者",	0},
	{"浮浪者",	0},
	{"旅行者",	0},
	{"放遊者",	0},
	{"遊牧民",	0},
	{"流浪者",	0},
	{"先駆者",	0}
  } },
  {					'E',0,	PM_ELF, {
/*JP*/
#if 0
	{"Edhel",	"Elleth"},
	{"Edhel",	"Elleth"},	/* elf-maid */
	{"Ohtar",	"Ohtie"},	/* warrior */
	{"Kano",			/* commander (Q.) ['a] */
			"Kanie"}, /* educated guess, until further research- SAC */
	{"Arandur",		  /* king's servant, minister (Q.) - guess */
			"Aranduriel"},	/* educated guess */
	{"Hir",		"Hiril"},	/* lord, lady (S.) ['ir] */
	{"Aredhel",	"Arwen"},	/* noble elf, maiden (S.) */
	{"Ernil",	"Elentariel"},	/* prince (S.), elf-maiden (Q.) */
	{"Elentar",	"Elentari"}	/* Star-king, -queen (Q.) */
#endif
	{"エルフ",	"エレス"},
	{"エルフ",	"エレス"},	/* elf-maid */
	{"エルフの戦士",	"エレスの女戦士"},	/* warrior */
	{"エルフの指揮者",			/* commander (Q.) ['a] */
			"エレスの指揮者"}, /* educated guess, until further research- SAC */
	{"王の従者",		  /* king's servant, minister (Q.) - guess */
			"王の侍女"},	/* educated guess */
	{"エルフの君主",	"エレスの姫"},	/* lord, lady (S.) ['ir] */
	{"高貴なエルフ",	"高貴なエレス"},	/* noble elf, maiden (S.) */
	{"エルフの王子",	"エレスの王女"},	/* prince (S.), elf-maiden (Q.) */
	{"星の王",	"星の妃"}	/* Star-king, -queen (Q.) */
  } },
#ifdef FIGHTER
  {					'F',0,	PM_FIGHTER, {
	{"マーキュリー",	0},
	{"ビーナス",	0},
	{"マーズ",	0},
	{"ジュピター",	0},
	{"サターン",	0},
	{"ウラヌス",	0},
	{"ネプチューン",	0},
	{"プルート",	0},
	{"ムーン",	0}
  } },
#endif
  {					'H',0,	PM_HEALER, {
/*JP*/
#if 0
	{"Rhizotomist",  0},
	{"Empiric",	0},
	{"Embalmer",	0},
	{"Dresser",	0},
	{"Medici ossium",	0},
	{"Herbalist",	0},
	{"Magister",	0},
	{"Physician",	0},
	{"Chirurgeon",	0}
#endif
	{"見習い",      0},
	{"医師見習い",	0},
	{"看護師",	"看護婦"},
	{"医師助手",	0},
	{"薬物主任",	0},
	{"医師主任",	"看護主任"},
	{"漢方医",	0},
	{"内科医",	0},
	{"外科医",	0}
  } },
  {					'K',0,	PM_KNIGHT, {
/*JP*/
#if 0
	{"Gallant",	0},
	{"Esquire",	0},
	{"Bachelor",	0},
	{"Sergeant",	0},
	{"Knight",	0},
	{"Banneret",	0},
	{"Chevalier",	0},
	{"Seignieur",	0},
	{"Paladin",	0}
#endif
	{"見習い",	0},
	{"歩兵",	0},
	{"戦士",	"女戦士"},
	{"騎兵",	0},
	{"重戦士",	0},
	{"騎士",	0},
	{"重騎士",	0},
	{"勲騎士",	0},
	{"聖騎士",	0}
  } },
  {					'P',0,	PM_PRIEST, {
/*JP*/
#if 0
	{"Aspirant",	0},
	{"Acolyte",	0},
	{"Adept",	0},
	{"Priest",	"Priestess"},
	{"Curate",	0},
	{"Canon",	"Canoness"},
	{"Lama",	0},
	{"Patriarch",	"Matriarch"},
	{"High Priest", "High Priestess"}
#endif
	{"修道者",	"修道女"},
	{"侍者",	0},
	{"侍祭",	0},
	{"僧侶",	"尼僧"},
	{"助任司祭",	0},
	{"聖者",	"聖女"},
	{"司教",	0},
	{"大司教",	0},
	{"大僧上",      0}
  } },
  {					'R',0,	PM_ROGUE, {
/*JP*/
#if 0
	{"Footpad",	0},
	{"Cutpurse",	0},
	{"Rogue",	0},
	{"Pilferer",	0},
	{"Robber",	0},
	{"Burglar",	0},
	{"Filcher",	0},
	{"Magsman",	"Magswoman"},
	{"Thief",	0}
#endif
	{"追いはぎ",	0},
	{"ひったくり",	0},
	{"スリ",	0},
	{"ごろつき",	0},
	{"こそどろ",	0},
	{"空巣",	0},
	{"泥棒",	"女泥棒"},
	{"強盗",	0},
	{"大泥棒",	0}
  } },
  {					'S',0,	PM_SAMURAI, {
/*JP*/
#if 0
	{"Hatamoto",	0},  /* Banner Knight */
	{"Ronin",	0},  /* no allegiance */
	{"Ninja",	0},  /* secret society */
	{"Joshu",	0},  /* heads a castle */
	{"Ryoshu",	0},  /* has a territory */
	{"Kokushu",	0},  /* heads a province */
	{"Daimyo",	0},  /* a samurai lord */
	{"Kuge",	0},  /* Noble of the Court */
	{"Shogun",	0}   /* supreme commander, warlord */
#endif
	{"旗本",	0},  /* Banner Knight */
	{"浪人",	0},  /* no allegiance */
	{"忍者",	"くノ一"},  /* secret society */
	{"城主",	0},  /* heads a castle */
	{"領主",	0},  /* has a territory */
	{"国主",	0},  /* heads a province */
	{"大名",	"腰元"},  /* a samurai lord */
	{"公家",	0},  /* Noble of the Court */
	{"将軍",	"大奥"}   /* supreme commander, warlord */
  } },
#ifdef TOURIST
  {					'T',0,	PM_TOURIST, {
/*JP*/
#if 0
	{"Rambler",	0},
	{"Sightseer",	0},
	{"Excursionist",0},
	{"Peregrinator","Peregrinatrix"},
	{"Traveler",	0},
	{"Journeyer",	0},
	{"Voyager",	0},
	{"Explorer",	0},
	{"Adventurer",	0}
#endif
	{"プー太郎",	"プー子"},
	{"観光客",	0},
	{"周遊旅行者",  0},
	{"遍歴者",      0},
	{"旅行者",	0},
	{"旅人",	0},
	{"航海者",	0},
	{"探検家",	0},
	{"冒険者",	0}
  } },
#endif
  {					'V',0,	PM_VALKYRIE, {
#if 0
	{"Stripling",	0},
	{"Skirmisher",	0},
	{"Fighter",	0},
	{"Man-at-arms", "Woman-at-arms"},
	{"Warrior",	0},
	{"Swashbuckler",0},
	{"Hero",	"Heroine"},
	{"Champion",	0},
	{"Lord",	"Lady"}
#endif
	{"見習い",	0},
	{"歩兵",	0},
	{"戦士",	"女戦士"},
	{"騎兵",      "女重騎兵"},
	{"戦闘兵",	0},
	{"攻撃兵",      0},
	{"英雄",	0},
	{"闘士",	"女闘士"},
	{"伯爵",	"女伯爵"}
  } },
  {					'W',0,	PM_WIZARD, {
/*JP*/
#if 0
	{"Evoker",	0},
	{"Conjurer",	0},
	{"Thaumaturge", 0},
	{"Magician",	0},
	{"Enchanter",	"Enchantress"},
	{"Sorcerer",	"Sorceress"},
	{"Necromancer", 0},
	{"Wizard",	0},
	{"Mage",	0}
#endif
	{"手品師",	0},
	{"奇術師",	0},
	{"占い師",	0},
	{"霊感師",	0},
	{"召喚師",	0},
	{"妖術師",      0},
        {"魔術師",      0},
	{"魔法使い",	"魔女"},
	{"大魔法使い",	0}
  } },
};

STATIC_OVL const struct rank_title *
rank_array(pc)
char pc;
{
	register int i;

	for (i = 0; i < SIZE(all_classes); i++)
	    if (all_classes[i].plclass == pc) return all_classes[i].titles;
	return 0;
}

/* convert experience level (1..30) to rank index (0..8) */
int xlev_to_rank(xlev)
int xlev;
{
	return (xlev <= 2) ? 0 : (xlev <= 30) ? ((xlev + 2) / 4) : 8;
}

#if 0	/* not currently needed */
/* convert rank index (0..8) to experience level (1..30) */
int rank_to_xlev(rank)
int rank;
{
	return (rank <= 0) ? 1 : (rank <= 8) ? ((rank * 4) - 2) : 30;
}
#endif

const char *
rank_of(lev, pc, female)
int lev;
char pc;
boolean female;
{
	register int idx = xlev_to_rank((int)lev);
	const struct rank_title *ranks = rank_array(pc);

	if (ranks)
	    return( female && ranks[idx].f ? ranks[idx].f : ranks[idx].m );
	return(pl_character);
}

STATIC_OVL const char *
rank()
{
	return(rank_of(u.ulevel, u.role, flags.female));
}

int
title_to_mon(str, rank_indx, title_length)
const char *str;
int *rank_indx, *title_length;
{
	register int i, j;
	register const struct rank_title *ttl;

	for (i = 0; i < SIZE(all_classes); i++)
	    for (j = 0; j < 9; j++) {
		ttl = &all_classes[i].titles[j];
		if (!strncmpi(ttl->m, str, strlen(ttl->m))) {
		    if (rank_indx) *rank_indx = j;
		    if (title_length) *title_length = strlen(ttl->m);
		    return all_classes[i].mplayer_class;
		} else if (ttl->f && !strncmpi(ttl->f, str, strlen(ttl->f))) {
		    if (rank_indx) *rank_indx = j;
		    if (title_length) *title_length = strlen(ttl->f);
		    return all_classes[i].plclass == 'C' ? PM_CAVEWOMAN :
			   all_classes[i].plclass == 'P' ? PM_PRIESTESS :
			   all_classes[i].mplayer_class;
		}
	    }
	return NON_PM;
}

#endif /* OVL1 */
#ifdef OVLB

void
max_rank_sz()
{
	register int i, r, maxr = 0;
	const struct rank_title *ranks = rank_array(u.role);

	if (ranks) {
	    for (i = 0; i < 9; i++) {
		if ((r = strlen(ranks[i].m)) > maxr) maxr = r;
		if (ranks[i].f)
		    if ((r = strlen(ranks[i].f)) > maxr) maxr = r;
	    }
	    mrank_sz = maxr;
	} else
	    mrank_sz = strlen(pl_character);
}

#endif /* OVLB */
#ifdef OVL0

#ifdef SCORE_ON_BOTL
long
botl_score()
{
    int deepest = deepest_lev_reached(FALSE);
    long ugold = u.ugold + hidden_gold();

    if ((ugold -= u.ugold0) < 0L) ugold = 0L;
    return ugold + u.urexp + (long)(50 * (deepest - 1))
			  + (long)(deepest > 30 ? 10000 :
				   deepest > 20 ? 1000*(deepest - 20) : 0);
}
#endif

static void
bot1()
{
	char newbot1[MAXCO];
	register char *nb;
	register int i,j;

	Strcpy(newbot1, plname);
	if('a' <= newbot1[0] && newbot1[0] <= 'z') newbot1[0] += 'A'-'a';
/*JP
 *      Ten columns of the status line, not ten bytes.  The two were the same
 *      number under EUC-JP, where a kanji is two of each, and stop being so
 *      under UTF-8, where it is three bytes and still two columns.  The old
 *      code cut at ten bytes and blanked byte nine if a wide character
 *      started there, so that no half character was left; mb_trunc_cols()
 *      is that intent stated once.
 *
 *      The field is still padded to ten columns, because everything after it
 *      on the line is positioned by counting.
 */
        {
                int cut = mb_trunc_cols(newbot1, 10);

                if (newbot1[cut]) {     /* only when it did not already fit */
                        int w;

			newbot1[cut] = 0;
                        w = mb_colwidth(newbot1);
                        /*
                         *      The old code blanked byte nine to '_' when a
                         *      wide character started there, which kept the
                         *      field ten columns wide.  A name that fits is
                         *      left alone, then as now -- it is not padded.
                         */
                        while (w < 10 && cut < (int)sizeof newbot1 - 1) {
				newbot1[cut++] = '_';
				newbot1[cut] = 0;
                                w++;
                        }
                }
        }
/*JP
	Sprintf(nb = eos(newbot1)," the ");
*/
	Sprintf(nb = eos(newbot1)," ");

	if (Upolyd) {
		char mbot[BUFSZ];
		int k = 0;

/*JP
		Strcpy(mbot, mons[u.umonnum].mname);
*/
		Strcpy(mbot, jtrns_mon(mons[u.umonnum].mname, flags.female));
		while(mbot[k] != 0) {
		    if ((k == 0 || (k > 0 && mbot[k-1] == ' ')) &&
					'a' <= mbot[k] && mbot[k] <= 'z')
			mbot[k] += 'A' - 'a';
		    k++;
		}
		Sprintf(nb = eos(nb), mbot);
	} else
		Sprintf(nb = eos(nb), rank());

	Sprintf(nb = eos(nb),"  ");
	i = mrank_sz + 15;
	j = (nb + 2) - newbot1; /* aka strlen(newbot1) but less computation */
	if((i - j) > 0)
		Sprintf(nb = eos(nb),"%*s", i-j, " ");	/* pad with spaces */
	if (ACURR(A_STR) > 18) {
		if (ACURR(A_STR) > 118)
/*JP		    Sprintf(nb = eos(nb),"St:%2d ",ACURR(A_STR)-100);*/
		    Sprintf(nb = eos(nb),"強:%2d ",ACURR(A_STR)-100);
		else if (ACURR(A_STR) < 118)
/*JP		    Sprintf(nb = eos(nb), "St:18/%02d ",ACURR(A_STR)-18);*/
		    Sprintf(nb = eos(nb), "強:18/%02d ",ACURR(A_STR)-18);
		else
/*JP		    Sprintf(nb = eos(nb),"St:18/ ** ");*/
		    Sprintf(nb = eos(nb),"強:18/** ");
	} else
/*JP		Sprintf(nb = eos(nb), "St:%-1d ",ACURR(A_STR));*/
		Sprintf(nb = eos(nb), "強:%-1d ",ACURR(A_STR));
	Sprintf(nb = eos(nb),
/*JP		"Dx:%-1d Co:%-1d In:%-1d Wi:%-1d Ch:%-1d",*/
		"早:%-1d 耐:%-1d 知:%-1d 賢:%-1d 魅:%-1d",
		ACURR(A_DEX), ACURR(A_CON), ACURR(A_INT), ACURR(A_WIS), ACURR(A_CHA));
/*JP	Sprintf(nb = eos(nb), (u.ualign.type == A_CHAOTIC) ? "  Chaotic" :
			(u.ualign.type == A_NEUTRAL) ? "  Neutral" : "  Lawful");*/
	Sprintf(nb = eos(nb), (u.ualign.type == A_CHAOTIC) ? "  混沌" :
			(u.ualign.type == A_NEUTRAL) ? "  中立" : "  秩序");
#ifdef SCORE_ON_BOTL
	if (flags.showscore)
/*JP	    Sprintf(nb = eos(nb), " S:%ld", botl_score());*/
	    Sprintf(nb = eos(nb), " 点:%ld", botl_score());
#endif
	curs(WIN_STATUS, 1, 0);
	putstr(WIN_STATUS, 0, newbot1);
}

static void
bot2()
{
	char  newbot2[MAXCO];
	register char *nb;
	int hp, hpmax;
	int cap = near_capacity();

	hp = Upolyd ? u.mh : u.uhp;
	hpmax = Upolyd ? u.mhmax : u.uhpmax;

	if(hp < 0) hp = 0;
/* TODO:	Add in dungeon name */
	if (Is_knox(&u.uz))
/*JP
		Sprintf(newbot2, "%s ", dungeons[u.uz.dnum].dname);
*/
		Sprintf(newbot2, "%s ", jtrns_obj('d', dungeons[u.uz.dnum].dname));
	else if (In_quest(&u.uz))
/*JP
		Sprintf(newbot2, "Home %d ", dunlev(&u.uz));
*/
		Sprintf(newbot2, "故郷 %d ", dunlev(&u.uz));
	else if (In_endgame(&u.uz))
		Sprintf(newbot2,
/*JP
			Is_astralevel(&u.uz) ? "Astral Plane " : "End Game ");
*/
			Is_astralevel(&u.uz) ? "精霊界 " : "最終試練 ");
	else
/*JP
		Sprintf(newbot2, "Dlvl:%-2d ", depth(&u.uz));
*/
		Sprintf(newbot2, "地下:%-2d ", depth(&u.uz));
	Sprintf(nb = eos(newbot2),
/*JP
		"%c:%-2ld HP:%d(%d) Pw:%d(%d) AC:%-2d", oc_syms[GOLD_CLASS],
*/
		"%c:%-2ld 体:%d(%d) 魔:%d(%d) 鎧:%-2d", oc_syms[GOLD_CLASS],
		u.ugold, hp, hpmax, u.uen, u.uenmax, u.uac);

	if (Upolyd)
		Sprintf(nb = eos(nb), " HD:%d", mons[u.umonnum].mlevel);
#ifdef EXP_ON_BOTL
	else if(flags.showexp)
/*JP
		Sprintf(nb = eos(nb), " Xp:%u/%-1ld", u.ulevel,u.uexp);
*/
		Sprintf(nb = eos(nb), " 経験:%u/%-1ld", u.ulevel,u.uexp);
#endif
	else
/*JP
		Sprintf(nb = eos(nb), " Exp:%u", u.ulevel);
*/
		Sprintf(nb = eos(nb), " 経験:%u", u.ulevel);

	if(flags.time)
/*JP
	    Sprintf(nb = eos(nb), " T:%ld", moves);
*/
	    Sprintf(nb = eos(nb), " 歩:%ld", moves);
	if(strcmp(hu_stat[u.uhs], "        ")) {
		Sprintf(nb = eos(nb), " ");
		Strcat(newbot2, hu_stat[u.uhs]);
	}
#if 0 /*JP*/
	if(Confusion)	   Sprintf(nb = eos(nb), " Conf");
	if(Sick) {
		if (u.usick_type & SICK_VOMITABLE)
			   Sprintf(nb = eos(nb), " FoodPois");
		if (u.usick_type & SICK_NONVOMITABLE)
			   Sprintf(nb = eos(nb), " Ill");
	}
	if(Blind)	   Sprintf(nb = eos(nb), " Blind");
	if(Stunned)	   Sprintf(nb = eos(nb), " Stun");
	if(Hallucination)  Sprintf(nb = eos(nb), " Hallu");
#endif
	if(Confusion)	   Sprintf(nb = eos(nb), " 混乱");
	if(Sick) {
		if (u.usick_type & SICK_VOMITABLE)
			   Sprintf(nb = eos(nb), " 食毒");
		if (u.usick_type & SICK_NONVOMITABLE)
			   Sprintf(nb = eos(nb), " 病気");
	}
	if(Blind)	   Sprintf(nb = eos(nb), " 盲目");
	if(Stunned)	   Sprintf(nb = eos(nb), " 眩暈");
	if(Hallucination)  Sprintf(nb = eos(nb), " 幻覚");
#ifdef JPEXTENSION
	if(Totter)	   Sprintf(nb = eos(nb), " 千鳥足");
#endif
	if(cap > UNENCUMBERED)
		Sprintf(nb = eos(nb), " %s", enc_stat[cap]);
	curs(WIN_STATUS, 1, 1);
	putstr(WIN_STATUS, 0, newbot2);
}

void
bot()
{
	bot1();
	bot2();

	flags.botl = flags.botlx = 0;
}

#endif /* OVL0 */

/*botl.c*/
