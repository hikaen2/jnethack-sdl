/*
**
**	$Id: jconj.c,v 1.1.3.1 1999/11/17 06:35:05 issei Exp issei $
**
*/

/* Copyright (c) Issei Numata 1994-2000 */
/* JNetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <ctype.h>
#include "hack.h"
#include "mbchar.h"

/*
**      The kcode constants and the IC macro that used to sit here went
**      unused once jconjsub() stopped taking characters apart by hand.
**      They are declared in japanese/jlib.c, which is where the encoding
**      is actually decided.
*/

#define J_A	0
#define J_KA	(1*5)
#define J_SA	(2*5)
#define J_TA	(3*5)
#define J_NA	(4*5)
#define J_HA	(5*5)
#define J_MA	(6*5)
#define J_YA	(7*5)
#define J_RA	(8*5)
#define J_WA	(9*5)

#define J_GA	(10*5)
#define J_ZA	(11*5)
#define J_DA	(12*5)
#define J_BA	(13*5)
#define J_PA	(14*5)

/*
**      The five columns of each hiragana row: MIZEN, RENYO, SHUUSHI, KATEI,
**      MEIREI.  jconjsub() indexes this as tab->column + n.
**
**      Strings rather than the byte pairs this used to hold.  The old table
**      was {0xa4, 0xa2} and the code assigned p[1] alone, keeping the 0xa4 --
**      which is EUC-JP's lead byte for hiragana and nothing else's.  As
**      literals they convert with the rest of the tree in Phase 3 of
**      UTF8-PLAN.md and no arithmetic here has to know how wide they are.
*/
static const char *const hira_tab[]={
  "あ", "い", "う", "え", "お",
  "か", "き", "く", "け", "こ",
  "さ", "し", "す", "せ", "そ",
  "た", "ち", "つ", "て", "と",
  "な", "に", "ぬ", "ね", "の",
  "は", "ひ", "ふ", "へ", "ほ",
  "ま", "み", "む", "め", "も",
  "や", "い", "ゆ", "え", "よ",
  "ら", "り", "る", "れ", "ろ",
  "わ", "い", "う", "え", "お",
  "が", "ぎ", "ぐ", "げ", "ご",
  "ざ", "じ", "ず", "ぜ", "ぞ",
  "だ", "ぢ", "づ", "で", "ど",
  "ば", "び", "ぶ", "べ", "ぼ",
  "ぱ", "ぴ", "ぷ", "ぺ", "ぽ",
};

#define FIFTH	0
#define UPPER	1
#define LOWER	2
#define SAHEN	3
#define KAHEN	4
#define NAHEN	5

/*
**      Onbin (euphonic change) applied before the TA/TE suffixes.
**
**      Note that SOKUON and HATSUON are swapped with respect to what they
**      produce: the SOKUON branch writes N and the HATSUON branch writes the
**      small TSU, which is the other way round from the terms' meanings.
**      The names are left alone because they are only ever compared against
**      the table above, and renaming them would silently reclassify all
**      sixty-four verbs if one entry were missed.  The output is right --
**      YOMU gives YONDA, UTSU gives UTTA -- which is what the golden file in
**      test/jconj.golden pins down.
*/
#define NORMAL	0
#define SOKUON	1
#define HATSUON	2	
#define ION	3
	
