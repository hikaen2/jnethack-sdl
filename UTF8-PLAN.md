# 内部文字コードの UTF-8 化 計画書

JNetHack の内部文字コードを EUC-JP から UTF-8 へ移行するための計画。

## 0. 決定事項

| 項目 | 決定 | 理由 |
|---|---|---|
| 受理する文字の範囲 | **方針B: 4バイト文字を含む Unicode 全域** | 上限を3バイトに決めた MySQL `utf8mb3` は、後から4バイトへ広げる際にインデックス長制限に当たって大きな負債になった。同じ構図を作らない |
| 書記素クラスタの扱い | **B-1: 幅0の文字はグリッドに置かないが、ゲーム内部文字列には保持する** | 「受理・保存・比較は完全、表示のみ基底文字」。データを失う制約ではないので、後から B-2（セル合成描画）へ進む道を塞がない |
| UTF-8 処理の実装 | **`include/utf8.h` + `japanese/utf8.c` を新設。sheredom/utf8.h を `include/sheredom_utf8.h` に vendoring し、文字列操作のみ委譲** | 下記 §1 参照 |

進捗: **Phase 0 完了**（§8）。Phase 1 以降は未着手。

## 1. `utf8.h` について

このツリーにも NetHack 3.6 本体にも `utf8.h` は存在しない。ただし開発環境には Debian の
`libutf8.h-dev`（sheredom/utf8.h, version 0.0~git20231220.535001e-1）が
`/usr/include/utf8/utf8.h` として入っている。

| 候補 | 評価 |
|---|---|
| **sheredom/utf8.h**（部分的に採用） | 当初 C99 前提と見たが誤り。**本ツリーの `-O2 -Wall -std=gnu89 -fcommon` でクリーンにコンパイル・リンクできることを実測で確認した。** 文字列操作（`utf8len` / `utf8size` / `utf8str` / `utf8cmp` / `utf8rcodepoint`）は使う。**ただしバリデーションには使えない**（§1.1）|
| `<wchar.h>` / `mbrtowc` | ロケール依存。`win/tty/sdlterm.c:444` が「iconv にもロケールにも依存しない」ことを明示的な設計判断として記録しており、これを崩すことになる |
| **自前の `include/utf8.h`**（採用） | 信頼境界（外部から来たバイト列のデコードと桁幅判定）を担当。`sdlterm.c` 内の既存実装は**2箇所ともバグがある**（§4）ので、移植ではなく書き直し |

### 1.1 sheredom/utf8.h をバリデーションに使えない理由（実測）

推測ではなく計測結果:

- **`utf8codepoint()` は検証しない。** `str[1..3]` が継続バイトかを確認せずに読む。バッファ末尾の切れた先行バイト（`"\xE6"` の次が終端）を渡すと**3バイト消費して NUL を読み越し**、U+603F を返した。キーボードと IME のテキストはまさにその形で届くので、実入力に対する overread になる
- **`utf8nvalid()` は継続バイトと overlong は見るが、サロゲートと範囲外を通す。** `ED A0 80`（U+D800）、`F4 90 80 80`（U+110000）、`F7 BF BF BF`（U+1FFFFF）がいずれも accept。入力フィルタとして単体では不十分

したがって `utf8_decode()` は自前。**未検証バイトを見てよいのはこれだけ**とし、終端 NUL を絶対に読み越さない。通過後の文字列に対してのみ vendored の文字列関数を使う。

### 1.2 vendoring と include 方針

`include/sheredom_utf8.h` にそのままコピー（Unlicense / public domain）。MinGW-w64 クロスビルドの
sysroot には `libutf8.h-dev` が無いため、パッケージ依存にはできない。

**`include/utf8.h` は `sheredom_utf8.h` を include しない。** 実測で、include した翻訳単位は
**オブジェクトコードが 34KB、コンパイル時間が 0.26 秒増える**（約40個の関数を宣言ではなく
weak シンボル定義として展開するため）。リンク後は1コピーに畳まれるのでバイナリは太らないが、
Phase 1 で `hack.h` に引き上げる形にすると 250 余りのソース全部がこの代償を払う。
`utf8len()` 等が実際に要るファイルだけが `sheredom_utf8.h` を明示的に include する。

## 2. 現状

内部コードは EUC-JP。`japanese/jlib.c:31` がソース自身のリテラルのバイトを見て EUC か SJIS かを判定している:

```c
#define IC ((unsigned char)("漢"[0])==0x8a)
```

UTF-8 化すると常に 0（EUC 扱い）になるため、この仕組み自体が破綻する。最初に手を付ける箇所。

### 作業規模の実測

