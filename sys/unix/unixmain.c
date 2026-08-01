/*	SCCS Id: @(#)unixmain.c	3.2	94/11/07	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* main.c - Unix NetHack */

/*
**	Japanese version Copyright (C) Issei Numata, 1994-1999
**	changing point is marked `JP' (94/6/7)
**	JNetHack may be freely redistributed.  See license for details. 
*/

#include "hack.h"
#include "dlb.h"

#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#ifndef O_RDONLY
#include <fcntl.h>
#endif

#if !defined(_BULL_SOURCE) && !defined(__sgi) && !defined(_M_UNIX)
# if !defined(SUNOS4) && !(defined(ULTRIX) && defined(__GNUC__))
#  if defined(POSIX_TYPES) || defined(SVR4) || defined(HPUX)
extern struct passwd *FDECL(getpwuid,(uid_t));
#  else
extern struct passwd *FDECL(getpwuid,(int));
#  endif
# endif
#endif
extern struct passwd *FDECL(getpwnam,(const char *));
#ifdef CHDIR
static void chdirx();
#endif /* CHDIR */
static boolean whoami();
static void FDECL(process_options, (int, char **));

#ifdef _M_UNIX
extern void NDECL(check_sco_console);
extern void NDECL(init_sco_cons);
#endif

static void NDECL(wd_message);
#ifdef WIZARD
static boolean wiz_error_flag = FALSE;
#endif

/*
 * The directory the player was standing in when they typed the command.
 * chdirx() below moves the process to the playground before the window
 * system starts, so anything that has to resolve a relative path the
 * player wrote -- on the command line or in the environment -- has to
 * resolve it against this and not against the current directory.
 * win/tty/sdlterm.c uses it for NETHACK_SDL_FONT.
 *
 * sys/share/pcmain.c has carried the same variable, under the same name,
 * since 3.2; this is the Unix half of it.  Empty if getcwd() failed, and
 * every user is expected to treat that as "no second place to look".
 */
char orgdir[PATHLEN];

