/*
**
**	$Id: bone_ext.c,v 1.1 1999/11/16 05:07:03 issei Exp issei $
**
*/

/* Copyright (c) Issei Numata 1994-2000 */
/* JNetHack may be freely redistributed.  See license for details. */

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

#include "patchlevel.h"
#ifdef JNETHACK
#include "../japanese/jpatchlevel.h"
#endif

#ifndef WIN32
# include <sys/utsname.h>
#endif

#include <fcntl.h>

static void
http_read(int sd, NHBUF *data)
{
    int len;
    char buf[4096];

    while((len = soc_read(sd, buf, 4096)) > 0){
	nh_buf_append(data, buf, len);
    }
}

static void
http_post(int sd, char *url, NHBUF *data)
{
    NHBUF *output;

    output = nh_buf_new();
    nh_buf_sprintf(
	output, "POST %s HTTP/1.0\r\n", url);
	  
    nh_buf_sprintf(
	output, "User-Agent: JNetHack-%d.%d.%d.%d\r\n",
	JVERSION_MAJOR, JVERSION_MINOR,
	JPATCHLEVEL, JEDITLEVEL);
	  
    nh_buf_sprintf(output, "Content-Length: %d\r\n", data->size);
    nh_buf_sprintf(output, "Content-Encoding: binary\r\n");
    nh_buf_sprintf(output, "Content-Type: application/octet-stream\r\n");
    nh_buf_sprintf(output, "\r\n");
    nh_buf_append(output, data->data, data->size);

    soc_write(sd, output->data, output->size);
}

#define	ht_append	nh_buf_append
#define ht_sprintf	nh_buf_sprintf

#if 0
NHBUF *
ht_append(NHBUF *b, char *data, size_t size)
{
    NHBUF		*ret;
    unsigned char	*p;
    char		tmp[10];

    p = data;
    for( ; p < (unsigned char *)(data) + size ; ){
	if(*p >= '0' && *p <= '9' ){
	    ret = nh_buf_append(b, p, 1);
	    if(!ret)
		return NULL;
	    ++p;
	}
	else if(*p >= 'A' && *p <= 'Z' ){
	    ret = nh_buf_append(b, p, 1);
	    if(!ret)
		return NULL;
	    ++p;
	}
	else if(*p >= 'a' && *p <= 'z' ){
	    ret = nh_buf_append(b, p, 1);
	    if(!ret)
		return NULL;
	    ++p;
	}
	else{
	    sprintf(tmp, "%%%02X", *p);
	    ret = nh_buf_append(b, tmp, 3);
	    if(!ret)
		return NULL;

	    ++p;
	}
    }

    return b;
}

NHBUF *
ht_sprintf(NHBUF *b, const char *fmt, ...)
{
    char		*tmpbuf;

    va_list	ap;
    va_start(ap, fmt);
    vasprintf(&tmpbuf, fmt, ap);
    va_end(ap);

    ht_append(b, tmpbuf, strlen(tmpbuf));

    free(tmpbuf);
    return b;
}
#endif

static void
ht_version(NHBUF *b)
{
    ht_sprintf(b, "version: %d\n", REPORTSCORE_VER);
    ht_sprintf(b, "nethack version: %d %d %d %d\n", 
	      VERSION_MAJOR, VERSION_MINOR,
	      PATCHLEVEL, EDITLEVEL);
    ht_sprintf(b, "jnethack version: %d %d %d %d\n", 
	      JVERSION_MAJOR, JVERSION_MINOR,
	      JPATCHLEVEL, JEDITLEVEL);
}

#define IC ((unsigned char)("漢"[0])==0x8a)

static void
ht_encode(NHBUF *b)
{
    if(IC){
	ht_sprintf(b, "encode: x-sjis\n");
    }
    else{
	ht_sprintf(b, "encode: euc-jp\n");
    }
}

static void
ht_uname(NHBUF *b)
{
#ifndef WIN32
    struct utsname nm;
#else
    OSVERSIONINFO osvi;
#endif

#ifndef WIN32
    uname(&nm);
    
    ht_sprintf(b, "sysname: %s\n", nm.sysname);
    ht_sprintf(b, "release: %s\n", nm.release);
    ht_sprintf(b, "machine: %s\n", nm.machine);
#else
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
	ht_sprintf(b, "sysname: win32(95/98)\n");
    } else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	ht_sprintf(b, "sysname: win32(95/98)\n");
    } else {
	ht_sprintf(b, "sysname: win32(95/98)\n");
    }
    
    ht_sprintf(b, "release: %d.%d\n", osvi.dwMajorVersion, osvi.dwMinorVersion);
    ht_sprintf(b, "machine: i586\n");