| 対象 | 規模 |
|---|---|
| EUC リテラルを含む `.c` | 99 ファイル / 約 62,850 個の2バイト列 |
| `dat/` テキスト | `nhdat`(102,654) `data.base`(43,699) `data`(43,485) `quest.txt`(34,171) `quest.dat`(28,004) `jrumors`(15,577) `jrumors.fal`(9,848) `jrumors.tru`(8,237) ほか `j*` ヘルプ類 |
| `doc/jGuidebook.txt` | 24,932 |
| 生成物 | `include/jdata.h`(6,934) ← `dat/jtrnsobj.dat`(4,646) + `dat/jtrnsmon.dat`(2,324) から `makedefs` が生成 |
| **バイト長ベタ書きの危険箇所** | **85 行** |
| **日本語APIの呼び出し箇所** | **98 箇所 / 34 ファイル**（下表） |

バイト長ベタ書きの内訳（`strncmp(bp, "の", 2)` のような「日本語1文字＝2バイト」前提。機械変換では直らない）:

| ファイル | 箇所数 |
|---|---|
| `src/objnam.c` | 35 |
| `japanese/jconj.c` | 13 |
| `japanese/jlib.c` | 10 |
| `src/topten.c` | 7 |
| `src/invent.c` | 4 |
| `src/trap.c` | 2 |
| `src/mon.c` | 1 |

### 2.1 計測に関する注意 ― `grep` は使えない

この環境の `grep` は **ugrep 7.5.0** で、**不正な UTF-8 を含むファイルを丸ごと黙ってスキップする**。
EUC-JP のソースはすべてこれに当たるため、`grep` の結果は無言でゼロ件になる。ASCII だけの
パターンでも同じで、`grep -c doengrave src/engrave.c` すら 0 件を返す。`LC_ALL=C` では
回避できない。

**必ず `grep -U` を付けるか、perl / `rg --no-require-git -a` を使うこと。**

この計画書の初版はこの罠にかかっており、`is_kanji1`/`is_kanji2` や `jconj` の呼び出し箇所を
「ほとんど無い」と誤って記録していた。下の表は `grep -U` と perl で取り直した値である。

### 2.2 日本語APIの呼び出し箇所（`grep -U` 実測）

| 関数 | 箇所数 | 主な呼び出し元 |
|---|---|---|
| `jconj_adj` | 32 | `seffects`(8), `chwepon`(3), `dodip`(3), `movemon`(2), `pleased`(2) ほか |
| `jconj` | 29 | `doengrave`(3), `getobj`(3), `m_dowear_type`(3), `dotrap`(2) ほか |
| `is_kanji2` | 9 | `engrave.c`(3), `do_name.c`(2), `wintty.c`, `getline.c`, `topten.c` |
| `isspace_8` | 6 | `engrave.c`(2), `files.c`(2), `options.c`(2) |
| `is_kanji1` | 5 | `engrave.c`(2), `botl.c`, `topten.c` |
| `split_japanese` | 5 | `topl.c`, `topten.c`, `winmesg.c`, `wintext.c` |
| `str2ic` | 5 | `files.c`, `options.c`, `wintty.c`, `getline.c` |
| `jrndm_replace` | 2 | `engrave.c`(2) — 刻文の劣化 |
| `jpast` | 2 | `on_msg`, `off_msg` |
| `setkcode` | 2 | `options.c`, `unixmain.c` |
| `jcan` | 1 | `ggetobj` |
| `jcannot` | 0 | 宣言のみ。未使用 |

合計 98 箇所、34 ファイル。最も集中するのは `src/engrave.c`（5種類のAPI）。

**`jconj` 系だけで 64 箇所ある。** 初版が「未使用かもしれない」と読み違えていた部分で、
Phase 1 で最も慎重を要するのはここという判断は変わらない（むしろ強まった）。

## 3. 既存リテラルは3バイト上限が保証される

`include/jis0208.h:21`:

```c
static const unsigned short jis0208_to_ucs[8836] = {
```

`unsigned short` なので、JIS X 0208 由来のコードポイントは必ず BMP 内（≤ U+FFFF）。半角カナの SS2 経路も U+FF61–U+FF9F。したがって既存ツリーを `iconv` 変換した結果は**漢字がすべて3バイト**で、文字列長は約1.5倍にとどまる。

4バイトが問題になるのは**入力経路のみ**（§4）。

## 4. 入力経路 ― 現状の EUC ボトルネックと、その消失

現在4バイト文字が入ってこないのは、EUC が関門になっているため。`sdl_queue_text()` (`sdlterm.c:925`) が IME の確定文字列を `sdl_ucs_to_euc()` に通し、JIS X 0208 にない文字は 0 が返って `tty_nhbell()` で捨てられる。

