/*
**
**	$Id: jlib.c,v 1.1.3.1 1999/11/17 06:35:05 issei Exp issei $
**
*/

/* Copyright (c) Issei Numata 1994-2000 */
/* JNetHack may be freely redistributed.  See license for details. */

#include <stdio.h>
#include <ctype.h>
#include "hack.h"
#include "mbchar.h"

#ifdef SDL_GRAPHICS
/*
** This file's whole job is to put bytes on the screen, so it keeps the
** real stdio and calls the backend by name.  See include/sdlterm.h.
*/
# define SDLTERM_KEEP_STDIO
# include "sdlterm.h"
#endif

#define EUC	0
#define SJIS	1
#define JIS	2

/* internal kcode */
/* IC=0 EUC */
/* IC=1 SJIS */
#define IC ((unsigned char)("漢"[0])==0x8a)

/* default input kcode */
#ifndef INPUT_KCODE
# ifdef MSDOS
#  define INPUT_KCODE SJIS
# else
#  define INPUT_KCODE EUC
# endif
#endif

/* default output kcode */
#ifndef OUTPUT_KCODE
# ifdef MSDOS
#  define OUTPUT_KCODE SJIS
# else
#  define OUTPUT_KCODE EUC
# endif
#endif

static int	output_kcode = OUTPUT_KCODE;
static int	input_kcode = INPUT_KCODE;

/*
**	Kanji code library....
*/

int
is_kanji(c)
     unsigned c;
{
  if(IC == EUC)
    return (c & 0x80);
  else
    return ((unsigned int)c>=0x81 && (unsigned int)c<=0x9f)
      || ((unsigned int)c>=0xe0 && (unsigned int)c<=0xfc);
}

void
setkcode(c)
     int c;
{
#ifdef SDL_GRAPHICS
  /*
  ** The SDL backend does not emit bytes at all; it takes characters and
  ** puts code points in cells.  An output encoding is therefore not a
  ** thing it can have, and the byte pairs the tty_*putc2() hooks below
  ** hand to sdl_puteuc() have to still be in the internal code.  Pinning
  ** output_kcode to IC keeps jbuffer() from converting them on the way.
  */
  output_kcode = input_kcode = IC;
  return;
#else
  if(c == 'E' || c == 'e' )
    output_kcode = EUC;
  else if(c == 'J' || c == 'j')
    output_kcode = JIS;
  else if(c == 'S' || c == 's')
    output_kcode = SJIS;
  else if(c == 'I' || c == 'i')
#ifdef MSDOS
    output_kcode = SJIS;
#else
    output_kcode = IC;
#endif
  else{
    output_kcode = IC;
  }
  input_kcode = output_kcode;
#endif /* SDL_GRAPHICS */
}
/*
**	EUC->SJIS
*/

unsigned char *
e2sj(s)
     unsigned char *s;
{
  unsigned char h,l;
  static unsigned char sw[2];

  h = s[0] & 0x7f;
  l = s[1] & 0x7f;

  sw[0] = ((h - 1) >> 1)+ ((h <= 0x5e) ? 0x71 : 0xb1);
  sw[1] = l + ((h & 1) ? ((l < 0x60) ? 0x1f : 0x20) : 0x7e);

  return sw;
}
/*
**	SJIS->EUC
*/
unsigned char *
sj2e(s)
     unsigned char *s;
{
  unsigned int h,l;
  static unsigned char sw[2];

  h = s[0];
  l = s[1];

  h = h + h - ((h <=0x9f) ? 0x00e1 : 0x0161);
  if( l<0x9f )
    l = l - ((l > 0x7f) ? 0x20 : 0x1f);
  else{
    l = l-0x7e;
    ++h;
  }
  sw[0] = h | 0x80;
  sw[1] = l | 0x80;
  return sw;
}
/*
**	translate string to internal kcode
*/
const char *
str2ic(s)
     const char *s;
{
  static unsigned char buf[1024];
  const unsigned char *up;
  unsigned char *p, *pp;
  int kin;

  if(!s)
    return s;

  buf[0] = '\0';

  if( IC==input_kcode ){
    strcpy((char *)buf, s);
    return (char *)buf;
  }

  p = buf;
  if( IC==EUC && input_kcode == SJIS ){
    while(*s){
      up = s;
      if(is_kanji(*up)){
	pp = sj2e((unsigned char *)s);
	*(p++) = pp[0];
	*(p++) = pp[1];
	s += 2;
      }
      else
	*(p++) = (unsigned char)*(s++);
    }
  }
  else if( IC==EUC && input_kcode == JIS ){
    kin = 0;
    while(*s){
      if(s[0] == 033 && s[1] == '$' && (s[2] == 'B' || s[3] == '@')){
	kin = 1;
	s += 3;
      }
      else if(s[0] == 033 && s[1] == '(' && (s[2] == 'B' || s[3] == 'J')){
	kin = 0;
	s += 3;
      }
      else if( kin )
	*(p++) = (*(s++) | 0x80);
      else
	*(p++) = *(s++);
    }
  }
  else{
    strcpy((char *)buf, s);
    return (char *)buf;
  }

  *(p++) = '\0';
  return (char *)buf;
}