int
main(argc,argv)
int argc;
char *argv[];
{
	register int fd;
#ifdef CHDIR
	register char *dir;
#endif
	boolean exact_username;

	hname = argv[0];
	hackpid = getpid();
	(void) umask(0777 & ~FCMASK);

	/* Before any chdir(); see the comment on orgdir[] above. */
	if (!getcwd(orgdir, sizeof orgdir))
		orgdir[0] = '\0';

	choose_windows(DEFAULT_WINDOW_SYS);

#ifdef CHDIR			/* otherwise no chdir() */
	/*
	 * See if we must change directory to the playground.
	 * (Perhaps hack runs suid and playground is inaccessible
	 *  for the player.)
	 * The environment variable HACKDIR is overridden by a
	 *  -d command line option (must be the first option given)
	 */
	dir = getenv("NETHACKDIR");
	if (!dir) dir = getenv("HACKDIR");
#endif
	if(argc > 1) {
#ifdef CHDIR
	    if (!strncmp(argv[1], "-d", 2) && argv[1][2] != 'e') {
		/* avoid matching "-dec" for DECgraphics; since the man page
		 * says -d directory, hope nobody's using -desomething_else
		 */
		argc--;
		argv++;
		dir = argv[0]+2;
		if(*dir == '=' || *dir == ':') dir++;
		if(!*dir && argc > 1) {
			argc--;
			argv++;
			dir = argv[0];
		}
		if(!*dir)
		    error("Flag -d must be followed by a directory name.");
	    }
	    if (argc > 1)
#endif /* CHDIR */

	    /*
	     * Now we know the directory containing 'record' and
	     * may do a prscore().
	     */
	    if (!strncmp(argv[1], "-s", 2)) {
#ifdef CHDIR
		chdirx(dir,0);
#endif
/*JP*/
#ifdef JNETHACK
		setkcode('I');
		initoptions();
		init_jtrns();
		prscore(argc, argv);
		jputchar('\0'); /* reset */
#else
		prscore(argc, argv);
#endif
		exit(EXIT_SUCCESS);
	    }
	}

	/*
	 * Find the creation date of this game,
	 * so as to avoid restoring outdated savefiles.
	 */
	gethdate(hname);


	/*
	 * We cannot do chdir earlier, otherwise gethdate will fail.
	 * Change directories before we initialize the window system so
	 * we can find the tile file.
	 */
#ifdef CHDIR
	chdirx(dir,1);
#endif

#ifdef _M_UNIX
	check_sco_console();
#endif
	/* Line like "OPTIONS=name:foo-@" may exist in config file.
	 * In this case, need to select random class,
	 * so must call setrandom() before initoptions().
	 */
	setrandom();
	initoptions();

	init_nhwindows(&argc,argv);

	exact_username = whoami();
#ifdef _M_UNIX
	init_sco_cons();
#endif

	/*
	 * It seems you really want to play.
	 */
	u.uhp = 1;	/* prevent RIP on early quits */
	(void) signal(SIGHUP, (SIG_RET_TYPE) hangup);
#ifdef SIGXCPU
	(void) signal(SIGXCPU, (SIG_RET_TYPE) hangup);
#endif
/*JP*/
#ifdef JNETHACK
	init_jtrns();
#endif

	process_options(argc, argv);	/* command line options */

#ifdef DEF_PAGER
	if(!(catmore = getenv("HACKPAGER")) && !(catmore = getenv("PAGER")))
		catmore = DEF_PAGER;
#endif
#ifdef MAIL
	getmailstatus();
#endif
#ifdef WIZARD
	if (wizard)
		Strcpy(plname, "wizard");
	else
#endif
	if(!*plname || !strncmp(plname, "player", 4)
		    || !strncmp(plname, "games", 4)) {
		askname();
	} else if (exact_username) {
		/* guard against user names with hyphens in them */
		int len = strlen(plname);
		/* append the current role, if any, so that last dash is ours */
		if (++len < sizeof plname)
			(void)strncat(strcat(plname, "-"),
				      pl_character, sizeof plname - len - 1);
	}
	plnamesuffix();		/* strip suffix from name; calls askname() */
				/* again if suffix was whole name */
				/* accepts any suffix */
#ifdef WIZARD
	if(!wizard) {
#endif
		/*
		 * check for multiple games under the same name
		 * (if !locknum) or check max nr of players (otherwise)
		 */
		(void) signal(SIGQUIT,SIG_IGN);
		(void) signal(SIGINT,SIG_IGN);
		if(!locknum)
			Sprintf(lock, "%d%s", (int)getuid(), plname);
		getlock();
#ifdef WIZARD
	} else {
		Sprintf(lock, "%d%s", (int)getuid(), plname);
		getlock();
	}
#endif /* WIZARD */

	dlb_init();	/* must be before newgame() */

	/*
	 * Initialization of the boundaries of the mazes
	 * Both boundaries have to be even.
	 */
	x_maze_max = COLNO-1;
	if (x_maze_max % 2)
		x_maze_max--;
	y_maze_max = ROWNO-1;
	if (y_maze_max % 2)
		y_maze_max--;

	/*
	 *  Initialize the vision system.  This must be before mklev() on a
	 *  new game or before a level restore on a saved game.
	 */
	vision_init();

	display_gamewindows();

	if ((fd = restore_saved_game()) >= 0) {
#ifdef WIZARD
		/* Since wizard is actually flags.debug, restoring might
		 * overwrite it.
		 */
		boolean remember_wiz_mode = wizard;
#endif
		(void) chmod(SAVEF,0);	/* disallow parallel restores */
		(void) signal(SIGINT, (SIG_RET_TYPE) done1);
#ifdef NEWS
		if(iflags.news) {
		    display_file(NEWS, FALSE);
		    iflags.news = FALSE; /* in case dorecover() fails */
		}
#endif
/*JP*/
#ifdef JNETHACK
		pline("セーブファイルを復元中．．．");
#else
		pline("Restoring save file...");
#endif
		mark_synch();	/* flush output */
		if(!dorecover(fd))
			goto not_recovered;
#ifdef WIZARD
		if(!wizard && remember_wiz_mode) wizard = TRUE;
#endif
#ifdef	GTK_GRAPHICS
		if(!strcmp(windowprocs.name, "gtk")){
		    winid id;
		    char buf[4096];
/*JP*/
#ifdef JNETHACK		    
		    sprintf(buf, "ようこそ %s, またNetHackの世界へ！", plname);
#else
		    sprintf(buf, "Hello %s, welcome back to NetHack!", plname);
#endif
		    id = create_nhwindow(NHW_MENU);
		    putstr(id, 0, buf);
		    display_nhwindow(id, TRUE);
		    destroy_nhwindow(id);
		}
		else
#endif
/*JP*/
#ifdef	JNETHACK
		    pline("ようこそ %s, またNetHackの世界へ！", plname);
#else
		    pline("Hello %s, welcome back to NetHack!", plname);
#endif

		check_special_room(FALSE);
		wd_message();

		if (discover || wizard) {
			if(yn("Do you want to keep the save file?") == 'n')
			    (void) delete_savefile();
			else {
			    (void) chmod(SAVEF,FCMASK); /* back to readable */
			    compress(SAVEF);
			}
		}
		flags.move = 0;
	} else {
not_recovered:
		player_selection();

		newgame();
		/* give welcome message before pickup messages */
#ifdef	GTK_GRAPHICS
		if(!strcmp(windowprocs.name, "gtk")){
		    winid id;
		    char buf[4096];
/*JP*/
#ifdef JNETHACK		    
		    sprintf(buf, "ようこそ %s, NetHackの世界へ！", plname);
#else
	   	    sprintf(buf, "Hello %s, welcome to NetHack!", plname);
#endif
		    id = create_nhwindow(NHW_MENU);
		    putstr(id, 0, buf);
		    display_nhwindow(id, TRUE);
		    destroy_nhwindow(id);
		}
		else
#endif
/*JP*/
#ifdef JNETHACK
		    pline("ようこそ %s, NetHackの世界へ！", plname);
#else
		    pline("Hello %s, welcome to NetHack!", plname);
#endif
		wd_message();

		flags.move = 0;
		set_wear();
		pickup(1);
	}

#ifdef	GTK_GRAPHICS
	if(!strcmp(windowprocs.name, "gtk"))
	    GTK_init_nhwindows2();
#endif
	moveloop();
	exit(EXIT_SUCCESS);
	/*NOTREACHED*/
	return(0);
}