内部を UTF-8 にすると**この関門ごと消える**。𠮟(U+20B9F)、𩸽(U+29E3D)、絵文字が名前・刻文・果物名から素通しで入る。そして既存コードは非BMPを正しく扱えない。

### 4.1 デコーダのバグ（`sdlterm.c:947-958`）

2バイトと3バイトの分岐しかなく、4バイト列を明示的に捨てている:

```c
	} else {
	    /* malformed or beyond the BMP: skip the whole sequence */
	    p++;
	    while (*p && (*p & 0xC0) == 0x80) p++;
	    continue;
	}
```

### 4.2 エンコーダのバグ（`sdlterm.c:1889-1898`）

分岐が3本で、最後の `else` が `ch < 0x10000` を暗黙に仮定している:

```c
	    } else {
		(void) fputc((int) (0xE0 | (ch >> 12)), fp);
```

`ch = 0x20B9F` だと `ch >> 12` が `0x20` になり `0xE0|0x20 = 0xE0` と上位ビットが落ちて、無関係な文字を出力する。

**この2箇所をそのまま `utf8.h` に昇格させると、バグを共有ヘッダに固定化することになる。移植ではなく書き直す。**

### 4.3 幅計算だけが非BMP想定済み

`sdl_cp_width()` (`sdlterm.c:409`) は `0x20000L..0x3FFFDL` と絵文字を既に2桁として扱っている。`CELL.ch` も `long` (`sdlterm.c:101`) なので格納自体は問題ない（MinGW の32bit long でも U+10FFFF は入る）。**幅計算だけが先行して非BMP対応済み**という不整合がある。

ただし幅0の扱いが誤っている。冒頭が

```c
    if (cp < 0x1100L) return 1;
```

なので**結合文字（U+0300–U+036F）に幅1を返す**。異体字セレクタ基本域（U+FE00–FE0F）も範囲表にないため 1 が返る。いずれも正しくは 0。方針Bでは必ず到達するので修正必須。

## 5. フォント層は既に非BMP対応済み

`sdl_glyph()` (`sdlterm.c:518-551`) が使っているのは SDL_ttf の `*32` 系 API:

```c
    if (f && TTF_GlyphIsProvided32(f, (Uint32) ch)) {
	surf = TTF_RenderGlyph32_Blended(f, (Uint32) ch, white);
```

`Uint32` のコードポイントを直接取るのでサロゲートペアを経由せず、U+10FFFF まで素で通る。グリフ収録判定も入っており、未収録なら `g->tex = 0` のまま返ってセルは空白として描画される。クラッシュも化けもしない。

**ただし空白で落ちるのは方針Bでは不都合。** 方針Aなら入力時に弾いていたので「出ない文字」は存在しなかったが、Bでは「受理したが描けない文字」が常態になる。名前に入れた絵文字が黙って消えたように見えるのは最悪なので、未収録グリフは U+FFFD かハッチ矩形を `sdl_draw_box()` で描いて**可視化**する。

フォント候補列（`sdlterm.c:68-83`）のフォールバック連鎖化は、その後の改善で構わない。

## 6. `PL_NSIZ` の二重用途（注意）

方針Bではバッファを拡大する必要があるが、`PL_NSIZ` (`include/global.h:319`) は**2つの無関係な役目**を持つ。

想定どおりの用途:

```c
include/decl.h:168:   E NEARDATA char plname[PL_NSIZ];
include/eshk.h:40-41: char customer[PL_NSIZ]; char shknam[PL_NSIZ];
src/files.c:47,50,55,58:  ロック/セーブファイル名長の派生元
```

問題は `src/monst.c`:

```c
src/monst.c:100:   SIZ(WT_HUMAN, 400, PL_NSIZ, MS_HUMANOID, MZ_HUMAN),   /* player */
src/monst.c:2125:  SIZ(WT_HUMAN, 0,   PL_NSIZ, MS_SILENT,   MZ_HUMAN),   /* ghost */
```

`SIZ(wt,nut,pxl,snd,siz)` (`src/monst.c:47`) の第3引数 `pxl` は **`struct monst` の後ろに確保する拡張領域のバイト数**（`mxlth`）。PL_NSIZ を上げると全ゴーストのアロケーションが増え、`mxlth` はセーブ/ボーンに書かれる値なので**ボーンファイルの形式が変わる**。

Phase 4 でどのみち版を上げるため実害はないが、「プレイヤ名バッファを広げたつもりがボーン互換を壊す」という繋がりは把握して進めること。