#endif
}

static void
ht_endian(NHBUF *b)
{
    static const int tmp[2] = {0x31323334, 0};

    ht_sprintf(b, "endian: %s\n", (const char *)tmp);
}

void
report_score(char *action, char *linebuf)
{
#ifdef WIN32
    WSADATA wsaData;
    WORD wVersionRequested =(WORD) (( 1) |  ( 1 << 8));
#endif

    int c;
    int sd;
    NHBUF *score;

    score = nh_buf_new();

    ht_version(score);
    ht_encode(score);
    ht_uname(score);
   
    ht_sprintf(score, "character: %s\n",
	      jtrns_mon(pl_character, flags.female));
    ht_sprintf(score, "female: %d\n", flags.female);
    ht_sprintf(score, "name: %s\n", plname);
    ht_sprintf(score, "score: %d\n", (int)u.urexp);
    ht_sprintf(score, "action: %s\n", action);
    ht_sprintf(score, "uz: %d\n", depth(&u.uz));
    ht_sprintf(score, "level: %d\n", u.ulevel);
    ht_sprintf(score, "maxhp: %d\n", u.uhpmax);
    ht_sprintf(score, "deepest: %d\n", deepest_lev_reached(TRUE));
    ht_sprintf(score, "gold: %d\n", (int)u.ugold);
    ht_sprintf(score, "moves: %d\n", (int)moves);
    ht_sprintf(score, "dying message: %s\n", linebuf);
    ht_sprintf(score, "homeurl: %s\n", get_homeurl());
    
#ifdef WIN32
    if(WSAStartup(wVersionRequested, &wsaData)){
	raw_printf("Report: WSAStartup failed.");
	goto report_end;
    }
#endif

    sd = connect_scoreserver();

    while(sd < 0){
	raw_printf("Report: %s", soc_err());
	c = yn_function("スコアサーバーとの接続に失敗しました。再度接続を試みますか？",ynqchars,'y');
	if(c != 'y')
	    goto report_end;
    }
    http_post(sd, SCORE_PATH, score);

#ifndef WIN32
    close(sd);
report_end:
#else
    closesocket(sd);
report_end:
    WSACleanup();
#endif
}

void
send_bones()
{
#ifdef WIN32
    WSADATA wsaData;
    WORD wVersionRequested =(WORD) (( 1) |  ( 1 << 8));
#endif
    int c;
    int sd, fd;
    int len;
    NHBUF *bones;
    char buf[BUFSZ];
    char *bonesid;

    bones = nh_buf_new();

    ht_version(bones);
    ht_uname(bones);
    ht_endian(bones);
    ht_sprintf(bones, "name: %s\n", plname);

    fd = open_bonesfile(&u.uz, &bonesid);
    if(fd < 0)
	return;

    ht_sprintf(bones, "filename: %s\n", bonesid);

    ht_sprintf(bones, "bones: \n\n");

    while((len = read(fd, buf, BUFSZ))> 0){
	ht_append(bones, buf, len);
    }

    close(fd);

#ifdef WIN32
    if(WSAStartup(wVersionRequested, &wsaData)){
	raw_printf("Report: WSAStartup failed.");
	goto report_end;
    }
#endif
    sd = connect_bonesserver();
    while(sd < 0){
	raw_printf("Report: %s", soc_err());
	c = yn_function("骨サーバーとの接続に失敗しました。再度接続を試みますか？",ynqchars,'y');
	if(c != 'y')
	    goto report_end;
    }
    http_post(sd, BONES_PATH, bones);

    nh_buf_delete(bones);
    delete_bonesfile(&u.uz);

    bones = nh_buf_new();

    {
	NHBUF *sub;
	int fd;
	int pos;

	http_read(sd, bones);
	pos = nh_buf_search(bones, "\r\n\r\n");

	if(pos >= 0){
	    sub = nh_buf_subbuf(bones, pos + 4, 0);
	    if(sub->size > 0){
		fd = create_bonesfile(&u.uz, &bonesid);
		if(fd >= 0){
		    nh_buf_write(sub, fd);
		    close(fd);
		    commit_bonesfile(&u.uz);
		}
	    }
	}
    }

#ifndef WIN32
    close(sd);
report_end:
#else
    closesocket(sd);
report_end:
    WSACleanup();
#endif
}
