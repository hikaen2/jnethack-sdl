#!/usr/bin/env python3
"""Drive the SDL build's real window with genuine X key events.

    ./test/xdrive.py --keys 'n V y SPACE SPACE i' -- src/jnethack.sdl -u poc
    ./test/xdrive.py --keys 'n V y SPACE SPACE M-n' -- src/jnethack.sdl -u poc
    ./test/xdrive.py --keys 'n V y SPACE SPACE' --close -- src/jnethack.sdl -u poc

test/compare.sh fills the key queue directly through NH_SDL_KEYS, which is
deliberately headless but therefore never exercises SDL's own event path.
This sends real XTEST key events instead, so SDL_TEXTINPUT, SDL_KEYDOWN and
the arrow-key translation are all actually used.

Needs a running X display and python3-xlib.  Writes a screenshot at the end
if --shot is given (via the game's own NH_SDL_SHOT, so it is the renderer's
output and not a compositor's idea of the window).

--close ends by asking the window manager to close the window instead of
terminating the process, and reports the exit status.  That is the only way
to reach the SDL_QUIT path: on Unix sdl_pump() answers it with
raise(SIGHUP), and on Windows -- which has no SIGHUP and nothing to install
a handler on -- with a direct call to hangup().  Both have to save the game.
--cwd runs the command from another directory, which the Windows binary
needs because it takes HACKDIR from the .exe's own location.
"""

import argparse
import os
import subprocess
import sys
import time

from Xlib import X, XK, display
from Xlib.ext import xtest
from Xlib.protocol import event

NAMED = {
    "SPACE": "space",
    "ESC": "Escape",
    "RET": "Return",
    "TAB": "Tab",
    "BS": "BackSpace",
    "LEFT": "Left",
    "RIGHT": "Right",
    "UP": "Up",
    "DOWN": "Down",
}


MODS = {"shift": "Shift_L", "ctrl": "Control_L", "alt": "Alt_L"}

# Characters that live on the shifted level of a key.  Naming their keysym
# alone is not enough: XTEST sends a keycode, and the server reads the
# modifiers that are actually held, so an unshifted "question" arrives as
# the "/" it shares a key with.
SHIFTED = {"?": "slash", "#": "3", "!": "1", "$": "4", "*": "8", "+": "equal"}


def tokens(spec):
    """'n V ^x M-n M-?' -> (keysym name, list of modifiers) pairs."""
    for tok in spec.split():
        mods = []
        # M- is meta.  src/cmd.c's meta commands are M(c) == 0x80|c, and
        # win/tty/sdlterm.c builds that byte out of the Alt modifier.
        while len(tok) > 2 and tok[:2] == "M-":
            mods.append("alt")
            tok = tok[2:]
        if tok in NAMED:
            yield NAMED[tok], mods
        elif len(tok) == 2 and tok[0] == "^":
            yield tok[1].lower(), mods + ["ctrl"]
        else:
            for ch in tok:
                if ch.isupper():
                    yield ch.lower(), mods + ["shift"]
                elif ch in SHIFTED:
                    yield SHIFTED[ch], mods + ["shift"]
                else:
                    yield {" ": "space"}.get(ch, ch), list(mods)


def descends_from(pid, ancestor):
    """Is `pid` the process `ancestor`, or one of its descendants?

    wine does not put its own pid on the window: the .exe runs as a child
    of the launcher, so the X client is a descendant rather than the
    process Popen returned.
    """
    for _ in range(64):
        if pid == ancestor:
            return True
        if pid is None or pid <= 1:
            return False
        try:
            with open("/proc/%d/stat" % pid) as f:
                stat = f.read()
            # comm sits in parentheses and may contain spaces; ppid is the
            # second field after it.
            pid = int(stat[stat.rindex(")") + 2:].split()[1])
        except Exception:
            return False
    return False


def same_cwd(pid, want):
    """Does `pid` run in the directory the game was started in?

    Needed for wine: the .exe ends up reparented away from the launcher,
    so process ancestry does not identify it, but its working directory
    is the playdir this run was given and nothing else's.
    """
    try:
        return os.path.realpath("/proc/%d/cwd" % pid) == want
    except Exception:
        return False