#ifdef MSDOS
/*
**	translate string to output kcode
*/
const char *
ic2str(s)
     const char *s;
{
  static unsigned char buf[1024];
  const unsigned char *up;
  unsigned char *p, *pp;
  int kin;

  if(!s)
    return s;

  buf[0] = '\0';

  p = buf;
  if( IC==EUC && output_kcode == SJIS ){
    while(*s){
      up = s;
      if( *up & 0x80 ){
	pp = e2sj((unsigned char *)s);
	*(p++) = pp[0];
	*(p++) = pp[1];
	s += 2;
      }
      else
	*(p++) = (unsigned char)*(s++);
    }
  }
  else{
    strcpy((char *)buf, s);
    return (char *)buf;
  }

  *(p++) = '\0';
  return (char *)buf;
}
#endif /* MSDOS */

/*
**	primitive function
*/

static int kmode;	/* 0: Kanji out */
			/* 1: Kanji in */

static void
tty_reset()
{
#ifdef SDL_GRAPHICS
  /* No shift state to leave: the grid holds code points, not bytes. */
  kmode = 0;
  return;
#endif
  if(kmode && output_kcode==JIS ){
    putchar(033);
    putchar('(');
    putchar('B');
/*
    if (flags.DECgraphics){
      putchar(033);
      putchar('$');
      putchar(')');
      putchar('B');
    }
*/
  }
  kmode = 0;
}

/* print out 1 byte character to tty (no conversion) */
static void
tty_cputc(unsigned int c)
{
#ifdef SDL_GRAPHICS
  /*
  ** A single byte, which the backend still has to interpret: between
  ** graph_on() and graph_off() it means a line-drawing character rather
  ** than the ASCII of the same value.  sdl_putbyte() is the layer that
  ** knows which graphics set is in effect.
  */
  kmode = 0;
  sdl_putbyte((int)c);
  return;
#endif
  if(kmode && output_kcode==JIS ){
    putchar(033);
    putchar('(');
    putchar('B');
  }
  kmode = 0;

#if defined(NO_TERMS) && defined(MSDOS)
  xputc(c);
#else
  putchar(c);
#endif
} 

/* print out 2 bytes character to tty (no conversion) */
static void
tty_cputc2(unsigned int c, unsigned int c2)
{
#ifdef SDL_GRAPHICS
  /* Both bytes of one character, still in the internal code. */
  kmode = 0;
  sdl_puteuc((int)c, (int)c2);
  return;
#endif
  kmode = 1;

#if defined(NO_TERMS) && defined(MSDOS)
  xputc2(c, c2);
#else
  putchar(c);
  putchar(c2);
#endif
} 

