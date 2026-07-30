#!/usr/bin/env python3
"""Drive the SDL build's real window with genuine X key events.

    ./test/xdrive.py --keys 'n V y SPACE SPACE i' -- src/jnethack.sdl -u poc

test/compare.sh fills the key queue directly through NH_SDL_KEYS, which is
deliberately headless but therefore never exercises SDL's own event path.
This sends real XTEST key events instead, so SDL_TEXTINPUT, SDL_KEYDOWN and
the arrow-key translation are all actually used.

Needs a running X display and python3-xlib.  Writes a screenshot at the end
if --shot is given (via the game's own NH_SDL_SHOT, so it is the renderer's
output and not a compositor's idea of the window).
"""

import argparse
import os
import subprocess
import sys
import time

from Xlib import X, XK, display
from Xlib.ext import xtest

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


def tokens(spec):
    for tok in spec.split():
        if tok in NAMED:
            yield NAMED[tok], False
        elif len(tok) == 2 and tok[0] == "^":
            yield tok[1].lower(), "ctrl"
        else:
            for ch in tok:
                if ch.isupper():
                    yield ch.lower(), "shift"
                else:
                    yield {" ": "space", "?": "question",
                           "#": "numbersign"}.get(ch, ch), False


def find_window(dpy, pid_title):
    """Locate the game's window by name, from the root downwards."""
    def walk(win):
        try:
            name = win.get_wm_name()
        except Exception:
            name = None
        if name and pid_title in name:
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


def send(dpy, win, keyname, mod):
    keysym = XK.string_to_keysym(keyname)
    if keysym == 0:
        print("xdrive: no keysym for %r" % keyname, file=sys.stderr)
        return
    code = dpy.keysym_to_keycode(keysym)
    mods = {"shift": "Shift_L", "ctrl": "Control_L"}
    if mod:
        mcode = dpy.keysym_to_keycode(XK.string_to_keysym(mods[mod]))
        xtest.fake_input(dpy, X.KeyPress, mcode)
    xtest.fake_input(dpy, X.KeyPress, code)
    xtest.fake_input(dpy, X.KeyRelease, code)
    if mod:
        xtest.fake_input(dpy, X.KeyRelease, mcode)
    dpy.sync()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default="")
    ap.add_argument("--shot")
    ap.add_argument("--title", default="JNetHack")
    ap.add_argument("--delay", type=float, default=0.25)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    cmd = args.cmd[1:] if args.cmd and args.cmd[0] == "--" else args.cmd
    if not cmd:
        ap.error("no command given")

    env = dict(os.environ)
    if args.shot:
        env["NH_SDL_SHOT"] = args.shot

    proc = subprocess.Popen(cmd, env=env)
    dpy = display.Display()

    win = None
    for _ in range(60):
        win = find_window(dpy, args.title)
        if win:
            break
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

    for keyname, mod in tokens(args.keys):
        send(dpy, win, keyname, mod)
        time.sleep(args.delay)

    time.sleep(0.5)
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("xdrive: done")


if __name__ == "__main__":
    main()
