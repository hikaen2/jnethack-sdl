# SDL バックエンドの Windows 対応 可否評価

- 前提: [`SDL-POC-PLAN.md`](SDL-POC-PLAN.md) / [`SDL-POC-RESULT.md`](SDL-POC-RESULT.md)
- 作成日: 2026-07-30
- 状態: **評価のみ。実装は未着手、かつ当面着手しない**

---

## 0. なぜこの文書があるか

`SDL-POC-PLAN.md` §1 は Windows / macOS 対応を明示的に「やらないこと」に入れており、
実証実験は Linux + SDL2 だけで完了した。その後「Windows でも動くようになるか」という
問いが出たため、**コードは書かずに調査だけ行った**結果を記録する。

結論を先に書く。

> **動くようにはできる。障害は事前の想定より小さい。**
> `sdlterm.c` の変更は 100 行前後の `#ifdef` で収まる見込みで、
> 作業量の大半は SDL 側ではなくビルド環境にある。
> ただし Linux 版で先にやるべきことのほうが価値が高いので、**着手は保留する**（§5）。

---

## 1. 追い風になる発見

3.2.3 には既に Win32 コンソールポート（`sys/winnt/nttty.c` + `include/ntconf.h`）があり、
**その構造が SDL バックエンドとほぼ一致している。**
結果として、Linux で苦労した点のいくつかが Windows では最初から解決されている。

| 論点 | Linux での状況 | Windows での状況 |
|---|---|---|
| `putchar` / `puts` のリダイレクト（H1 の要） | `include/sdlterm.h` に自前のマクロを用意した | **`include/wintty.h:234-242` に元からある**。`WIN32CON` 向けに `putchar`/`putc`/`puts` を `xputc`/`xputs` へ飛ばす |
| `msmsg()`（`MICRO` 環境で `putchar` の代わりに使われる 6 箇所） | 該当なし | **`sys/share/pcsys.c:405-416` の WIN32 分岐が既に `xputs()` を呼ぶ** |
| `CM` / `ul_hack` の参照 | 非 NULL にする必要があった（R-2） | `ntconf.h:25` が `NO_TERMS` を定義するので `wintty.c` 側で `#ifndef NO_TERMS` により除外され、**そもそも参照されない** |
| `gettty` / `settty` / `setftty` | `sys/share/unixtty.c` を `#ifdef` で潰した | `sys/winnt/nttty.c:66-90` の実装が **SDL 用に書いたものと実質同一** |

最後の行は特筆に値する。`nttty.c` の実装は次のとおりで、

```c
void gettty() {
	erase_char = '\b';
	kill_char = 21;		/* cntl-U */
	iflags.cbreak = TRUE;
	...
}
void settty(s) const char *s; { end_screen(); if(s) raw_print(s); }
void setftty() { start_screen(); }
```

これは `SDL-POC-RESULT.md` §7 で Linux 向けに書いた `#ifdef SDL_GRAPHICS` 版と
ほぼ字面まで同じである。**「端末を持たない表示系」に必要なものは既に一度書かれていた**
ということで、設計が特殊でないことの裏付けになる。

### 想定する構成

`MICRO` + `NO_TERMS` + `SDL_GRAPHICS` を定義し、**`WIN32CON` は定義しない。**

`WIN32CON` を外すと `win/tty/wintty.c:2303` の `nttty_open()` 呼び出しも消えるので、
スタブを用意する必要がなくなる（この呼び出しは `#if defined(WIN32CON)` の中にある）。
`putchar` のリダイレクトは `WIN32CON` ブロックではなく
`SDL_GRAPHICS` ブロック（Linux と同じもの）が担当する。

この構成なら `wintty.c` / `topl.c` / `getline.c` の無改造（H1）は
**Linux と同様に成立する。**

---

## 2. 実際に必要になる変更

| # | 項目 | 規模 | 備考 |
|---|---|---|---|
| 1 | `SDL_MAIN_HANDLED` + `SDL_SetMainReady()` | 2 行 | `main()` は `sys/share/pcmain.c:89`。SDL.h を include しないので名前の衝突は起きないが、SDL2 に `main` を横取りさせない宣言は要る |
| 2 | `gettty`/`settty`/`setftty`/`tgetch` を `sdlterm.c` へ | ~30 行 | `nttty.c` を置き換えるため。中身は `nttty.c:66-90` とほぼ同じ。`ntconf.h` には `#define tgetch` が無く `nttty.c:158` が関数として定義しているので、Windows では `#define` ではなく関数を出す |
| 3 | `raise(SIGHUP)`（ウィンドウを閉じたとき）の代替 | ~10 行 | **Windows に `SIGHUP` は無い。** PC 系ポートにはそもそも hangup 処理が存在しない（`pcmain.c` / `sys/winnt/*.c` に `SIGHUP` の記述なし）ので、保存処理を直接呼ぶ形にする |
| 4 | `raise(SIGWINCH)`（リサイズ）の代替 | **要設計判断** | 下記 |
| 5 | フォント探索パス | ~10 行 | `font_candidates[]` が Linux のパス直書き。`C:\Windows\Fonts\msgothic.ttc` を先頭に置く。**MS ゴシックは ASCII が漢字のちょうど半角の等幅フォントなので H3 の要件を満たすはず**（未検証） |