/* print out 1 byte character to tty (IC->output_kcode) */
static void
tty_jputc(unsigned int c)
{
#ifdef SDL_GRAPHICS
  /*
  ** A single byte, which the backend still has to interpret: between
  ** graph_on() and graph_off() it means a line-drawing character rather
  ** than the ASCII of the same value.  sdl_putbyte() is the layer that
  ** knows which graphics set is in effect.
  */
  kmode = 0;
  sdl_putbyte((int)c);
  return;
#endif
  if(kmode && output_kcode==JIS ){
    putchar(033);
    putchar('(');
    putchar('B');
  }
  kmode = 0;

#if defined(NO_TERMS) && defined(MSDOS)
  xputc(c);
#else
  putchar(c);
#endif
}

/* print out 2 bytes character to tty (IC->output_kcode) */
static void
tty_jputc2(unsigned int c, unsigned int c2)
{
#ifdef SDL_GRAPHICS
  /* As tty_cputc2(): setkcode() pinned output_kcode to IC, so jbuffer()
  ** has not touched these bytes. */
  kmode = 0;
  sdl_puteuc((int)c, (int)c2);
  return;
#endif
  if(!kmode && output_kcode==JIS ){
    putchar(033);
    putchar('$');
    putchar('B');
  }
  kmode = 1;
#if defined(NO_TERMS) && defined(MSDOS)
  xputc2(c, c2);
#else
  putchar(c);
  putchar(c2);
#endif
}

/*
**  japanese buffersing function
*/
int
jbuffer(
     unsigned int c,
     unsigned int *buf,
     void (*reset)(),
     void (*f1)(unsigned int),
     void (*f2)(unsigned int, unsigned int))
{
  static unsigned int ibuf[2];
  unsigned int c1, c2;
  unsigned char uc[2];
  unsigned char *p;

  if(!buf) buf = ibuf;
  if(!reset) reset = tty_reset;
  if(!f1) f1 = tty_jputc;
  if(!f2) f2 = tty_jputc2;

  if(!(buf[0]) && (is_kanji(c))){
    buf[1] = c;
    ++buf[0];
    return 0;
  }
  else if(buf[0]){
    c1 = buf[1];
    c2 = c;

    if(IC == output_kcode)
      ;
    else if(IC == EUC){
      switch(output_kcode){
      case JIS:
	c1 &= 0x7f;
	c2 &= 0x7f;
	break;
      case SJIS:
	uc[0] = c1;
	uc[1] = c2;
	p = e2sj(uc);
	c1 = p[0];
	c2 = p[1];
	break;
      default:
	impossible("Unknown kcode!");
	break;
      }
    }
    else if(IC == SJIS){
      uc[0] = c1;
      uc[1] = c2;
      p = sj2e(uc);
      switch(output_kcode){
      case JIS:
	c1 &= 0x7f;
	c2 &= 0x7f;
	break;
      case EUC:
	break;
      default:
	impossible("Unknown kcode!");
	break;
      }
    }
    f2(c1, c2);
    buf[0] = 0;
    return 2;
  }
  else if(c){
    f1(c);
    return 1;
  }
  reset();
  return -1;
}

int
cbuffer(
     unsigned int c,
     unsigned int *buf,
     void (*reset)(),
     void (*f1)(unsigned int),
     void (*f2)(unsigned int, unsigned int))
{
  static unsigned int ibuf[2];

  if(!buf) buf = ibuf;
  if(!reset) reset = tty_reset;
  if(!f1) f1 = tty_cputc;
  if(!f2) f2 = tty_cputc2;

  if(!(buf[0]) && is_kanji(c)){
    buf[1] = c;
    ++buf[0];
    return 0;
  }
  else if(buf[0]){
    f2(buf[1], c);
    buf[0] = 0;
    return 2;
  }
  else if(c){
    f1(c);
    return 1;
  }
  reset();
  return -1;
}

void
jputchar(int c)
{
  static unsigned int buf[2];
  jbuffer((unsigned int)c, buf, NULL, NULL, NULL);
}
void
cputchar(int c)
{
  static unsigned int buf[2];
  cbuffer((unsigned int)c, buf, NULL, NULL, NULL);
}

void
jputs(s)
     const char *s;
{
  while(*s)
    jputchar((unsigned char)*s++);
  jputchar('\n');
}

