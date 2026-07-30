#!/usr/bin/env python3
"""Drive a tty NetHack under a pty and dump the resulting 80x24 text grid.

Used by SDL-POC-PLAN.md §7 A3 to obtain the reference screen that the SDL
backend's NH_SDL_DUMP output is diffed against.

    ./ptydrive.py --keys 'n V y' --dump out.txt -- src/nethack -u poc

Keys are given as space-separated tokens; see parse_keys() for the accepted
escapes.  The driver is deliberately dumb: it writes a key, waits for output
to go quiet, and repeats.  That is enough for NetHack, which never emits
anything without being asked.
"""

import argparse
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

import pyte

COLS, ROWS = 80, 24

# Tokens accepted in --keys, beyond single literal characters.
NAMED_KEYS = {
    "SPACE": " ",
    "ESC": "\033",
    "RET": "\r",
    "NL": "\n",
    "TAB": "\t",
    "BS": "\b",
}


def parse_keys(spec):
    """'n V y ^R SPACE' -> ['n', 'V', 'y', '\\x12', ' ']"""
    out = []
    for tok in spec.split():
        if tok in NAMED_KEYS:
            out.append(NAMED_KEYS[tok])
        elif len(tok) == 2 and tok[0] == "^":
            out.append(chr(ord(tok[1].upper()) & 0x1F))
        elif tok.startswith("\\x"):
            out.append(chr(int(tok[2:], 16)))
        else:
            out.extend(tok)
    return out


def set_winsize(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def drain(fd, stream, quiet_for=0.35, hard_limit=10.0, charset="latin-1"):
    """Feed the pty's output into the emulator until it stays quiet."""
    deadline = time.time() + hard_limit
    last = time.time()
    while time.time() < deadline:
        timeout = quiet_for - (time.time() - last)
        if timeout <= 0:
            return True
        r, _, _ = select.select([fd], [], [], timeout)
        if not r:
            return True
        try:
            data = os.read(fd, 65536)
        except OSError as e:
            if e.errno == errno.EIO:      # child exited
                return False
            raise
        if not data:
            return False
        stream.feed(data.decode(charset, "replace"))
        last = time.time()
    return True


def render(screen):
    return "\n".join(line.rstrip() for line in screen.display)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", default="", help="space-separated key tokens")
    ap.add_argument("--dump", help="write the final grid here (default: stdout)")
    ap.add_argument("--trace", help="write the grid after every key into this dir")
    ap.add_argument("--env", action="append", default=[],
                    help="NAME=VALUE to add to the child environment")
    ap.add_argument("--settle", type=float, default=0.35,
                    help="seconds of silence that count as 'done'")
    ap.add_argument("--charset", default="latin-1",
                    help="how to decode the pty stream; use cp437 when the "
                         "game is running with IBMgraphics")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    cmd = args.cmd[1:] if args.cmd and args.cmd[0] == "--" else args.cmd
    if not cmd:
        ap.error("no command given")

    env = dict(os.environ, TERM="xterm", COLUMNS=str(COLS), LINES=str(ROWS))
    for kv in args.env:
        name, _, value = kv.partition("=")
        env[name] = value

    pid, fd = pty.fork()
    if pid == 0:
        os.execvpe(cmd[0], cmd, env)
        os._exit(127)
    set_winsize(fd, ROWS, COLS)

    screen = pyte.Screen(COLS, ROWS)
    stream = pyte.Stream(screen)
    # pyte ignores ESC(0 and SO/SI while use_utf8 is set, so DECgraphics
    # walls would arrive as the bare letters lqkxmj.  Decoding is already
    # done in drain(), so clearing it only affects charset handling.
    stream.use_utf8 = False

    if args.trace:
        os.makedirs(args.trace, exist_ok=True)

    alive = drain(fd, stream, args.settle, charset=args.charset)
    for i, key in enumerate(parse_keys(args.keys)):
        if not alive:
            break
        try:
            os.write(fd, key.encode("latin-1"))
        except OSError:
            break
        alive = drain(fd, stream, args.settle, charset=args.charset)
        if args.trace:
            with open(os.path.join(args.trace, "%03d.txt" % i), "w") as f:
                f.write(render(screen) + "\n")

    text = render(screen) + "\n"
    if args.dump:
        with open(args.dump, "w") as f:
            f.write(text)
    else:
        sys.stdout.write(text)

    try:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    except OSError:
        pass
    os.close(fd)


if __name__ == "__main__":
    main()
