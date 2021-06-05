## 1.1: RPi OSの導入、ベアメタルで"Hello, World!"

小さなベアメタルの"Hello, World"アプリケーションを書くことにより、OS開発の旅を始めようと
思います。すでに、[前提条件](../Prerequisites.md)に目を通し、すべての準備ができている
と思います。もしそうでなければ、今すぐしてください。

先に進む前に、簡単な命名規約を定めたいと思います。READMEファイルを見るとチュートリアル
全体が複数のレッスンに分かれていることがわかるでしょう。各レッスンは、私が「チャプタ」と
呼ぶ個々のファイルで構成されています（今あなたが読んでいるのはレッスン1のチャプタ1.1です）。
チャプタはさらに見出しを持つ「セクション」に分かれています。この命名規約により、この
チュートリアルの様々な部分を参照することができます。

もうひとつ注目していただきたいのはこのチュートリアルにはソースコードのサンプルが数多く
含まれていることです。通常、私は完全なコードブロックを提供することで説明を始め、次に、
それを一行ずつ説明していきます。

### プロジェクトの構成

各レッスンのソースコードは同じ構成になっています。このレッスンのソースコードは
[ここ](https://github.com/s-matyukevich/raspberry-pi-os/tree/master/src/lesson01)にあります。
このフォルダの主な構成要素を簡単に説明しましょう。

1. **Makefile** カーネルのビルドには[make](http://www.math.tau.ac.il/~danha/courses/software1/make-intro.html)ユーティリティーを
   使用します。`make`の動作はMakefileで構成されます。Makefileにはソースコードをコンパイル・
   リンクする方法に関する命令が書かれています。
2. **build.sh または build.bat** これらのファイルはDockerを使ってカーネルをビルドする場合に
   必要です。Dockerを使うとラップトップにmakeユーティリティーやコンパイラツールチェーンを
   インストールする必要がありません。
3. **src** すべてのソースコードを含むフォルダです。
4. **include** すべてのヘッダーファイルはここに置かれます。

### Makefile

では、プロジェクトのMakefileを詳しく見てみましょう。makeユーティリティーの主な目的は
プログラムのどの部分を再コンパイルする必要があるかを自動的に判断し、それらを再コンパイル
するためのコマンドを発行することです。makeやMakefileについてよく知らない人は
[この記事](http://opensourceforu.com/2012/06/gnu-make-in-detail-for-beginners/) を読むことを
お勧めします。第1回目のレッスンで使用するMakefileは[ここ](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/Makefile)に
あります。Makefileの全体を以下に示します。

```
ARMGNU ?= aarch64-linux-gnu

COPS = -Wall -nostdlib -nostartfiles -ffreestanding -Iinclude -mgeneral-regs-only
ASMOPS = -Iinclude

BUILD_DIR = build
SRC_DIR = src

all : kernel8.img

clean :
    rm -rf $(BUILD_DIR) *.img

$(BUILD_DIR)/%_c.o: $(SRC_DIR)/%.c
    mkdir -p $(@D)
    $(ARMGNU)-gcc $(COPS) -MMD -c $< -o $@

$(BUILD_DIR)/%_s.o: $(SRC_DIR)/%.S
    $(ARMGNU)-gcc $(ASMOPS) -MMD -c $< -o $@

C_FILES = $(wildcard $(SRC_DIR)/*.c)
ASM_FILES = $(wildcard $(SRC_DIR)/*.S)
OBJ_FILES = $(C_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%_c.o)
OBJ_FILES += $(ASM_FILES:$(SRC_DIR)/%.S=$(BUILD_DIR)/%_s.o)

DEP_FILES = $(OBJ_FILES:%.o=%.d)
-include $(DEP_FILES)

kernel8.img: $(SRC_DIR)/linker.ld $(OBJ_FILES)
    $(ARMGNU)-ld -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/kernel8.elf  $(OBJ_FILES)
    $(ARMGNU)-objcopy $(BUILD_DIR)/kernel8.elf -O binary kernel8.img
```

では、このファイルを詳しく見てみましょう。

```
ARMGNU ?= aarch64-linux-gnu
```

Makefileは変数の定義から始まります。`ARMGNU`はクロスコンパイラ用の接頭辞です。
私達は`arm64`アーキテクチャ用のソースコードを`x86`マシンでコンパイルするので
[クロスコンパイラ](https://en.wikipedia.org/wiki/Cross_compiler)を使う必要が
あります。そのため、`gcc`ではなく`aarch64-linux-gnu-gcc`を使用します。

```
COPS = -Wall -nostdlib -nostartfiles -ffreestanding -Iinclude -mgeneral-regs-only
ASMOPS = -Iinclude
```

`COPS`と`ASMOPS`は各々Cコードとアセンブラコードをコンパイルする際にコンパイラに渡す
オプションです。これらのオプションについて簡単な説明が必要でしょう。

* **-Wall** すべてのワーニングを表示する。
* **-nostdlib** C標準ライブラリを使用しない。C標準ライブラリの呼び出しのほとんどは
  最終的にオペレーティングシステムと相互作用します。私たちはベアメタルのプログラムを
  書いており、基盤となるオペレーティングシステムを持っていないので、C標準ライブラリは
  ここでは機能しません。
* **-nostartfiles** 標準スタートアップファイルを使用しない。スタートアップファイルは
  初期スタックポインタの設定、静的データの初期化、メインエントリポイントへのジャンプ
  などを行います。私たちはこのすべてを自分で行うことになります。
* **-ffreestanding** フリースタンディング環境とは、標準ライブラリが存在せず、プログラムの
  スタートアップが必ずしもmain関数であるとは限らない環境のことです。`-ffreestanding`
  オプションは標準関数が通常の定義を持っていることを前提としないようにコンパイラに
  指示します。
* **-Iinclude** ヘッダファイルを`include`フォルダで探すようにします。
* **-mgeneral-regs-only**. 汎用レジスタのみを使用する。ARMプロセッサは[NEON](https://developer.arm.com/technologies/neon)
  レジスタも持っていますが、（たとえば、コンテキストスイッチの際にレジスタを保存する
  必要があるので）複雑性を増すNEONをコンパイラに使ってほしくないからです。

```
BUILD_DIR = build
SRC_DIR = src
```

`SRC_DIR`と`BUILD_DIR`は各々、ソースコードとコンパイル済のオブジェクトファイルを格納する
ディレクトリです。

```
all : kernel8.img

clean :
    rm -rf $(BUILD_DIR) *.img
```

次に、makeのターゲットを定義します。最初の2つのターゲットは非常にシンプルです。`all`
ターゲットはデフォルトターゲットであり、引数なしで`make`と入力した場合はこのターゲットが
実行されます（`make`は常に最初のターゲットをデフォルトとして使用します）。このターゲットは、
すべての作業を別のターゲットである`kernel8.img`にリダイレクトするだけです。`clean`
ターゲットはすべてのコンパイル成果物とコンパイルされたカーネルイメージを削除する役割を
果たします。

```
$(BUILD_DIR)/%_c.o: $(SRC_DIR)/%.c
    mkdir -p $(@D)
    $(ARMGNU)-gcc $(COPS) -MMD -c $< -o $@

$(BUILD_DIR)/%_s.o: $(SRC_DIR)/%.S
    $(ARMGNU)-gcc $(ASMOPS) -MMD -c $< -o $@
```

次の2つのターゲットは、Cファイルとアセンブラファイルのコンパイルを担当します。たとえば、
`src`ディレクトリに`foo.c`と`foo.S`がある場合、それぞれ`build/foo_c.o`と`build/foo_s.o`に
コンパイルされます。`$<`と`$@`は実行時に入力ファイル名と出力ファイル名（`foo.c`と`foo_c.o`）に
置き換えられます。また、Cファイルをコンパイルする前に、まだ存在しない場合に備えて`build`
ディレクトリを作成します。

```
C_FILES = $(wildcard $(SRC_DIR)/*.c)
ASM_FILES = $(wildcard $(SRC_DIR)/*.S)
OBJ_FILES = $(C_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%_c.o)
OBJ_FILES += $(ASM_FILES:$(SRC_DIR)/%.S=$(BUILD_DIR)/%_s.o)
```

ここでは、Cとアセンブラのソースファイルを連結して作成されるすべてのオブジェクトファイル
（`OBJ_FILES`）の配列を構築しています（[置換参照](https://www.gnu.org/software/make/manual/html_node/Substitution-Refs.html)を参照）。


```
DEP_FILES = $(OBJ_FILES:%.o=%.d)
-include $(DEP_FILES)
```

次の2行は少し注意が必要です。Cとアセンブラのソースファイルのコンパイルターゲットを定義した
方法を見てみると`-MMD`パラメータを使用していることに気づくでしょう。このパラメータは、
生成されるオブジェクトファイルごとに依存関係ファイルを作成するよう`gcc`コンパイラに指示します。
依存関係ファイルとは、特定のソースファイルに関するすべての依存関係を定義するものです。通常、
これらの依存関係にはインクルードされるすべてのヘッダーのリストが含まれています。ヘッダが
変更された場合にmakeが再コンパイルする内容を正確に知ることができるように、生成されたすべての
依存関係ファイルをインクルードする必要があります。

```
$(ARMGNU)-ld -T $(SRC_DIR)/linker.ld -o kernel8.elf  $(OBJ_FILES)
```

`OBJ_FILES`配列を使用して`kernel8.elf`ファイルを構築します。リンカスクリプト`src/linker.ld`を
使用して生成される実行イメージの基本的なレイアウトを定義します（リンカスクリプトについては
次節で説明します）。

```
$(ARMGNU)-objcopy kernel8.elf -O binary kernel8.img
```

`kernel8.elf`は[ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
フォーマットです。問題はELFファイルはオペレーティングシステムで実行されるように
設計されていることです。ベアメタルプログラムを書くには、ELFファイルから実行ファイルと
データセクションをすべて抜き出して`kernel8.img`イメージに入れる必要があります。
ファイル名の最後の`8`は64ビットアーキテクチャであるARMv8を示しています。このファイル名は
ファームウェアにプロセッサを64ビットモードで起動するよう指示します。`config.txt`
ファイルに`arm_control=0x200`と設定することによりCPUを64ビットモードで起動することも
できます。RPi OSは以前にこの方法を採用しており、今でも練習問題の解答の中にはこの方法が
出てくることがあります。しかし、`arm_control`フラグは文書化されておらず、`kernel8.img`
とういう命名規約を使用する方が望ましいです。

### リンカスクリプト

リンカスクリプトの第一の目的は、入力オブジェクトファイル(`_c.o`と`_s.o`)の各セクションを
出力ファイル(`.elf`)にどのようにマッピングするかを記述することです。リンカスクリプトに
関する詳細は[こちら](https://sourceware.org/binutils/docs/ld/Scripts.html#Scripts)を
ご覧ください。それでは、RPi OSのリンカスクリプトを見てみましょう。

```
SECTIONS
{
    .text.boot : { *(.text.boot) }
    .text :  { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    . = ALIGN(0x8);
    bss_begin = .;
    .bss : { *(.bss*) }
    bss_end = .;
}
```

Raspberry Piは起動後、`kernel8.img`をメモリにロードし、ファイルの先頭から実行を開始します。
そのため、`.text.boot`セクションが最初になければなりません。このセクションの中にOSの
スタートアップコードを入れることになります。`.text`, `.rodata`, `.data`の各セクションには
コンパイルされたカーネル命令、読み取り専用データ、通常のデータがそれぞれ格納されます。
これらについては特に説明することはありません。`.bss`セクションには、0に初期化する必要の
あるデータが格納されます。このようなデータを別のセクションに格納することでコンパイラは
ELFバイナリのサイズを削減することができます（ELFヘッダにはセクションサイズだけが格納され、
セクション自体は格納されません）。イメージをメモリにロードした後に`.bss`セクションを0に
初期化する必要があります。セクションの開始アドレスと終了アドレス（`bss_begin`と`bss_end`
シンボル）を記録しなければならないのはそのためです。また、セクションが8の倍数のアドレス
から始まるようにアラインする必要があります。セクションがアラインされていないと`str`命令を
使用して`bss`セクションの先頭に0を格納することが難しくなります。`str`命令は8バイト
アラインのアドレスでしか使用できないからです。

### カーネルを起動する

ようやく[boot.S](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/src/boot.S)ファイルを
見る番なりました。このファイルにはカーネルの起動コードが含まれています。

```
#include "mm.h"

.section ".text.boot"

.globl _start
_start:
    mrs    x0, mpidr_el1
    and    x0, x0,#0xFF        // プロセッサidをチェック
    cbz    x0, master          // プライマリCPU以外をハングアップ
    b    proc_hang

proc_hang:
    b proc_hang

master:
    adr    x0, bss_begin
    adr    x1, bss_end
    sub    x1, x1, x0
    bl     memzero

    mov    sp, #LOW_MEMORY
    bl    kernel_main
```

では、このファイルを詳しく見ていきましょう。
```
.section ".text.boot"
```

まず、`boot.S`で定義するすべてのものを`.text.boot`セクションに入れるように指定します。
前節でこのセクションがリンカスクリプトによってカーネルイメージの先頭に置かれることを
説明しました。そのため、カーネルが起動されると`start`関数から実行が始まります。

```
.globl _start
_start:
    mrs    x0, mpidr_el1
    and    x0, x0,#0xFF        // プロセッサidをチェック
    cbz    x0, master          // プライマリCPU以外をハングアップ
    b    proc_hang
```

この関数が最初に行うことはプロセッサIDのチェックです。Raspberry Pi 3には4つのコア
プロセッサが搭載されています。デバイスの電源を入れると各コアが同一のコードの実行を
開始します。しかし、4つのコアを動作させたくはありません。最初のコアだけを動作させ、
他のコアはすべて無限ループにしたいのです。`_start`関数が担当しているのがまさにこれです。
この関数は[mpidr_el1](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0500g/BABHBJCI.html)
システムレジスタからプロセッサIDを取得します。現在のプロセスIDが0であれば実行を
`master`関数に移します。

```
master:
    adr    x0, bss_begin
    adr    x1, bss_end
    sub    x1, x1, x0
    bl     memzero
```

ここでは`memzero`を呼び出して`.bss`セクションをゼロクリアします。この関数の定義は後で
行います。ARMv8アーキテクチャでは、慣習的に、最初の7つの引数はレジスタx0-x6を介して
呼び出された関数に渡されます。`memzero`関数は引数を2つだけ受け付けます。開始アドレス
（`bss_begin`）とゼロクリアが必要なセクションのサイズ（`bss_end - bss_begin`）です。

```
    mov    sp, #LOW_MEMORY
    bl    kernel_main
```

`.bss`セクションをゼロクリアした後はスタックポインタを初期化し、`kernel_main`関数に
実行を渡します。Raspberry Piはカーネルをアドレス0にロードします。そのため、初期の
スタックポインタは十分に高い任意の位置に設定することができ、カーネルイメージが十分に
大きくなってもスタックが上書きされることはありません。`LOW_MEMORY`は[mm.h](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/include/mm.h) で
定義されており、4MBに相当します。私たちのカーネルのスタックはそれほど大きくならず、
イメージ自体も小さいので、4MBで十分です。

ARMのアセンブラ文法に詳しくない方のために、今回使用した命令を簡単にまとめておきます。

* [**mrs**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289881374.htm)
  システムレジスタの値を汎用レジスタ(x0–x30)にロードします。
* [**and**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289863017.htm)
  論理AND操作を行います。ここでは`mpidr_el1`レジスタから取得したあたりから最後の
  1バイトを取り出すためにこの命令を使用しています。
* [**cbz**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289867296.htm)
  1つ前に実行した操作の結果と0を比べて、その結果が真の場合、指定したラベルにジャンプ
  （ARMの用語に従えば`branch（分岐）`します。
* [**b**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289863797.htm)
  指定のラベルに無条件分岐します。
* [**adr**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289862147.htm)
  ラベルの相対アドレスをターゲットレジスタにロードします。ここでは`.bss`セクションの
  開始アドレスと終了アドレスへのポインタを求めています。
* [**sub**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289908389.htm)
  2つのレジスタの値の差を取ります。
* [**bl**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289865686.htm)
  「リンクありの分岐」: 無条件分岐を行い、復帰アドレスをx30（リンクレジスタ）に格納
  します。サブルーチンが終わったら、`ret`命令を使って復帰アドレスにジャンプします。
* [**mov**](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289878994.htm)
  レジスタ間で値を、または定数をレジスタに移動します。

ARMv8-A開発者ガイドが[ここ](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/index.html)にあります。
これはARM ISAに馴染みのない方には良い資料です。[このページ](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/ch09s01s01.html)ではABIにおけるレジスタ使用規則が具体的に
説明されています。

### `kernel_main`関数

ブートコードは最終的に`kernel_main`関数に制御を渡すことを見てきました。それを見て
みましょう。

```
#include "mini_uart.h"

void kernel_main(void)
{
    uart_init();
    uart_send_string("Hello, world!\r\n");

    while (1) {
        uart_send(uart_recv());
    }
}

```

この関数はカーネルの中で最もシンプルな関数の1つです。この関数は`Mini UART`デバイスを使って
画面に表示し、ユーザーの入力を読み取ります。カーネルは`Hello, world!`と表示した後、ユーザ
から文字を読み取って画面に送り返す無限ループに入ります。

### Raspberry Piデバイス

ここからは、Raspberry Pi固有の内容を掘り下げていきます。その前に[BCM2837 ARMペリフェラルマニュアル](https://github.com/raspberrypi/documentation/files/1888662/BCM2837-ARM-Peripherals.-.Revised.-.V2-1.pdf)を
ダウンロードすることをお勧めします。BCM2837は、Raspberry Pi 3 Model BとB+で使用されている
ボードです。説明の中ではBCM2835とBCM2836にも言及することがあるかもしれません。これらは古い
Raspberry Piで使用されていたボードの名前です。

実装の詳細を説明する前に、メモリマップドデバイスを扱う上での基本的な概念を説明します。
BCM2837はシンプルな[SOC (System on a chip)](https://en.wikipedia.org/wiki/System_on_a_chip)
ボードです。このようなボードではすべてのデバイスにメモリマップドレジスタを介してアクセス
します。Raspberry Pi 3はアドレス`0x3F000000`以上のメモリをデバイス用に予約しています。
特定のデバイスの起動や設定を行うには、そのデバイスのレジスタのいずれかに何らかのデータを
書き込む必要があります。デバイスレジスタは32ビットのメモリ領域に過ぎません。各デバイス
レジスタの各ビットの意味は「BCM2837 ARMペリフェラルマニュアル」に記載されています。
（マニュアルでは`0x7E000000`が使用されているにも関わらず）`0x3F000000`をベースアドレスと
して使用している理由については、マニュアルの「1.2.3 ARM物理アドレス」とその周辺
ドキュメントを参照してください。

`kernel_main`関数から`Mini UART`デバイスを使用しようとしていることが推測できるでしょう。
UARTとは[Universal asynchronous receiver-transmitter](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)の
略です。このデバイスは、メモリマップされたレジスタの1つに格納されている値を電圧の高低の
シーケンスに変換することができます。このシーケンスは`TTLシリアルケーブル`を介して
コンピュータに渡され、ターミナルエミュレータによって解釈されます。ここではRaspberry Pi
との通信を容易にするためにMini UARTを使用します。Mini UARTのレジスタの仕様を確認したい
場合は「BCM2837 ARMペリフェラルマニュアル」の8ページを参照してださい。

Raspberry Piには2つのUARTがあります。Mini UARTとPL011 UARTです。このチュートリアルでは
よりシンプルな1つ目のUARTだけを使用します。しかし、PL011 UARTの扱い方を示すオプションの
[練習問題](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/docs/lesson01/exercises.md)があります。
Raspberry PiのUARTについてもっと詳しく知りたい方や2つのUARTの違いを知りたい方は[公式どきゅ](https://www.raspberrypi.org/documentation/configuration/uart.md)を参照してください。

もう1つ慣れておく必要のあるデバイスはGPIO [汎用入出力（General-purpose input/output）](https://en.wikipedia.org/wiki/General-purpose_input/output)です。GPIOはGPIOピンを制御する役割を
担っています。下の画像を見ればすぐにわかるはずです。

![Raspberry Pi GPIO pins](../../images/gpio-pins.jpg)

GPIOは各GPIOピンの動作を構成するために使用できます。たとえば、Mini UARTを使えるように
するには、ピン14と15をアクティブにして、このデバイスを使えるように設定する必要があります。
下の画像はGPIOピンの番号がどのように割り当てられているのかを示しています。

![Raspberry Pi GPIO pin numbers](../../images/gpio-numbers.png)

### Mini UARTの初期化

では、mini UARTがどのように初期化されるかを見てみましょう。このコードは [mini_uart.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/src/mini_uart.c)で
定義されています。

```
void uart_init ( void )
{
    unsigned int selector;

    selector = get32(GPFSEL1);
    selector &= ~(7<<12);                   // gpio14をクリーンアップ
    selector |= 2<<12;                      // gpio14をalt5に設定
    selector &= ~(7<<15);                   // gpio15をクリーンアップ
    selector |= 2<<15;                      // gpio 15をalt5に設定
    put32(GPFSEL1,selector);

    put32(GPPUD,0);
    delay(150);
    put32(GPPUDCLK0,(1<<14)|(1<<15));
    delay(150);
    put32(GPPUDCLK0,0);

    put32(AUX_ENABLES,1);                   // mini uartを有効にする（これはそのレジスタへのアクセスも有効にする）
    put32(AUX_MU_CNTL_REG,0);               // 自動フロー制御、送受信機を無効にする（一時的に）
    put32(AUX_MU_IER_REG,0);                // 送受信割り込みを無効にする
    put32(AUX_MU_LCR_REG,3);                // 8ビットモードに設定する
    put32(AUX_MU_MCR_REG,0);                // RTSラインを常にHighに設定する
    put32(AUX_MU_BAUD_REG,270);             // 通信速度を115200に設定する

    put32(AUX_MU_CNTL_REG,3);               // 最後に、送受信機を有効にする
}
```

ここでは`put32`と`get32`という2つの関数を使います。これらの関数はきわめてシンプルであり、
32ビットのレジスタとの間でデータを読み書きすることができます。それらがどのように実装されて
いるかは[utils.S](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/src/utils.S)で見ることができます。
`uart_init`はこのレッスンの中で最も複雑かつ重要な関数の1つです。以下の3つの節で検討していきます。

#### GPIO代替機能の選択

まず、GPIOピンをアクティブにする必要があります。ほとんどのピンは様々なデバイスで使用
できるので、特定のピンを使用する前にそのピンの`代替機能`を選択する必要があります。
`代替機能`とは、各ピンに設定できる0から5までの数字のことであり、そのピンにどのデバイスを
接続するかを設定します。下の図は利用可能なすべてのGPIO代替機能のリストです（図は
「BCM2837 ARMペリフェラルマニュアル」の102ページから引用しました）。

![Raspberry Pi GPIO alternative functions](../../images/alt.png?raw=true)

ここでは、ピン14と15にTXD1とRXD1の代替機能が用意されていることがわかります。これは
ピン14と15に代替機能番号5を選択すれば、それぞれMini UART送信データピンおよびMini UART
受信データピンとして使用できることを意味します。`GPFSEL1`レジスタはピン10からピン19の
代替機能の制御に使用されます。これらのレジスタのすべてのビットの意味は次の表に示されて
います（「BCM2837 ARMペリフェラルマニュアル」の92ページ）。

![Raspberry Pi GPIO function selector](../../images/gpfsel1.png?raw=true)

これで、Mini UARTデバイスを使用できるようにGPIOピン14と15を設定するために使用される
以下のコードを理解するために必要なことがすべてわかりました。

```
    unsigned int selector;

    selector = get32(GPFSEL1);
    selector &= ~(7<<12);                   // gpio14をクリーンアップ
    selector |= 2<<12;                      // gpio14をalt5に設定
    selector &= ~(7<<15);                   // gpio15をクリーンアップ
    selector |= 2<<15;                      // gpio 15をalt5に設定
    put32(GPFSEL1,selector);
```

#### GPIOプルアップ/プルダウン

Raspberry PiのGPIOピンを扱っているとプルアップ/プルダウンといった用語をよく目に
するようになります。これらの概念については[この記事](https://grantwinney.com/using-pullup-and-pulldown-resistors-on-the-raspberry-pi/)で詳しく説明されています。
記事全体を読むのが面倒な方のためにプルアップ/プルダウンの概念を簡単に説明します。

あるピンを入力として使用していても、このピンに何も接続していないと、このピンの値が
1なのか0なのかを識別することはできません。実際、デバイスはランダムな値を報告します。
この問題を解決するのがプルアップ/プルダウン機構です。ピンをプルアップ状態に設定して、
何も接続しなければ、常に`1`を報告します（プルダウン状態では、値は常に0です）。今回の
ケースでは、14ピンと15ピンは共に常に接続されているため、プルアップ状態もプルダウン状態も
必要ありません。ピンの状態は再起動後も保持されるのでピンを使用する前には必ずその状態を
初期化する必要があります。設定可能な状態は、プルアップ、プルダウン、どちらでもない
（現在のプルアップまたはプルダウンの状態を解除する）の3つで、ここでは3つ目の状態が
必要です。

ピンの状態を切り替えることはそれほど簡単なことではありません。電気回路上のスイッチを
物理的に切り替える必要があるからです。このプロセスには`GPPUD`レジスタと`GPPUDCLK`
レジスタが関与しており、B「BCM2837 ARMペリフェラルマニュアル」の101ページに記載されて
います。その記述を以下にコピーしました。

```
GPIOプルアップ/プルダウンクロックレジスタはGPIOピンの内部プルダウン動作を制御します。
GPIOのプルアップ/プルダウンを変更するには、これらのレジスタとGPPUDレジスタを連携させて
使用する必要があります。次のような操作手順が必要です。
1. 必要な制御信号を設定（プルアップ、プルダウン、または現在のプルアップ/ダウンを解除）
   するためにGPPUDに書き込む。
2. 150サイクル待つ。これは制御信号に必要なセットアップタイムを与える。
3. 制御信号を変更したいGPIOパッドに入力するためにGPPUDCLK0/1に書き込む。
   注意: クロックを受信したパッドのみが変更され、その他のパッドは以前の状態を維持する。
4. 150サイクル待つ 。これは制御信号に必要なホールドタイムを与える。
5. 制御信号を除去するためにGPPUDに書き込む。
6. クロックを除去するためにGPPUDCLK0/1に書き込む。
```

この手順では、1つのピンからプルアップとプルダウン双方の状態を解除する方法を説明しています。
これがピン14と15について次のコードで行っていることです。

```
    put32(GPPUD,0);
    delay(150);
    put32(GPPUDCLK0,(1<<14)|(1<<15));
    delay(150);
    put32(GPPUDCLK0,0);
```

#### Mini UARTの初期化

これで、Mini UARTがGPIOピンに接続され、ピンが設定されました。`uart_init`関数の残りの部分は
Mini UARTの初期化です。

```
    put32(AUX_ENABLES,1);                   // mini uartを有効にする（これはそのレジスタへのアクセスも有効にする）
    put32(AUX_MU_CNTL_REG,0);               // 自動フロー制御、送受信機を無効にする（一時的に）
    put32(AUX_MU_IER_REG,0);                // 送受信割り込みを無効にする
    put32(AUX_MU_LCR_REG,3);                // 8ビットモードに設定する
    put32(AUX_MU_MCR_REG,0);                // RTSラインを常にHighに設定する
    put32(AUX_MU_BAUD_REG,270);             // 通信速度を115200に設定する

    put32(AUX_MU_CNTL_REG,3);               // 最後に、送受信機を有効にする
```

コード片を一行ずつ見ていきましょう。

```
    put32(AUX_ENABLES,1);                   // mini uartを有効にする（これはそのレジスタへのアクセスも有効にする）
```

この行は、Mini UARTを有効にします。これは最初に行う必要があります。これは他のすべての
Mini UARTレジスタへのアクセスも有効にするからです。

```
    put32(AUX_MU_CNTL_REG,0);               // 自動フロー制御、送受信を無効にする（一時的に）
```

ここでは構成を完了するまで送受信機を無効にしています。また、自動フロー制御を永久に
無効にしています。この制御にはさらに別のGPIOピンを使用する必要があり、また、TTL-シリアル
ケーブルはそれをサポートしていないからです。自動フロー制御の詳細については、
[この記事](http://www.deater.net/weave/vmwprod/hardware/pi-rts/)を参照してください。

```
    put32(AUX_MU_IER_REG,0);                // 送受信割り込みを無効にする
```

新しいデータが利用可能になるたびにプロセッサに割り込みを生成するようにMini UARTを
設定することができます。割り込みについてはレッスン3で扱う予定なので、現時点では、
この機能を無効にしておきます。

```
    put32(AUX_MU_LCR_REG,3);                // 8ビットモードに設定する
```

Mini UARTは7ビット操作と8ビット操作をサポートしています。これは、ASCII文字が
標準セットでは7ビット、拡張セットでは8ビットであるためです。ここでは8ビット
モードを使用します。

```
    put32(AUX_MU_MCR_REG,0);                // RTSラインを常にHighに設定する
```

RTSラインはフロー制御で使用されるものなので必要ありません。常にHighになるように
設定します。

```
    put32(AUX_MU_BAUD_REG,270);             // 通信速度を115200に設定する
```

ボーレートとは、通信チャネルで情報を転送する速度のことです。"115200ボー"は
シリアルポートが最大1秒間に115200ビットの転送ができることを意味します。
Raspberry Pi mini UARTデバイスのボーレートは、ターミナルエミュレータのボー
レートと同じである必要があります。Mini UARTは次の式に従ってボーレートを計算します。

```
baudrate = system_clock_freq / (8 * ( baudrate_reg + 1 ))
```

`system_clock_freq`は250MHzなので`baudrate_reg`の値は270と簡単に計算できます。

```
    put32(AUX_MU_CNTL_REG,3);               // 最後に、送受信機を有効にする
```

この行が実行されると、Mini UARTは動作可能な状態になります。

### Mini UARTを使ったデータの送信

Mini UARTの準備ができたので実際にデータの送受信を行ってみましょう。これを行うには
次の2つの関数を使用します。

```
void uart_send ( char c )
{
    while(1) {
        if(get32(AUX_MU_LSR_REG)&0x20)
            break;
    }
    put32(AUX_MU_IO_REG,c);
}

char uart_recv ( void )
{
    while(1) {
        if(get32(AUX_MU_LSR_REG)&0x01)
            break;
    }
    return(get32(AUX_MU_IO_REG)&0xFF);
}
```

どちらの関数も無限ループから始まりますが、その目的はデバイスがデータを送信または
受信する準備ができているか否かを確認することです。この確認には`AUX_MU_LSR_REG`
レジスタを使用します。第0ビットが1に設定されている場合は、データの準備ができている
ことを示し、UARTから読み取ることができることを意味します。第5ビットが1に設定されて
いる場合は、送信機が空であることを示し、UARTへの書き込みができることを意味します。
次に、`AUX_MU_IO_REG`を使用して、送信する文字の値を格納したり、受信した文字の値を
読み取ったりします。

また、文字ではなく文字列を送信することができる非常に簡単な関数も用意しています。

```
void uart_send_string(char* str)
{
    for (int i = 0; str[i] != '\0'; i ++) {
        uart_send((char)str[i]);
    }
}
```

この関数は文字列のすべての文字を一文字ずつ取り出して送信しているだけです。

### Raspberry Piの設定

Raspberry Piの起動シーケンスは以下の通りです（簡略化しています）。

1. デバイスの電源が入る。
2. GPUが起動し、ブートパーティションから`config.txt`ファイルを読み込みます。
   このファイルにはGPUが起動シーケンスを微調整するために使用する設定パラメータが
   含まれています。
3. `kernel8.img`がメモリにロードされ、実行されます。

私たちのシンプルなOSを実行するために`config.txt`ファイルを以下のようにします。

```
kernel_old=1
disable_commandline_tags=1
```

* `kernel_old=1`はカーネルイメージをアドレス0にロードするよう指定します。
* `disable_commandline_tags=1`は、ブートイメージにコマンドライン引数も渡さない
  ようにGPUに指示します。

### カーネルのテスト

さて、ソースコードを一通り読んだのでいよいよ動作を確認してみましょう。カーネルを
ビルドしてテストするには、次のようにする必要があります。

1. [src/lesson01](https://github.com/s-matyukevich/raspberry-pi-os/tree/master/src/lesson01)で
   `./build.sh`または`./build.bat`を実行してカーネルをビルドします。
2. 生成された`kernel8.img`ファイルをRaspberry Piフラッシュカードの`boot`パーティションに
   コピーし、`kernel7.img`とSDカードに存在する可能性のある他の`kernel*.img`ファイルを
   削除します。ブートパーティションにあるその他のファイルはすべてそのままにしておいて
   ください（詳細はissueの[43](https://github.com/s-matyukevich/raspberry-pi-os/issues/43) と
   [158](https://github.com/s-matyukevich/raspberry-pi-os/issues/158)を参照）。
3. 前節で説明したように`config.txt`ファイルを修正します。
4. [前提条件](../Prerequisites.md)で説明したようにUSB-TTLシリアルケーブルを接続します。
5. Raspberry Piの電源を入れます。
6. ターミナルエミュレータを開きます。そこには"Hello, world!"のメッセージが表示されるはずです。

この手順は、SDカードにRaspbianがインストールされていることを前提としています。
空のSDカードを使ってRPi OSを起動することも可能です。

1. SDカードを準備します。
    * MBRパーティションテーブルを使用します。
    * ブートパーティションをFAT32でフォーマットします。
    > カードはRaspbianのインストールに必要な方法でフォーマットする必要があります。詳細は
    [公式ドキュメント](https://www.raspberrypi.org/documentation/installation/noobs.md) の
    「SDカードをFATでフォーマットする方法」のセクションをご覧ください。
2. 以下のファイルをカードにコピーします。
    * [bootcode.bin](https://github.com/raspberrypi/firmware/blob/master/boot/bootcode.bin)
    これはGPUブートローダです。GPUを起動してGPUファームウェアをロードするためのGPUコードが
    含まれています。
    * [start.elf](https://github.com/raspberrypi/firmware/blob/master/boot/start.elf)
    これはGPUファームウェアです。`config.txt`を読み込んで、GPUが`kernel8.img`からARM固有の
    ユーザーコードをロードして実行できるようにします。
3. `kernel8.img`と`config.txt`をコピーします。
4. USB-TTLシリアルケーブルを接続します。
5. Raspberry Piの電源を入れます。
6. ターミナルエミュレータを使って、RPi OSに接続します。

残念ながら、Raspberry Piのファームウェアファイルはすべてクローズドソースで文書化
されていません。Raspberry Piの起動手順については[このStackExchangeでの質問](https://raspberrypi.stackexchange.com/questions/10442/what-is-the-boot-sequence)や
[このGithubリポジトリ](https://github.com/DieterReuter/workshop-raspberrypi-64bit-os/blob/master/part1-bootloader.md)などの非公式なソースを参考にすることができます。

##### 前ページ

[前提条件](../../docs/Prerequisites.md)

##### 次ページ

1.2 [カーネルの初期化: Linuxプロジェクトの構成](../../docs/lesson01/linux/project-structure.md)
