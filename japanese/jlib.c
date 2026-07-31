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
#include "jiscode.h"
#include "utf8.h"

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
#define UTF8    3

/*
**	The internal code.
**
**      This used to be worked out at compile time from the first byte of a
**      kanji literal -- 0x8a meant the file had been converted to
**      Shift-JIS, anything else meant EUC-JP.  That trick only distinguishes
**      two encodings, and only because their lead bytes happen to differ; it
**      says nothing at all once the literals are UTF-8.  The build decides
**      now, in include/config.h.
*/
#ifdef JP_INTERNAL_UTF8
# define IC     UTF8
#else
# define IC     EUC
#endif

/*
**      Defaults for the outside world.
**
**      The internal code is the default in both directions now, so a
**      terminal that agrees with the build needs no conversion at all and
**      the common case costs nothing.  A player on an EUC-JP or Shift-JIS
**      terminal still says so with the -k option, as always.
**
**      This is a change of default: it used to be EUC-JP regardless, which
**      was the internal code then as well.
*/
/* default input kcode */
#ifndef INPUT_KCODE
# ifdef MSDOS
#  define INPUT_KCODE SJIS
# else
#  define INPUT_KCODE IC
# endif
#endif

/* default output kcode */
#ifndef OUTPUT_KCODE
# ifdef MSDOS
#  define OUTPUT_KCODE SJIS
# else
#  define OUTPUT_KCODE IC
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
  ** thing it can have, and the characters jbuffer() hands to sdl_putcp()
  ** have to still be in the internal code.  Pinning output_kcode to IC
  ** keeps jbuffer() from converting them on the way.
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
  else if(c == 'U' || c == 'u')
    output_kcode = UTF8;
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
/*
**      Convert a string from input_kcode to the internal code.
**
**      Every route goes by way of EUC-JP: Shift-JIS and JIS both have a
**      byte-level mapping to it and nothing else, and EUC-JP has one to
**      Unicode through japanese/jiscode.c.  Under an EUC-JP build the
**      second leg is nothing at all, which is why this used to look like
**      one conversion rather than two.
*/
static const char *
jto_euc(s)
     const char *s;
{
  static unsigned char buf[1024];
  unsigned char *p, *pp;
  int kin;

  p = buf;

  if( input_kcode == SJIS ){
    while(*s){
      if(is_kanji(*(const unsigned char *)s)){
	pp = sj2e((unsigned char *)s);
	*(p++) = pp[0];
	*(p++) = pp[1];
	s += 2;
      }
      else
	*(p++) = (unsigned char)*(s++);
    }
  }
  else if( input_kcode == JIS ){
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
  else {                        /* already EUC-JP */
    strcpy((char *)buf, s);
    return (char *)buf;
  }

  *p = '\0';
  return (char *)buf;
}

const char *
str2ic(s)
     const char *s;
{
  static unsigned char buf[1024];

  if(!s)
    return s;

  if( IC==input_kcode ){
    strcpy((char *)buf, s);
    return (char *)buf;
  }

#ifdef JP_INTERNAL_UTF8
  {
    const char *e = jto_euc(s);
    char *p = (char *)buf;
    int n, m;

    while(*e){
      long cp = euc_to_ucs(e, &n);

      if(!n){                   /* not EUC-JP: drop the byte rather than
                                   letting it into a UTF-8 string */
        ++e;
        continue;
      }
      e += n;
      m = utf8_encode(cp, p, MB_MAXBYTES);
      p += m;
    }
    *p = '\0';
    return (char *)buf;
  }
#else
  if( input_kcode == SJIS || input_kcode == JIS ){
    strcpy((char *)buf, jto_euc(s));
    return (char *)buf;
  }
  strcpy((char *)buf, s);
  return (char *)buf;
#endif
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
**  japanese buffering function
*/

/*
**      Convert one character from the internal code to output_kcode.
**      Returns how many bytes were written, or 0 if it has no form there.
**
**      Under EUC-JP internal this is the byte fiddling the old jbuffer()
**      did inline -- strip the high bits for JIS, e2sj() for Shift-JIS.
**      Under UTF-8 internal there is nothing to fiddle, so it goes through
**      the code point: that is the only representation the two families
**      have in common.
*/
static int
jconv_out(in, n, out)
     const char *in;
     int n;
     char *out;
{
  unsigned char uc[2], *p;

  if(output_kcode == IC){               /* nothing to do */
    memcpy(out, in, n);
    return n;
  }

#ifdef JP_INTERNAL_UTF8
  {
    long cp = mb_decode(in);
    int m;

    if(!cp)
      return 0;
    m = ucs_to_euc(cp, out, MB_MAXBYTES);
    if(m != 2)
      return m;                         /* ASCII, or no EUC-JP form */
    /* now in EUC-JP; the rest is the same two conversions as ever */
    uc[0] = (unsigned char)out[0];
    uc[1] = (unsigned char)out[1];
    switch(output_kcode){
    case EUC:
      return 2;
    case JIS:
      out[0] = (char)(uc[0] & 0x7f);
      out[1] = (char)(uc[1] & 0x7f);
      return 2;
    case SJIS:
      p = e2sj(uc);
      out[0] = (char)p[0];
      out[1] = (char)p[1];
      return 2;
    default:
      impossible("Unknown kcode!");
      return 0;
    }
  }
#else
  if(n != 2){                           /* ASCII, or SS2/SS3 we cannot map */
    memcpy(out, in, n);
    return n;
  }
  uc[0] = (unsigned char)in[0];
  uc[1] = (unsigned char)in[1];

  if(IC == EUC){
    switch(output_kcode){
    case JIS:
      out[0] = (char)(uc[0] & 0x7f);
      out[1] = (char)(uc[1] & 0x7f);
      return 2;
    case SJIS:
      p = e2sj(uc);
      out[0] = (char)p[0];
      out[1] = (char)p[1];
      return 2;
    default:
      impossible("Unknown kcode!");
      return 0;
    }
  }
  else {                                /* IC == SJIS */
    p = sj2e(uc);
    switch(output_kcode){
    case JIS:
      out[0] = (char)(p[0] & 0x7f);
      out[1] = (char)(p[1] & 0x7f);
      return 2;
    case EUC:
      out[0] = (char)p[0];
      out[1] = (char)p[1];
      return 2;
    default:
      impossible("Unknown kcode!");
      return 0;
    }
  }
#endif
}

/*
**      Feed one byte of the internal code; emit whole characters.
**
**      buf holds the partial character between calls: buf[0] is how many
**      bytes are in hand, buf[1] how many the lead byte says to expect, and
**      buf[2..] the bytes themselves.  It needs JBUF_SIZE elements -- it
**      used to need two, because a character was a byte or a pair of them
**      and nothing else was possible.
**
**      Returns 0 while a character is incomplete, its length in bytes when
**      one is emitted, and -1 for the flush that c == 0 asks for.
*/
int
jbuffer(
     unsigned int c,
     unsigned int *buf,
     void (*reset)(),
     void (*f1)(unsigned int),
     void (*f2)(unsigned int, unsigned int))
{
  static unsigned int ibuf[JBUF_SIZE];
  char in[MB_MAXBYTES + 1];
  int i, n;
#ifndef SDL_GRAPHICS
  char out[MB_MAXBYTES + 1];
  int m;
#endif

  if(!buf) buf = ibuf;
  if(!reset) reset = tty_reset;
  if(!f1) f1 = tty_jputc;
  if(!f2) f2 = tty_jputc2;

  if(buf[0]){                           /* part way through a character */
    buf[2 + buf[0]] = c;
    ++buf[0];
    if(buf[0] < buf[1])
      return 0;
  }
  else if(c){
    int need = mb_lead_len((int)c);

    if(need > 1){
      buf[0] = 1;
      buf[1] = (unsigned int)need;
      buf[2] = c;
      return 0;
    }
    f1(c);
    return 1;
  }
  else{
    reset();
    return -1;
  }

  n = (int)buf[0];
  for(i = 0; i < n; ++i)
    in[i] = (char)buf[2 + i];
  in[n] = '\0';
  buf[0] = 0;

#ifdef SDL_GRAPHICS
  /*
  ** The backend takes characters, not bytes: it puts a code point in a
  ** cell and takes the width from mb_cpwidth().  setkcode() pinned
  ** output_kcode to the internal code, so nothing is converted on the way.
  **
  ** This replaces a call to sdl_puteuc(), which decided the width from the
  ** fact that two bytes had arrived.  That was right for kanji and wrong
  ** for the SS2 half-width katakana, which are two bytes of EUC-JP and one
  ** column -- the tty side has always drawn them in one, so the two
  ** backends disagreed.  Going through the code point settles it.
  */
  sdl_putcp((int) mb_decode(in));
  return n;
#else
  m = jconv_out(in, n, out);
  if(m == 2)
    f2((unsigned int)(unsigned char)out[0],
       (unsigned int)(unsigned char)out[1]);
  else
    for(i = 0; i < m; ++i)
      f1((unsigned int)(unsigned char)out[i]);
  return n;
#endif
}

/*
**      As jbuffer(), but with no conversion: the bytes go out in the
**      internal code whatever output_kcode says.  Used for text the game
**      has already put in the right form, and for the graphics character
**      sets, where a byte is a line and not a letter.
*/
int
cbuffer(
     unsigned int c,
     unsigned int *buf,
     void (*reset)(),
     void (*f1)(unsigned int),
     void (*f2)(unsigned int, unsigned int))
{
  static unsigned int ibuf[JBUF_SIZE];
  int i, n;

  if(!buf) buf = ibuf;
  if(!reset) reset = tty_reset;
  if(!f1) f1 = tty_cputc;
  if(!f2) f2 = tty_cputc2;

  if(buf[0]){
    buf[2 + buf[0]] = c;
    ++buf[0];
    if(buf[0] < buf[1])
      return 0;
  }
  else if(c){
    int need = mb_lead_len((int)c);

    if(need > 1){
      buf[0] = 1;
      buf[1] = (unsigned int)need;
      buf[2] = c;
      return 0;
    }
    f1(c);
    return 1;
  }
  else{
    reset();
    return -1;
  }

  n = (int)buf[0];
  buf[0] = 0;

#ifdef SDL_GRAPHICS
  /*
  ** As in jbuffer(): the backend takes characters, and a single byte is
  ** the one case that must not be treated as one, because between
  ** graph_on() and graph_off() it names a line-drawing glyph rather than
  ** the character of that value.  sdl_putbyte() -- reached through f1 --
  ** is the layer that knows which graphics set is in effect.
  */
  {
    char in[MB_MAXBYTES + 1];

    for(i = 0; i < n; ++i)
      in[i] = (char)buf[2 + i];
    in[n] = '\0';
    sdl_putcp((int) mb_decode(in));
  }
  return n;
#else
  if(n == 2)
    f2(buf[2], buf[3]);
  else
    for(i = 0; i < n; ++i)
      f1(buf[2 + i]);
  return n;
#endif
}

void
jputchar(int c)
{
  static unsigned int buf[JBUF_SIZE];
  jbuffer((unsigned int)c, buf, NULL, NULL, NULL);
}
void
cputchar(int c)
{
  static unsigned int buf[JBUF_SIZE];
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

/*
**      Degrade one character of an engraving into another from the same
**      part of JIS X 0208, so that a kanji stays a kanji and a hiragana
**      stays a hiragana.
**
**      Rewritten for Phase 2 of UTF8-PLAN.md.  The logic is the same, but
**      it is now expressed in row and cell rather than in the bytes of an
**      EUC-JP pair, so it works whatever the internal encoding is.  The old
**      body masked 0x80 off both bytes to get the JIS values, adjusted the
**      low one, and put the bit back -- which is only how EUC-JP spells a
**      JIS position, not how UTF-8 does.
**
**      Takes the buffer and an offset rather than a pointer at the
**      character, because the replacement is not always the same number of
**      bytes: JIS X 0208 row 1 holds both U+00B1 and U+3000, two bytes and
**      three of UTF-8.  mb_replace() moves the rest of the string.
**
**      Two things about the original worth recording:
**
**        The lowercase branch for row 3 tested cc[2], which was never
**        assigned -- an uninitialised read of the third byte of a two-byte
**        buffer.  It is cell here, which is plainly what was meant, and is
**        not a behaviour change that can be preserved: the old one had none
**        to preserve.  A fullwidth lowercase letter now degrades into
**        another one instead of into whatever the stack happened to hold.
**
**        The default branch tests "cell <= 84" where the shape of every
**        other test, and the fact that rows 16 to 84 are the kanji, suggest
**        "row <= 84" was meant.  Left as it was, because it is a live
**        condition with defined behaviour and changing it would alter which
**        characters degrade.
*/
static int
jrndm_cell(row, cell)
     int row;
     int cell;
{
  switch(row){
  case 1:
    return rn2(94) + 1;
  case 3:
    if(cell <= 25)              /* fullwidth 0-9 */
      return rn2(10) + 16;
    else if(cell <= 58)         /* fullwidth A-Z */
      return rn2(26) + 33;
    else if(cell <= 90)         /* fullwidth a-z; was cc[2], uninitialised */
      return rn2(26) + 65;
    return cell;
  case 4:
  case 5:
    return rn2(83) + 1;         /* hiragana or katakana */
  case 6:
    if(cell <= 16)
      return rn2(24) + 1;       /* Greek capitals */
    else
      return rn2(24) + 33;      /* Greek small letters */
  case 7:
    if(cell <= 32)
      return rn2(33) + 1;       /* Cyrillic capitals */
    else
      return rn2(33) + 49;      /* Cyrillic small letters */
  case 47:
    return rn2(51) + 1;
  case 84:
    return rn2(4) + 1;
  default:
    if(row >= 16 && cell <= 84)
      return rn2(94) + 1;
    return cell;
  }
}

void 
jrndm_replace(buf, pos)
     char *buf;
     int pos;
{
  long cp;
  int row, cell, n;
  char rep[MB_MAXBYTES + 1];

  cp = mb_decode(buf + pos);
  if(!cp || !ucs_to_jis(cp, &row, &cell))
    return;                     /* not a JIS X 0208 character: leave it */

  cell = jrndm_cell(row, cell);

  cp = jis_to_ucs(row, cell);
  if(!cp)
    return;                     /*
                                ** Defensive only.  Every range above was
                                ** chosen to stay inside the assigned cells
                                ** of its row -- checked against the table,
                                ** all twelve of them -- which is why row 47
                                ** draws from 51 cells and row 84 from 4
                                ** rather than both from 94.  Should the
                                ** table or a range ever change, this stops
                                ** an unassigned position reaching the
                                ** engraving instead of writing it out.
                                */

  n = mb_encode(cp, rep, MB_MAXBYTES);
  if(!n)
    return;
  rep[n] = '\0';
  (void) mb_replace(buf, pos, rep);
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