struct _jconj_tab {
  const char *main;
  int column;
/* 0: fifth conj. 1:upper conj. 2:lower conj. 3:SAHEN 4:KAHEN */
  int katsuyo_type;
/* 0: normal 1: sokuon 2: hatson 3: ion */
  int onbin_type;
} jconj_tab[] = {
  {"来る", J_KA, KAHEN, NORMAL}, 
  {"する", J_SA, SAHEN, NORMAL}, 
  {"食べる", J_HA, LOWER, NORMAL}, 
  {"読む", J_MA, FIFTH, SOKUON},
  {"脱ぐ", J_GA, FIFTH, ION},
  {"着る", J_KA, UPPER, NORMAL},
  {"身につける", J_KA, LOWER, NORMAL},
  {"はずす", J_SA, FIFTH, NORMAL},
  {"外す", J_SA, FIFTH, NORMAL},
  {"捧げる", J_KA, LOWER, NORMAL},
  {"書く", J_KA, FIFTH, ION},
  {"こする", J_RA, FIFTH, HATSUON},
  {"投げる", J_GA, LOWER, NORMAL},
  {"落す", J_SA, FIFTH, NORMAL},
  {"置く", J_KA, FIFTH, ION},
  {"殺す", J_SA, FIFTH, NORMAL},
  {"死ぬ", J_NA, FIFTH, SOKUON},
  {"落ちる", J_TA, UPPER, NORMAL},
  {"入れる", J_RA, LOWER, NORMAL},
  {"いれる", J_RA, LOWER, NORMAL},
  {"出す", J_SA, FIFTH, NORMAL},
  {"拾う", J_WA, FIFTH, HATSUON},
  {"飲む", J_MA, FIFTH, SOKUON},
  {"錆びる", J_BA, UPPER, NORMAL},
  {"濡らす", J_SA, FIFTH, NORMAL},
  {"浸す", J_SA, FIFTH, NORMAL},
  {"使う", J_WA, FIFTH, HATSUON},
  {"打つ", J_TA, FIFTH, HATSUON},
  {"浮く", J_KA, FIFTH, ION},
  {"飛ぶ", J_BA, FIFTH, SOKUON},
  {"滑る", J_RA, FIFTH, HATSUON},
  {"出る", J_NA, LOWER, NORMAL},
  {"はいずる", J_RA, FIFTH, HATSUON},
  {"踏む", J_MA, FIFTH, SOKUON},
  {"つまずく", J_KA, FIFTH, ION},
  {"かける", J_KA, UPPER, NORMAL},
  {"あける", J_KA, LOWER, NORMAL},
  {"開ける", J_KA, LOWER, NORMAL},
  {"塗る", J_RA, FIFTH, HATSUON},
  {"加える", J_A, LOWER, NORMAL},
  {"刻む", J_MA, FIFTH, SOKUON},
  {"こます", J_SA, FIFTH, NORMAL},
  {"名づける", J_KA, LOWER, NORMAL},
  {"呼ぶ", J_BA, FIFTH, SOKUON},
  {"焼く", J_KA, FIFTH, ION},
  {"つける", J_KA, LOWER, NORMAL},
  {"壊す", J_SA, FIFTH, NORMAL},
  {"はめる", J_MA, UPPER, NORMAL},
  {"かぶる", J_RA, FIFTH, HATSUON},
  {"構える", J_A, LOWER, NORMAL},
  {"納める", J_MA, LOWER, NORMAL},
  {"取る", J_RA, FIFTH, HATSUON},
  {"守る", J_RA, FIFTH, HATSUON},
  {"解く", J_KA, FIFTH, ION},
  {"込む", J_MA, FIFTH, SOKUON},
  {"とばす", J_SA, FIFTH, NORMAL},
  {"回す", J_SA, FIFTH, NORMAL},
  {"握る", J_RA, FIFTH, HATSUON},
  {"ひっかける", J_KA, LOWER, NORMAL},
  {"はさむ", J_MA, FIFTH, SOKUON},
  {"持つ", J_TA, FIFTH, HATSUON},
  {"巻く", J_KA, FIFTH, ION},
  {"履く", J_KA, FIFTH, ION},
  {"噛みつく", J_KA, FIFTH, ION},
  {(void*)0, 0, 0, 0},
};


/*
**      The last character of s, and the one n characters from the end.
**
**      These replace "tmp + (len - 2)" and "tmp + (len - 4)".  Two bytes was
**      one character only while the encoding was EUC-JP; mb_prev() asks the
**      question the code actually meant.  See include/mbchar.h.
*/
static char *
jbackup( s, n )
     char *s;
     int n;
{
  char *p = s + strlen(s);

  while( n-- > 0 && p > s )
    p = (char *)mb_prev(s, p);

  return p;
}

/*
**      Replace the character at p with the string c, keeping whatever
**      followed p.  Returns the end of what was written, so the caller can
**      append there without knowing how wide c was.
*/
static char *
jputchar_at( p, c )
     char *p;
     const char *c;
{
  int n = strlen(c);

  strcpy(p, c);
  return p + n;
}