/*
**      Is byte offset pos inside a character rather than at its start?
**
**      The old body walked the string two bytes at a time on seeing a high
**      bit and reported whether it had overshot pos.  That is exactly
**      "pos is not a character boundary" with EUC-JP's widths hardcoded, so
**      it becomes a call to mb_is_boundary() and stops caring how wide a
**      character is.  See include/mbchar.h and UTF8-PLAN.md.
**
**      Both of these keep their names.  There are fourteen call sites and
**      the surrounding code reads in terms of them; renaming would make the
**      Phase 1 diff impossible to review for no gain, and the names stay
**      accurate enough -- "is this the trailing part of a wide character".
*/
int is_kanji2(s,pos)
const char *s;
int pos;
{
  return !mb_is_boundary(s, pos);
}

/*
**      Does a character wider than one byte start at offset pos?
**
**      Used before truncating: if the byte about to be cut off is the start
**      of a wide character, the caller blanks or drops it rather than
**      leaving half behind.
*/
int is_kanji1(s,pos)
const char *s;
int pos;
{
  return mb_is_boundary(s, pos) && mb_seqlen(s + pos) > 1;
}

/*
**	8bit through isspace for Japanese
*/
int
isspace_8(c)
     int c;
{
  unsigned int *up;

  up = (unsigned int *)&c;
  return *up<0x80 ? isspace(*up) : 0;
}
/*
** split string(str) including japanese before pos and return to
** str1, str2.
*/
/*
**      Kinsoku sets.  Given as strings rather than byte pairs so that they
**      convert with the rest of the tree in Phase 3 of UTF8-PLAN.md, and so
**      that the widths below come from the literals instead of a hardcoded 2.
*/
static const char *const jsplit_close[] = {
  "］", "）", "｝", 0           /* pulled back into the first half */
};
static const char *const jsplit_nostart[] = {
  "！", "？", "、", "。", "，", "．", 0 /* may not begin the second half */
};

/*
**      Does one of tab's entries start at s, on a character boundary?
**      Returns its length in bytes, or 0.
*/
static int
jmatch( str, at, tab )
     const char *str;
     int at;
     const char *const *tab;
{
  int i;

  if(!mb_is_boundary(str, at))
    return 0;

  for( i=0 ; tab[i] ; ++i ){
    int n = strlen(tab[i]);

    if(!strncmp(str + at, tab[i], n))
      return n;
  }
  return 0;
}

