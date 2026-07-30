/*
**
**	$Id: nhinet.c,v 1.1 1999/11/16 05:07:03 issei Exp issei $
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

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#include <setjmp.h>
#include <signal.h>
#endif

static char	*errstr;
static char	*homeurl;
static char	*proxy = HTTP_PROXY;
static int	proxy_port = HTTP_PROXY_PORT;

void
set_homeurl(char *s)
{
    if(homeurl)
	free(homeurl);

    homeurl = malloc(strlen(s) + 1);
    strcpy(homeurl, s);
}

char *
get_homeurl()
{
    if(homeurl)
	return homeurl;
    else
	return "";
}

void
set_proxy(char *s)
{
    size_t len;

/*
    if(proxy)
	free(proxy);
*/

#ifndef WIN32
    if(!strncasecmp(s, "http://", 7)){
	s += 7;
    }
#else
    if(!strnicmp(s, "http://", 7)){
	s += 7;
    }
#endif

    len = strlen(s);
    proxy = malloc(len + 1);

    --len;
    while(len > 0 && s[len] >= '0' && s[len] <= '9')
	--len;

    if(len > 0 && s[len] == ':' && s[len + 1] != '\0'){
	s[len] = '\0';
	strcpy(proxy, s);
	proxy_port = atoi(s + (len + 1));
    }
    else{
	strcpy(proxy, s);
	proxy_port = HTTP_PROXY_PORT;
    }

    if(proxy_port == 0)
	proxy_port = 80;
}

char *
get_proxy()
{
    static char buf[BUFSIZ];

    if(proxy && proxy[0]){
#ifndef WIN32
	snprintf(buf, BUFSIZ, "%s:%d", proxy, proxy_port);
#else
	_snprintf(buf, BUFSIZ, "%s:%d", proxy, proxy_port);
#endif
	return buf;
    }
    else
	return "";
}

char *
get_proxy_host()
{
    return proxy;
}

int
get_proxy_port()
{
    return proxy_port;
}

int
soc_read(int sd, char *buf, size_t sz)
{
#ifndef WIN32
    return read(sd, buf, sz);
#else
    return recv(sd, buf, sz, 0);
#endif
}

int
soc_write(int sd, char *buf, size_t sz)
{
#ifndef WIN32
    write(sd, buf, sz);
#else
    int nleft, nwritten;
	
    nleft = sz;

    while (nleft > 0) {
	nwritten = send(sd, buf, nleft, 0);
	if (nwritten <= 0)
	    return (nwritten);
	nleft -= nwritten;
	buf += nwritten;
    }
#endif
    return sz;
}

int
soc_write_str(int sd, char *buf)
{
    return soc_write(sd, buf, strlen(buf));
}

#ifndef WIN32
static jmp_buf	env;
static void (*sig_int_saved)(int);
static void (*sig_alm_saved)(int);

static void
interrupt_report(int sig)
{
    unlock_file(RECORD);
    longjmp(env, sig);
}
#endif

static int
connect_server(int timeout, const char *host, int port)
{
    int			sd;
    struct sockaddr_in	to;
    struct hostent	*hp;

#ifndef WIN32
    struct itimerval	val, val0;
    int			ret;

    val0.it_interval.tv_sec = 0;
    val0.it_interval.tv_usec = 0;
    val0.it_value.tv_sec = 0;
    val0.it_value.tv_usec = 0;

    val.it_interval.tv_sec = 0;
    val.it_interval.tv_usec = 0;
    val.it_value.tv_sec = 10;
    val.it_value.tv_usec = 0;
    
    if((ret = setjmp(env)) != 0){
	signal(SIGINT, sig_int_saved);
	signal(SIGALRM, sig_alm_saved);
	if(ret == SIGALRM)
	    errstr = "error: timeout!\n";
	else
	    errstr = "error: interrupt!\n";
	
	return -1;
    }
    sig_int_saved = signal(SIGINT, interrupt_report);
    sig_alm_saved = signal(SIGALRM, interrupt_report);
    
    setitimer(ITIMER_REAL, &val, NULL);
#endif

    if(proxy && proxy[0]){
	if((hp = gethostbyname(proxy)) == NULL){
	    errstr = "error: bad score server!\n";
	    
#ifndef WIN32
	    signal(SIGINT, sig_int_saved);
	    signal(SIGALRM, sig_alm_saved);
#endif

	    return -1;
	}
    }
    else if((hp = gethostbyname(host)) == NULL){
	errstr = "error: bad score server!\n";
	
#ifndef WIN32
	signal(SIGINT, sig_int_saved);
	signal(SIGALRM, sig_alm_saved);
#endif
	return -1;
    }
    
#ifndef WIN32
    setitimer(ITIMER_REAL, &val0, NULL);
    
    signal(SIGALRM, sig_alm_saved);
#endif

    memset(&to, 0, sizeof(to));
    memcpy(&to.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    to.sin_family = AF_INET;
    
    if(proxy && proxy[0] && proxy_port)
	to.sin_port = htons(proxy_port);
    else
	to.sin_port = htons(port);
    
#ifndef WIN32
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	errstr = "error: cannot create socket!\n";
	return -1;
    }
#else
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET){
	errstr = "error: cannot create socket!\n";
	return -1;
    }
#endif

    if(connect(sd, (struct sockaddr *)&to, sizeof(to)) < 0){
	errstr = "error: cannot connect score server!\n";
	return -1;
    }

#ifndef WIN32
    signal(SIGINT, sig_int_saved);
#endif

    return sd;
}

int
connect_scoreserver()
{
    return connect_server(HTTP_TIMEOUT, SCORE_SERVER, SCORE_PORT);
}

int
connect_bonesserver()
{
    return connect_server(HTTP_TIMEOUT, BONES_SERVER, BONES_PORT);
}

char *
soc_err()
{
    return errstr;
}