### 推奨値

| 定数 | 現在 | 変更後 | 根拠 |
|---|---|---|---|
| `BUFSZ` | 256 | 512 | 最悪ケース2倍（4バイト/文字） |
| `QBUFSZ` | 128 | 256 | 同上 |
| `PL_NSIZ` | 32 | 128 | 4バイト文字で32文字 |

## 7. 書記素クラスタ ― B-1 の具体化

方針Aなら入力フィルタが一緒に排除してくれた問題が、Bでは正面から来る。**1文字が複数コードポイント**になるケース:

- 異体字セレクタ（IVS, U+E0100–U+E01EF）: 4バイト、幅0
- 結合文字（U+0300–U+036F）: 幅0
- 絵文字 ZWJ 連結（👨‍👩‍👧）: 複数の非BMP文字＋U+200D で1つの絵文字

`CELL.ch` は `long` 単体（`sdlterm.c:101`）なので、1セル＝1コードポイントの構造。

**採用する B-1:**

- 幅0の文字は**グリッドに置かない**（セルを消費しない）
- ただし**ゲーム内部の文字列には保持する**。保存・比較・往復はすべて完全
- 表示上は基底文字のみになる。異体字の書き分けは画面から失われるが、データは失われない
- `utf8_prev()`（バックスペース）は**幅0の文字を基底文字までまとめて飛ばす**。ここは省略不可

不採用の B-2（`CELL` にコードポイント配列を持たせ `TTF_RenderUTF8_Blended` で合成描画）は、セル構造・グリフキャッシュ・ダンプ形式すべてに波及するため、必要になった時点で別途検討する。B-1 はその道を塞がない。

## 8. フェーズ

順序が重要。**リテラルの一括変換は最後**。先にコードを符号化非依存にしないと、変換した瞬間に全部壊れて切り分けが不能になる。

### Phase 0 — 基盤（リテラルは EUC のまま）— **完了**

1. ✅ `include/sheredom_utf8.h` を vendoring（無改変、Unlicense）
2. ✅ `include/utf8.h` + `japanese/utf8.c` を新設
   - `utf8_decode(const char *, long *cp)` → 消費バイト数。**4分岐**。overlong / サロゲート域(U+D800–DFFF) / U+10FFFF 超を拒否し、不正入力は U+FFFD ＋1バイト前進で必ず前進する。**終端 NUL を読み越さない**
   - `utf8_encode(long cp, char *buf, int n)` → 書いたバイト数。**4分岐**。バッファ不足は 0 を返し、切り詰めない
   - `utf8_cpwidth(long cp)` → 0/1/2。幅2は `sdl_cp_width` から継承、**幅0の範囲は新規**。U+3099/309A（結合濁点・半濁点）が幅2のかなブロック内にあるため、**幅0の判定を先に行う**
   - `utf8_prev(const char *base, const char *p)` → 後退。**幅0の連なりを基底文字までまとめて飛ばす**（B-1 のバックスペース要件）
   - `utf8_seqlen()`, `utf8_colwidth()`, `utf8_valid()`
3. ✅ `sdlterm.c` の2つのバグを新 API で置換
   - `sdl_queue_text()`: overread と、非BMPの無音破棄を修正。EUC ボトルネック自体は Phase 2 まで残る
   - `sdl_dump_grid()`: 3分岐エンコーダの上位ビット落ちを修正
4. ✅ `sdl_glyph()` の未収録グリフを中空の矩形で可視化（方針Bでは「受理したが描けない文字」が常態になるため）
5. ✅ `test/utf8test.c` + `test/utf8test.sh` を新設。**98 チェック、0 失敗**
   - U+0000–U+10FFFF の**全数往復テスト**
   - sheredom 版との相互照合。一致すべき箇所は一致を、意図的に違う箇所（overread、サロゲート受理）は**違うことを**表明する。将来 vendored ファイルを更新したときに前提の変化がここで露見する
6. ✅ ビルド配線: `sys/unix/Makefile.src` の `JSRC`/`JOBJ` と依存規則
7. ✅ 回帰確認: tty / SDL / MinGW の3ターゲットがビルド、`compare.sh` `wincompare.sh` `winjname.sh` と `NH_SDL_WIDTHTEST`（6ケース）が全通過

### Phase 1 — 符号化非依存化（まだ EUC）

5. `japanese/jlib.c` の書き換え。`is_kanji1`/`is_kanji2` はバイト位置を問う API で UTF-8 では意味を持たない。**文字境界を問う API に置き換え**、呼び出し側を追随:
   - `getline.c` のバックスペース処理（`sdlterm.c:932` のコメントが指す箇所）
   - `win/X11/winmesg.c:281` の `split_japanese`
   - `include/extern.h:2116-2121` の宣言