/*
** split string(str) including japanese before pos and return to
** str1, str2.
*/
/*
**      Rewritten for Phase 1 of UTF8-PLAN.md.  The logic is unchanged --
**      test/jlib.golden pins every break position of every test string --
**      but it now tracks the break offset directly instead of counting bytes
**      backwards from pos, and every step is the width of an actual
**      character rather than a literal 2.
**
**      The old form made the arithmetic hard to follow as well as wrong for
**      UTF-8: "j -= 2" meant "move the break two bytes later", so the sign
**      was inverted relative to the position it computed.
*/
void
split_japanese( str, str1, str2, pos )
     char *str;
     char *str1;
     char *str2;
     int pos;
{
  int len, i, b, k, n, lowest;
  char *pstr;
  char *pnstr;

  len = strlen(str);

  if( len < pos ){
    strcpy(str1,str);
    *str2 = '\0';
    return;
  }

  /*
  **    Align to a character boundary.  This was "--i", which stepped back
  **    exactly one byte -- right for EUC-JP, one byte short of the start
  **    of a three-byte UTF-8 character.
  */
  i = pos;
  if(!mb_is_boundary(str, i))
    i = (int)(mb_prev(str, str + i) - str);

  /*
  **    How far back the search may reach.  Measured from i and not from
  **    pos: the original counted j upwards from the aligned i, so a window
  **    of mlen bytes ends at i-mlen however far the alignment moved i.
  */
  lowest = i - ((pos > 20) ? 20 : pos);

  /*
  **    The loops below stop at lowest+1, so lowest may be -1: offset 0 is
  **    a position the original examined and clamping to 0 would skip it.
  **    It must not go further, though.  The original could, once the
  **    alignment above moved i by more than one byte -- impossible under
  **    EUC-JP, routine under UTF-8 -- and then indexed the string with a
  **    negative subscript.
  */
  if(lowest < -1)
    lowest = -1;

  b = -1;

/* 1:
** search space character
*/
  for( k=i ; k>lowest ; --k ){
    if(isspace_8(str[k])){
      b = k + 1;                /* the space stays in the first half */
      break;
    }
    if(is_kanji1(str,k) && !strncmp(str+k,"　",sizeof("　")-1)){
      b = k + sizeof("　")-1;   /* likewise the ideographic space */
      break;
    }
  }

/* 2:
** search end of japanese -- disabled long before this rewrite
*/

/* 3:
** search the start of a wide character
*/
  if(b < 0)
    for( k=i ; k>lowest ; --k )
      if(is_kanji1(str,k)){
	b = k;
        break;
      }

  if(b < 0)
    b = lowest;
  if(b < 0)
    b = 0;                      /* the copy below treats -1 as 0 anyway */

  /*
  **    The window's lower bound is a byte count, so nothing so far
  **    guarantees it lands between characters.  It did not: splitting
  **    "xxx...KANJI...xxx" at 47 cut the second kanji in half and handed
  **    the caller two strings that were not valid text.  Both halves went
  **    on to the screen and, for topten.c, into the record file.
  **
  **    This is older than the UTF-8 work -- the original computed the same
  **    offset -- but it is worth fixing here rather than carrying forward,
  **    because under UTF-8 the broken halves would reach a decoder that now
  **    rejects them, turning a display glitch into a string of U+FFFD.
  */
  if(!mb_is_boundary(str, b))
    b = (int)(mb_prev(str, str + b) - str);

  /*
  **    Kinsoku.  A closing bracket may not begin the second half, so the
  **    break moves past it; a full stop or comma may not either, so the
  **    break moves back over the character before it.
  */
  while(1){
    if(b < i &&
       (str[b] == ']' ||
	str[b] == ')' ||
	str[b] == '}'))
      ++b;
    else if(b < i-1 && (n = jmatch(str, b, jsplit_close)) != 0)
      b += n;
    else
      break;
  }

  while(b > 0 && jmatch(str, b, jsplit_nostart))
    b = (int)(mb_prev(str, str + b) - str);

  pstr = str;

  pnstr = str1;
  for( k=0 ; k<b ; ++k )
    *(pnstr++) = *(pstr++);
  *(pnstr++) = '\0';

  pnstr = str2;
  for( ; str[k] ; ++k )
    *(pnstr++) = *(pstr++);
  *(pnstr++) = '\0';
}

/*
**      Kanji numerals and the counter words that follow them.
**
**      These live here rather than in src/objnam.c, where readobjnam() used
**      to spell them out as twenty-odd strncmp() calls with the byte length
**      written in by hand.  They are facts about Japanese, not about object
**      naming, and down here they can be tested: japanese/jlib.c links
**      standalone against four stubs, which src/objnam.c does not.
**
**      Every length comes from the literal itself, so Phase 3 of
**      UTF8-PLAN.md converts the table and nothing else has to change.
*/
static const char *const jnumeral_tab[] = {
  "一", "二", "三", "四", "五", "六", "七", "八", "九", "十", 0
};

/*
**      Counter words, in the order readobjnam() tested them.  "の" is last
**      because it is the bare particle -- the fallback when no counter is
**      given -- and the others all end in it.
*/
static const char *const jcounter_tab[] = {
  "冊の", "本の", "着の", "個の", "枚の", "つの", "の", 0
};

/*
**      If a kanji numeral from 1 to 10 starts at s, return its value and put
**      its length in bytes in *len.  Otherwise return 0 and leave *len alone.
*/
int
jnumeral(s, len)
     const char *s;
     int *len;
{
  int i;

  for( i=0 ; jnumeral_tab[i] ; ++i ){
    int n = strlen(jnumeral_tab[i]);

    if(!strncmp(s, jnumeral_tab[i], n)){
      *len = n;
      return i+1;
    }
  }
  return 0;
}