static void
process_options(argc, argv)
int argc;
char *argv[];
{
	/*
	 * Process options.
	 */
	while(argc > 1 && argv[1][0] == '-'){
		argv++;
		argc--;
		switch(argv[0][1]){
		case 'D':
#ifdef WIZARD
			{
			  char *user;
			  int uid;
			  struct passwd *pw = (struct passwd *)0;

			  uid = getuid();
			  user = getlogin();
			  if (user) {
			      pw = getpwnam(user);
			      if (pw && (pw->pw_uid != uid)) pw = 0;
			  }
			  if (pw == 0) {
			      user = getenv("USER");
			      if (user) {
				  pw = getpwnam(user);
				  if (pw && (pw->pw_uid != uid)) pw = 0;
			      }
			      if (pw == 0) {
				  pw = getpwuid(uid);
			      }
			  }
			  if (pw && !strcmp(pw->pw_name,WIZARD)) {
			      wizard = TRUE;
			      break;
			  }
/*JP*/
			  if(pw && !strcmp(pw->pw_name, "issei")){
			      wizard = TRUE;
			      break;
			  }
			}
			/* otherwise fall thru to discover */
			wiz_error_flag = TRUE;
#endif
		case 'X':
			discover = TRUE;
			break;
#ifdef NEWS
		case 'n':
			iflags.news = FALSE;
			break;
#endif
		case 'u':
			if(argv[0][2])
			  (void) strncpy(plname, argv[0]+2, sizeof(plname)-1);
			else if(argc > 1) {
			  argc--;
			  argv++;
			  (void) strncpy(plname, argv[0], sizeof(plname)-1);
			} else
				raw_print("Player name expected after -u");
			break;
		case 'I':
		case 'i':
			if (!strncmpi(argv[0]+1, "IBM", 3))
				switch_graphics(IBM_GRAPHICS);
			break;
	    /*  case 'D': */
		case 'd':
			if (!strncmpi(argv[0]+1, "DEC", 3))
				switch_graphics(DEC_GRAPHICS);
			break;
		default:
			/* allow -T for Tourist, etc. */
			(void) strncpy(pl_character, argv[0]+1,
				sizeof(pl_character)-1);

			/* raw_printf("Unknown option: %s", *argv); */
		}
	}

	if(argc > 1)
		locknum = atoi(argv[1]);
#ifdef MAX_NR_OF_PLAYERS
	if(!locknum || locknum > MAX_NR_OF_PLAYERS)
		locknum = MAX_NR_OF_PLAYERS;
#endif
}