6. `split_japanese` を「バイト20個先まで探索」から**コードポイント単位の探索**へ。禁則対象文字（`、` `。` 等の `strncmp(...,2)`）はコードポイント比較に
7. `japanese/jconj.c:40` の `hira_tab` を `{0xa4, 0xa2}` のバイト対から**コードポイント配列 `{0x3042, ...}`** に。`jconjsub` が `tmp+(len-2)` で末尾2バイトを直接叩く構造なので、「末尾1文字を差し替える」形へ書き直し（13箇所）
8. `src/objnam.c` の35箇所。日本語数詞・助数詞の判定。長さをリテラル依存にする（`sizeof("一")-1`）か、コードポイント比較に。`sizeof` 方式なら Phase 3 の変換で自動追従するので安全。`topten.c`/`invent.c`/`trap.c`/`mon.c` の計14箇所も同様

**回帰テストを先に用意すること。** `jconj.c` の活用形処理だけは純粋な機械変換が効かず日本語文法の理解が要る。動詞63語 × 活用形の出力を Phase 1 の前後で全パターン比較する。

### Phase 2 — 入出力の境界

9. `japanese/jlib.c:31` の `IC` マクロを廃止し、内部コードを UTF-8 固定に。`output_kcode`/`input_kcode` は「外部との変換先」の意味に純化
10. `sdl_queue_text()` は**変換ではなく検証のみ**に。不正バイト列だけ弾き、コードポイントの範囲では絞らない（方針B）
11. tty(termcap) 経路: UTF-8 端末は素通し。EUC/SJIS/JIS 出力オプションは `jis0208.h` の逆引きで実装。`sdlterm.c:903-920` の線形探索版は8836要素を毎文字なめるので**ソート済み逆引きテーブルに置換**。非BMP文字は表現できないので**代替文字に落として出力**（無音で消さない）
12. `src/files.c:1295` の `str2ic`（config 読み込み）、`isspace_8`（`files.c:1057,1073`）を確認

### Phase 3 — リテラル一括変換

13. `iconv -f EUC-JP -t UTF-8` を `.c` / `.h` / `dat/` テキスト / `doc/` に適用。**1コミットで機械的に**
14. `include/jdata.h` は生成物。`dat/jtrnsobj.dat` / `dat/jtrnsmon.dat` を変換して `makedefs` で再生成（`sys/unix/Makefile.src:512`）
15. `dat/nhdat`（dlb アーカイブ）を再生成。`.lev` は `lev_comp` から再生成

### Phase 4 — 互換性とビルド

16. `BUFSZ` / `QBUFSZ` / `PL_NSIZ` 拡大（§6）
17. **セーブ/ボーン/record/logfile の非互換**。プレイヤ名・ペット名・店主名・刻文が EUC バイトで保存されている。`include/patchlevel.h` の版を上げて既存セーブを弾く。読み込み時変換は `jrndm_replace` の刻文劣化と絡んで厄介なので採らない
18. `topten.c` / `record` / `xlogfile` のフィールド区切りと長さ制限を見直し
19. `sys/unix/Makefile.src:65-66` の `JSRC`/`JOBJ` に `utf8.c` を追加。MinGW クロスビルド側（`SDL-WINDOWS.md` の系）も同様

### Phase 5 — 検証

20. `test/compare.sh` / `test/wincompare.sh` で tty と SDL の画面一致を確認
21. `test/winjname.sh` に**非BMP文字を含むプレイヤ名**のケースを追加。**これが方針Bの受け入れ条件そのもの**
22. `sdl_width_test`（`NH_SDL_WIDTHTEST`, `sdlterm.c:1940-2130`）の case 5「EUC→Unicode→EUC 往復」を「UTF-8→コードポイント→UTF-8 往復」に書き換え。非BMP と幅0のケースを追加

## 9. リスク

| リスク | 対策 |
|---|---|
| **バッファ溢れ**（最大の懸念）。漢字が2→3バイト、入力経由で最悪4バイト。JNetHack のメッセージは元々長く、`Sprintf` 系で切り詰めが起きる | Phase 3 と同時に §6 の定数拡大を入れる |
| `jconj.c` の活用形処理。機械変換が効かない | Phase 1 の前に全パターン回帰テストを用意 |
| `PL_NSIZ` 拡大がボーン形式を変える | Phase 4 の版上げに含める（§6） |
| 未収録グリフが無音で消える | Phase 0-3 の可視化で対処（§5） |
