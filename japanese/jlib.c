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

int is_kanji2(s,pos)
const char *s;
int pos;
{
  unsigned char *str;

  str = (unsigned char *)s;
  while(*str && pos>0){
    if(is_kanji(*str)){
      str+=2;
      pos-=2;
    }
    else{
      ++str;
      --pos;
    }
  }
  if(pos<0)
    return 1;
  else
    return 0;
}

int is_kanji1(s,pos)
const char *s;
int pos;
{
  unsigned char *str;

  str = (unsigned char *)s;
  while(*str && pos>0){
    if(is_kanji(*str)){
      str+=2;
      pos-=2;
    }
    else{
      ++str;
      --pos;
    }
  }
  if(!pos && is_kanji(*str))
    return 1;
  else
    return 0;
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
void
split_japanese( str, str1, str2, pos )
     char *str;
     char *str1;
     char *str2;
     int pos;
{
  int len, i, j, k, mlen;
  char *pstr;
  char *pnstr;

  len = strlen((char *)str);

  if( len < pos ){
    strcpy(str1,str);
    *str2 = '\0';
    return;
  }

  if(pos > 20)
    mlen = 20;
  else
    mlen = pos;

  i = pos;
  if(is_kanji2(str, i))
    --i;

/* 1:
** search space character
*/
  j = 0;
  while( j<mlen ){
    if(isspace_8(str[i-j])){
/*    str[i-j] = '\0';*/
      --j;
      goto found;
    }
    else if(is_kanji1(str,i-j)){
      if(!strncmp(str+i-j,"　",2)){
/*	str[i-j] = '\0'; */
	j -= 2;
	goto found;
      }
    }
    ++j;
  }
/* 2:
** search end of japanese
*/
#if 0
  j = 0;
  while( j<mlen ){
    if((is_kanji1(str,i-j) && !is_kanji2(str,i-j-1))||
       (is_kanji2(str,i-j-1) && !is_kanji1(str,i-j))){
      goto found;
    }
    ++j;
  }
#endif
/* 3:
** search second bytes of japanese
*/
  j = 0;
  while( j<mlen ){
    if(is_kanji1(str,i-j)){
      goto found;
    }
    ++j;
  }
 found:

  while(1){
    if(j>0 && 
       (str[i-j] == ']' ||
	str[i-j] == ')' ||
	str[i-j] == '}'))
      --j;
    else if(j>1 &&
	    ((!strncmp(str+i-j, "］", 2)) ||
	     (!strncmp(str+i-j, "）", 2)) ||
	     (!strncmp(str+i-j, "｝", 2))))
      j-=2;
    else
      break;
  }
  while(!strncmp(str+i-j,"！",2) ||
	!strncmp(str+i-j,"？",2) ||
	!strncmp(str+i-j,"、",2) ||
	!strncmp(str+i-j,"。",2) ||
	!strncmp(str+i-j,"，",2) ||
	!strncmp(str+i-j,"．",2))
    j+=2;

  pstr = str;

  pnstr = str1;
  for( k=0 ; k<i-j ; ++k )
    *(pnstr++) = *(pstr++);
  *(pnstr++) = '\0';

  pnstr = str2;
  for( ; str[k] ; ++k )
    *(pnstr++) = *(pstr++);
  *(pnstr++) = '\0';
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