/*
**      Voice the first character of the suffix: TA -> DA, TE -> DE.
**
**      This was "++p[3]", which incremented the low byte of the suffix's
**      first character.  It worked because EUC-JP puts TA at A4BF and DA at
**      A4C0, adjacent -- an accident of that encoding that UTF-8 does not
**      repeat.  Only TA and TE ever reach here: the branch is guarded by the
**      suffix starting with one of them.
*/
static void
jdakuten( p )
     char *p;
{
  static const char *const from[] = { "た", "て", 0 };
  static const char *const to[]   = { "だ", "で" };
  int i;

  for( i=0 ; from[i] ; ++i )
    if(!strncmp(p, from[i], strlen(from[i]))){
      /* In place, and only this character: "たら" must come back as
         "だら" and not lose its tail.  Voiced and unvoiced kana are the
         same width in either encoding, so the copy cannot shift what follows. */
      memcpy(p, to[i], strlen(to[i]));
      return;
    }
}

/*      Does the suffix begin with the literal c?       */
#define SFX_IS(sfx,c)   (!strncmp((sfx), (c), sizeof(c)-1))

/*
**	conjection verb word
**
**	Example
**	arg1	arg2	result
**	脱ぐ	ない	脱がない
**	脱ぐ	た	脱いだ
**
*/
static char *
jconjsub( tab, jverb, sfx )
     struct _jconj_tab *tab;
     const char *jverb;
     const char *sfx;
{
  char *p, *q;
  static char tmp[1024];

  strcpy(tmp, jverb);

  if(SFX_IS(sfx, "と")){
    strcat(tmp, sfx);
    return tmp;
  }

  switch( tab->katsuyo_type ){
  case FIFTH:
    p = jbackup(tmp, 1);
    if(SFX_IS(sfx, "な")){
      q = jputchar_at(p, hira_tab[tab->column]);
      strcpy(q, sfx);
      break;
    }
    else if(SFX_IS(sfx, "た") || SFX_IS(sfx, "て")){
      switch( tab->onbin_type ){
      case NORMAL:
	q = jputchar_at(p, hira_tab[tab->column+1]);
	break;
      case SOKUON:
        q = jputchar_at(p, "ん");
	break;
      case HATSUON:
        q = jputchar_at(p, "っ");
	break;
      case ION:
        q = jputchar_at(p, "い");
        break;
      default:
        q = p;
	break;
      }
      strcpy(q, sfx);
      if(tab->onbin_type==SOKUON || (tab->onbin_type==ION && tab->column>=J_GA))
        jdakuten(q);
      break;
    }
    else if(SFX_IS(sfx, "ば")){
      q = jputchar_at(p, hira_tab[tab->column+3]);
      strcpy(q, sfx);
    }
    else if(SFX_IS(sfx, "れ")){
      q = jputchar_at(p, hira_tab[tab->column+3]);
      /* "れば" conjugates as the KATEI form plus "ば": drop the leading
         "れ" the caller supplied, because the stem already ends in one. */
      strcpy(q, sfx + strlen("れ"));
    }
    else if(SFX_IS(sfx, "ま")) {
      q = jputchar_at(p, hira_tab[tab->column+1]);
      strcpy(q, sfx);
      break;
    }
    break;
  case LOWER:
  case UPPER:
  case KAHEN:
    p = jbackup(tmp, 1);
    if(SFX_IS(sfx, "ば")){
      q = jputchar_at(p, "れ");
      strcpy(q, sfx);
    }
    else if(SFX_IS(sfx, "れ") && tab->katsuyo_type == LOWER){
      q = jputchar_at(p, "ら");
      strcpy(q, sfx);
    }
    else
      strcpy(p, sfx);
    break;
  case SAHEN:
    p = jbackup(tmp, 2);
    if(SFX_IS(sfx, "な")||SFX_IS(sfx, "ま")||SFX_IS(sfx, "た")||SFX_IS(sfx, "て")){
      q = jputchar_at(p, "し");
      strcpy(q, sfx);
    }
    else if(SFX_IS(sfx, "ば")||SFX_IS(sfx, "れば")){
      strcpy(p, "すれば");
    }
    break;
  }
  return tmp;
}
const char *
jconj( jverb, sfx )
     const char *jverb;
     const char *sfx;
{
  struct _jconj_tab *tab;
  int len;

  len = strlen(jverb);
  for( tab=jconj_tab ; tab->main!=(void*)0 ;++tab )
    if(!strcmp(jverb, tab->main)){
      return jconjsub( tab, jverb, sfx );
    }

  for( tab=jconj_tab ; tab->main!=(void*)0 ;++tab )
    if(len-(int)strlen(tab->main)>0&&!strcmp(jverb+(len-strlen(tab->main)), tab->main))
      return jconjsub( tab, jverb, sfx );

#ifdef JAPANESETEST
  fprintf( stderr, "I don't know such word \"%s\"\n", jverb);
#endif
  return jverb;
}