def find_window(dpy, pid_title, pid=None, cwd=None):
    """Locate the game's window by name, from the root downwards.

    The title alone is not enough: the terminal the build was started from
    often has the game's name in its own title, and an abandoned window
    from an earlier run answers to it too.  When _NET_WM_PID is available
    it has to belong to the process just started.
    """
    net_wm_pid = dpy.intern_atom("_NET_WM_PID")

    def owner(win):
        try:
            prop = win.get_full_property(net_wm_pid, X.AnyPropertyType)
        except Exception:
            return None
        return prop.value[0] if prop else None

    def walk(win):
        try:
            name = win.get_wm_name()
        except Exception:
            name = None
        if name and pid_title in name:
            who = owner(win)
            if (pid is None
                    or descends_from(who, pid)
                    or (cwd is not None and same_cwd(who, cwd))):
                return win
        try:
            children = win.query_tree().children
        except Exception:
            return None
        for c in children:
            found = walk(c)
            if found:
                return found
        return None

    return walk(dpy.screen().root)


def send(dpy, win, keyname, mods):
    keysym = XK.string_to_keysym(keyname)
    if keysym == 0:
        print("xdrive: no keysym for %r" % keyname, file=sys.stderr)
        return
    code = dpy.keysym_to_keycode(keysym)
    mcodes = [dpy.keysym_to_keycode(XK.string_to_keysym(MODS[m]))
              for m in mods]
    for mcode in mcodes:
        xtest.fake_input(dpy, X.KeyPress, mcode)
    xtest.fake_input(dpy, X.KeyPress, code)
    xtest.fake_input(dpy, X.KeyRelease, code)
    for mcode in reversed(mcodes):
        xtest.fake_input(dpy, X.KeyRelease, mcode)
    dpy.sync()


def close(dpy, win):
    """Ask the window manager to close the window; SDL reports SDL_QUIT."""
    wm_protocols = dpy.intern_atom("WM_PROTOCOLS")
    wm_delete = dpy.intern_atom("WM_DELETE_WINDOW")
    msg = event.ClientMessage(window=win, client_type=wm_protocols,
                              data=(32, [wm_delete, X.CurrentTime, 0, 0, 0]))
    win.send_event(msg)
    dpy.sync()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default="")
    ap.add_argument("--shot")
    # The exact title win/tty/sdlterm.c gives the window.  A looser match
    # such as "JNetHack" also finds the terminal the build was started
    # from, whose title tends to contain the same word.
    ap.add_argument("--title", default="JNetHack (SDL)")
    ap.add_argument("--delay", type=float, default=0.25)
    ap.add_argument("--cwd")
    ap.add_argument("--close", action="store_true",
                    help="close the window via the WM instead of killing the "
                         "process, and wait for it to exit")
    ap.add_argument("--wait", type=float, default=60.0,
                    help="seconds to allow for the exit after --close")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    cmd = args.cmd[1:] if args.cmd and args.cmd[0] == "--" else args.cmd
    if not cmd:
        ap.error("no command given")

    env = dict(os.environ)
    if args.shot:
        env["NH_SDL_SHOT"] = args.shot

    proc = subprocess.Popen(cmd, env=env, cwd=args.cwd)
    dpy = display.Display()

    want_cwd = os.path.realpath(args.cwd or os.getcwd())
    win = None
    for _ in range(240):
        win = find_window(dpy, args.title, proc.pid, want_cwd)
        if win:
            break
        if proc.poll() is not None:
            sys.exit("xdrive: the game exited before a window appeared")
        time.sleep(0.25)
    if not win:
        proc.kill()
        sys.exit("xdrive: never found a window called %r" % args.title)

    # The window may be mapped but not yet viewable, in which case
    # SetInputFocus is a BadMatch; retry until it takes.
    for _ in range(40):
        try:
            win.configure(stack_mode=X.Above)
            win.set_input_focus(X.RevertToParent, X.CurrentTime)
            dpy.sync()
            break
        except Exception:
            time.sleep(0.25)
    time.sleep(0.5)

    for keyname, mods in tokens(args.keys):
        send(dpy, win, keyname, mods)
        time.sleep(args.delay)

    time.sleep(0.5)

    if args.close:
        close(dpy, win)
        try:
            rc = proc.wait(timeout=args.wait)
        except subprocess.TimeoutExpired:
            proc.kill()
            sys.exit("xdrive: still running %gs after WM_DELETE_WINDOW"
                     % args.wait)
        print("xdrive: closed, exit %d" % rc)
        return

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("xdrive: done")


if __name__ == "__main__":
    main()