### #4 が唯一の厄介な点

`wintty.c` の `winch()` は

```c
#if defined(SIGWINCH) && defined(CLIPPING)
```

の中にあり、Windows では `SIGWINCH` が存在しないためコンパイルされない。
つまり **再レイアウトを依頼する経路が無い。**

（Linux でも同じ罠を踏んだが、あちらは `wintty.c` が `<signal.h>` を
BSD でしか読まないのが原因で、`wintty.h` から include して解決できた。
Windows は「シグナルそのものが無い」ので同じ手は使えない。）

**最初の版では「ウィンドウ固定・リサイズ非対応」で割り切るのが妥当。**
`winch()` 相当を `sdlterm.c` 側で書くと `ttyDisplay` や `wins[]` を直接触ることになり、
「バックエンドは画面だけを持つ」という現在の境界が崩れる。
それは H1 の価値を損なうので、割に合わない。

---

## 3. ビルド環境（作業量の大半はここ）

`sys/winnt/Makefile.nt` は **MS Visual C++ 4.x + `win32.mak` + NMAKE** 前提で
1996 年のまま。現代の環境では使えない。現実的な選択肢は MinGW-w64。

### 見込みは悪くない

- `sys/winnt/winnt.c` は `FindFirstFile` / `GetDiskFreeSpace` / `GetModuleFileName`
  といった素の Win32 API しか使っておらず、**MSVC 固有の部分は
  `sys/winnt/win32api.h` の中で `#if defined(_MSC_VER)` に隔離されている**
- `ntconf.h` が要求する `<dos.h>` `<direct.h>` `<io.h>` `<conio.h>` `<process.h>` は
  MinGW-w64 にすべてある

### 必要になるもの

- `sudo apt install mingw-w64`（未インストール。apt に有り）
- SDL2 / SDL2_ttf の MinGW 版開発ライブラリ。**apt には無い**ので
  libsdl.org の公式 `SDL2-devel-*-mingw.tar.gz` を取得する
- `sys/winnt/Makefile.mingw`（新規、~80 行）

なお **作業機には wine が入っている**（`/usr/bin/wine`）ので、
クロスビルドした `.exe` を Linux 上で起動確認することは可能。
「Windows 実機が無いと検証できない」という制約は無い。

---

## 4. 検証方法

`test/ptydrive.py` は pty ベースなので Windows では動かない。
しかし **A3 の基準側は Linux の tty 版が出したダンプをそのまま流用できる。**

`NETHACK_SEED` を揃えれば同じダンジョンが生成されるので、

1. Linux で `src/nethack.tty` を `test/ptydrive.py` に通してダンプを取る（既存の仕組み）
2. Windows(wine) で `src/nethack.exe` を `NH_SDL_KEYS` + `NH_SDL_DUMP` で走らせる
3. 1 と 2 を diff する

でよい。A4 は `NH_SDL_WIDTHTEST=1` がそのまま動く。

---

## 5. 優先度: 低い

Windows 対応の価値は「JNetHack を Windows で配れる」ことだが、
それは **JNetHack 側の移植が終わってから**考えるべき順序である。

先にやる価値があるもの:

1. **JNetHack への移植**（`SDL-POC-RESULT.md` §12）— 実験の本来の目的
2. フォント設定の永続化（現在は環境変数のみ）
3. タイル描画

この文書は、その順番が回ってきたときに調査をやり直さずに済むように残す。

---

## 付録: 参照した箇所

| ファイル:行 | 内容 |
|---|---|
| `include/wintty.h:234-242` | `NO_TERMS` / `WIN32CON` の `putchar`→`xputc` 再定義ブロック |
| `include/wintty.h:249-254` | 今回追加した `SDL_GRAPHICS` ブロック（上記の真下） |
| `sys/share/pcsys.c:405-416` | `msmsg()` の WIN32 分岐が `xputs()` を呼ぶ |
| `sys/winnt/nttty.c:66-90` | `gettty` / `settty` / `setftty` |
| `sys/winnt/nttty.c:158,304` | `tgetch()` / `get_scr_size()` が関数として定義されている |
| `win/tty/wintty.c:2299-2306` | `nttty_open()` が `WIN32CON` 限定であること |
| `include/ntconf.h:12,24-26` | `TEXTCOLOR` / `MICRO` / `NO_TERMS` / `ASCIIGRAPH` |
| `sys/winnt/winnt.c:78-118` | Win32 API のみを使用 |
| `sys/winnt/win32api.h` | MSVC 固有部の隔離 |
| `sys/share/pcmain.c:89` | `main()` の所在 |
| `sys/winnt/Makefile.nt:1-30` | MSVC 4.x / `win32.mak` 前提であること |