#ifdef CHDIR
static void
chdirx(dir, wr)
const char *dir;
boolean wr;
{

# ifdef SECURE
	if(dir					/* User specified directory? */
#  ifdef HACKDIR
	       && strcmp(dir, HACKDIR)		/* and not the default? */
#  endif
		) {
		(void) setgid(getgid());
		(void) setuid(getuid());		/* Ron Wessels */
	}
# endif

# ifdef HACKDIR
	if(dir == (const char *)0)
		dir = HACKDIR;
# endif

	if(dir && chdir(dir) < 0) {
		perror(dir);
		error("Cannot chdir to %s.", dir);
	}

	/* warn the player if we can't write the record file */
	/* perhaps we should also test whether . is writable */
	/* unfortunately the access system-call is worthless */
	if (wr) check_recordfile(dir);
}
#endif /* CHDIR */

static boolean
whoami() {
	/*
	 * Who am i? Algorithm: 1. Use name as specified in NETHACKOPTIONS
	 *			2. Use $USER or $LOGNAME	(if 1. fails)
	 *			3. Use getlogin()		(if 2. fails)
	 * The resulting name is overridden by command line options.
	 * If everything fails, or if the resulting name is some generic
	 * account like "games", "play", "player", "hack" then eventually
	 * we'll ask him.
	 * Note that we trust the user here; it is possible to play under
	 * somebody else's name.
	 */
	register char *s;

	if (*plname) return FALSE;
	if(/* !*plname && */ (s = getenv("USER")))
		(void) strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = getenv("LOGNAME")))
		(void) strncpy(plname, s, sizeof(plname)-1);
	if(!*plname && (s = getlogin()))
		(void) strncpy(plname, s, sizeof(plname)-1);
	return TRUE;
}

#ifdef PORT_HELP
void
port_help()
{
	/*
	 * Display unix-specific help.   Just show contents of the helpfile
	 * named by PORT_HELP.
	 */
	display_file(PORT_HELP, TRUE);
}
#endif

/*JP*/
#ifdef JNETHACK
static void
wd_message()
{
#ifdef WIZARD
	if (wiz_error_flag) {
		pline("「%s」のみがデバッグ(wizard)モードを使用できる．",
# ifndef KR1ED
			WIZARD);
# else
			WIZARD_NAME);
# endif
		pline("かわりに発見モードへ移行する．");
	} else
#endif
	if (discover)
		You("スコアの載らない発見モードで起動した．");
}
#else
static void
wd_message()
{
#ifdef WIZARD
	if (wiz_error_flag) {
		pline("Only user \"%s\" may access debug (wizard) mode.",
# ifndef KR1ED
			WIZARD);
# else
			WIZARD_NAME);
# endif
		pline("Entering discovery mode instead.");
	} else
#endif
	if (discover)
		You("are in non-scoring discovery mode.");
}
#endif
/*unixmain.c*/
