/*
**
**	$Id: nhbuf.c,v 1.1 1999/11/16 05:07:03 issei Exp issei $
**
*/

/* Copyright (c) Issei Numata 1994-2000 */

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#if defined(WIN32) || defined(_WIN32)   /* config.h has not been read yet */
/*
 * Ahead of hack.h on purpose.  <windows.h> has parameters named
 * Protection, Warning and Confusion, and include/youprop.h turns those
 * identifiers into u.uprops[...] expressions, so the Windows headers have
 * to be parsed while they still mean what they say.  WIN32_LEAN_AND_MEAN
 * additionally keeps out <rpcndr.h>, which typedefs boolean.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock.h>
#undef TRUE             /* include/global.h defines its own, unguarded */
#undef FALSE
#endif

#include "hack.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#include <setjmp.h>
#include <signal.h>
#endif

/*
  simple buffer library
 */

#define	NH_BUFSIZ	(4096)

#if defined(WIN32) || defined(SUNOS4)
#define vasprintf	Vasprintf
#endif

#ifdef SUNOS4
static int
Vasprintf(char **buf, const char *fmt, va_list ap)
{
    int ret;

    *buf = malloc(NH_BUFSIZ);

    ret = vsnprintf(*buf, NH_BUFSIZ, fmt, ap);

    return ret;
}
#endif

#ifdef WIN32
static int
Vasprintf(char **buf, const char *fmt, va_list ap)
{
    int ret;
    *buf = malloc(NH_BUFSIZ * 4);

    ret = vsprintf(*buf, fmt, ap);

    return ret;
}
#endif

NHBUF *
nh_buf_new(void)
{
    NHBUF *p;

    p = malloc(sizeof(NHBUF));
    if(!p)
	return NULL;

    p->size = 0;
    p->max_size = NH_BUFSIZ;
    p->data = malloc(NH_BUFSIZ);
    if(!p->data){
	free(p);
	return NULL;
    }
    return p;
}

void
nh_buf_delete(NHBUF *b)
{
    free(b->data);
    free(b);
}

int
nh_buf_append(NHBUF *buf, const char *data, size_t size)
{
    char *tmp;

    while(buf->size + size > buf->max_size){
	tmp = malloc(buf->max_size * 2);
	if(!tmp)
	    return -1;

	memcpy(tmp, buf->data, buf->max_size);
	free(buf->data);

	buf->data = tmp;

	buf->max_size *= 2;
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;

    return buf->size;
}

int
nh_buf_sprintf(NHBUF *buf, const char *fmt, ...)
{
    int		ret;
    char	*tmpbuf;
    va_list	ap;

    va_start(ap, fmt);
    vasprintf(&tmpbuf, fmt, ap);
    va_end(ap);

    if(!tmpbuf)
	return -1;

    ret = nh_buf_append(buf, tmpbuf, strlen(tmpbuf));

    free(tmpbuf);

    return ret;
}

int
nh_buf_read(NHBUF *buf, int fd)
{
    int len;
    char tmp[NH_BUFSIZ];

    while((len = read(fd, tmp, NH_BUFSIZ)) > 0)
	nh_buf_append(buf, tmp, len);

    return buf->size;
}

int
nh_buf_write(NHBUF *buf, int fd)
{
    write(fd, buf->data, buf->size);

    return buf->size;
}

int
nh_buf_search(NHBUF *buf, const char *str)
{
    char *ret;

    ret = strstr(buf->data, str);

    if(!ret)
	return -1;

    return ret - buf->data;
}

NHBUF *
nh_buf_subbuf(NHBUF *buf, int pos1, size_t sz)
{
    NHBUF *ret;

    if(pos1 < 0)
	return NULL;

    ret = nh_buf_new();

    if(sz <= 0)
	sz = buf->size - pos1;

    nh_buf_append(ret, buf->data + pos1, sz);

    return ret;
}