/*
**      Length in bytes of the counter word at s, or 0 if there is none.
*/
int
jcounter(s)
     const char *s;
{
  int i;

  for( i=0 ; jcounter_tab[i] ; ++i ){
    int n = strlen(jcounter_tab[i]);

    if(!strncmp(s, jcounter_tab[i], n))
      return n;
  }
  return 0;
}

void 
jrndm_replace(c)
     char *c;
{
  unsigned char cc[3];

  if(IC==SJIS)
    memcpy(cc, (char *)sj2e(c), 2);
  else
    memcpy(cc, c, 2);

  cc[0] &= 0x7f;
  cc[1] &= 0x7f;

  switch(cc[0]){
  case 0x21:
    cc[1] = rn2(94) + 0x21;
    break;
  case 0x23:
    if(cc[1] <= 0x39) /* ０〜９ */
      cc[1] = rn2(10) + 0x30;
    else if(cc[1] <= 0x5A) /* Ａ〜Ｚ */
      cc[1] = rn2(26) + 0x41;
    else if(cc[2] <= 0x7A) /* ａ〜ｚ */
      cc[1] = rn2(26) + 0x61;
    break;
  case 0x24:
  case 0x25:
    cc[1] = rn2(83) + 0x21; /* あ〜ん or ア〜ン */
    break;
  case 0x26:
    if(cc[1] <= 0x30)
      cc[1] = rn2(24) + 0x21; /* Α〜Ω ギリシャ文字 */
    else
      cc[1] = rn2(24) + 0x41; /* α〜ω ギリシャ文字 */
    break;
  case 0x27:
    if(cc[1] <= 0x40)
      cc[1] = rn2(33) + 0x21; /* А〜Я ロシア文字 */
    else
      cc[1] = rn2(33) + 0x51; /* а〜я ロシア文字 */
    break;
  case 0x4f:
    cc[1] = rn2(51) + 0x21; /* 蓮〜 腕 */
    break;
  case 0x74:
    cc[1] = rn2(4) + 0x21; /* 堯 槇 遙 瑤 の4文字*/
    break;
  default:
    if(cc[0] >= 0x30 && cc[1] <= 0x74)
      cc[1] = rn2(94) + 0x21;
    break;
  }

  cc[0] |= 0x80;
  cc[1] |= 0x80;

  if(IC==SJIS)
    memcpy(c, (char *)e2sj(cc), 2);
  else
    memcpy(c, cc, 2);
}

const char *
joffmsg(otmp, joshi)
register struct obj *otmp;
const char **joshi;
{
  static char buf[BUFSZ];

  *joshi = "を";

  if(otmp->oclass == RING_CLASS){
    Sprintf(buf, "%sからはずす", body_part(FINGER));
    return buf;
  }
  if( otmp->oclass == AMULET_CLASS){
    return "はずす";
  }
  else if(is_helmet(otmp))
    return "取る";
  else if(is_gloves(otmp))
    return "はずす";
  else if(otmp->oclass == WEAPON_CLASS||is_shield(otmp)){
    *joshi = "の";
    return "装備を解く";
  }
  else if(is_suit(otmp))
    return "脱ぐ";
  else
    return "はずす";
}

const char *
jonmsg(otmp, joshi)
register struct obj *otmp;
const char **joshi;
{
  static char buf[BUFSZ];

  *joshi = "を";

  if(otmp->oclass == RING_CLASS){
    Sprintf(buf, "%sにはめる", body_part(FINGER));
    return buf;
  }
  else if(otmp->oclass == AMULET_CLASS)
    return "身につける";
  else if(is_gloves(otmp))
    return "身につける";
  else if(is_shield(otmp)){
    *joshi = "で";
    return "身を守る";
  }
  else if(is_helmet(otmp))
    return "かぶる";
  else if(otmp->oclass == WEAPON_CLASS){
    Sprintf(buf, "%sにする", body_part(HAND));
    return buf;
  }
  else if(is_boots(otmp))
    return "履く";
  else if(is_suit(otmp))
    return "着る";
  else
    return "身につける";
}