/*
**      Does jverb end in "する"?  If so, *cut is where that begins.
**
**      This used to be "!strcmp(jverb + len - 4, ...)" with 4 for the two
**      EUC-JP characters, which read before the start of the string for any
**      verb shorter than that.  No table entry is that short, but jconj() is
**      also reached with names the player typed.
*/
static boolean
jsuru( jverb, cut )
     const char *jverb;
     int *cut;
{
  int len = strlen(jverb);
  int n = sizeof("する")-1;

  if( len < n || strcmp(jverb + len - n, "する") )
    return FALSE;

  *cut = len - n;
  return TRUE;
}

const char *
jcan(jverb)
     const char *jverb;
{
  static char tmp[1024];
  int cut;

  if(jsuru(jverb, &cut)){
    memcpy(tmp, jverb, cut);
    strcpy(tmp + cut, "できる");
    return tmp;
  }
  else
    return jconj(jverb, "れる");
}
const char *
jcannot(jverb)
     const char *jverb;
{
  static char tmp[1024];
  int cut;

  if(jsuru(jverb, &cut)){
    memcpy(tmp, jverb, cut);
    strcpy(tmp + cut, "できない");
    return tmp;
  }
  else
    return jconj(jverb, "れない");
}
const char *
jpast(jverb)
     const char *jverb;
{
  return jconj(jverb, "た");
}


/*
**	conjection of adjective word
**
**	Example:
**
**      連体形             連用形
**
**	赤い		-> 赤く		(形容詞)
**	静かだ		-> 静かに	(形容動詞)
**
**      The example lines that used to sit here described verbs -- ぶつ to
**      ぶち and so on -- which is not what the code does and never was.
*/
const char *
jconj_adj( jadj )
     const char *jadj;
{
  char *p;
  static char tmp[1024];

  strcpy(tmp, jadj);
  p = jbackup(tmp, 1);

  if(!strcmp(p, "い"))
    strcpy(p, "く");
  else if(!strcmp(p, "だ")||
	  !strcmp(p, "な")||
	  !strcmp(p, "の"))
    strcpy(p, "に");

  return tmp;
}


#ifdef JAPANESETEST
main()
{
  struct _jconj_tab *tab;

  for( tab=jconj_tab ; tab->main!=(void*)0 ;++tab ){
    printf("%s %s\n", tab->main, jconj(tab->main, "ない"));
    printf("%s %s\n", tab->main, jconj(tab->main, "ます"));
    printf("%s %s\n", tab->main, jconj(tab->main, "た"));
    printf("%s %s\n", tab->main, jconj(tab->main, "れば"));
    printf("%s %s\n", tab->main, jconj(tab->main, "とき"));
    printf("%s %s\n", tab->main, jcan(tab->main));
    printf("%s %s\n", tab->main, jcannot(tab->main));
  }
  printf("%s\n", jconj("徹夜でnethackの翻訳をする", "ない"));
  printf("%s\n", jconj("徹夜でnethackの翻訳をする", "ます"));
  printf("%s\n", jconj("徹夜でnethackの翻訳をする", "た"));
  printf("%s\n", jconj("徹夜でnethackの翻訳をする", "れば"));
  printf("%s\n", jconj("徹夜でnethackの翻訳をする", "とき"));
}
#endif
