## 1.3: カーネルビルドシステム

Linuxカーネルの構成を調べた後は、それをどのようにビルド・実行するかを調べる
ことに時間をかける価値があります。Linuxもカーネルのビルドには`make`
ユーティリティを使用しますが、Linuxのmakefileははるかに複雑です。makefileを
見る前にLinuxのビルドシステムに関するいくつかの重要な概念を学びましょう。
それは"kbuild"と呼ばれています。

### kbuildの重要なコンセプト

* ビルドプロセスはkbuild変数を使ってカスタマイズすることができます。
  これらの変数は`Kconfig`ファイルで定義されます。このファイルでは変数自体と
  そのデフォルト値を定義することができます。変数には、文字列、真偽値、
  整数など、さまざまな型を持つことができます。Kconfigファイルでは、変数間の
  依存関係を定義することもできます（たとえば、変数Xが選択されたら、変数Yが
  暗黙のうちに選択されるようにすることができます）。例として、[arm64のKconfigファイル](https://github.com/torvalds/linux/tree/v4.14/arch/arm64/Kconfig)を
  見てみましょう。このファイルには`arm64`アーキテクチャ固有のすべての変数が
  定義されています。`Kconfig`の機能は標準的な`make`の機能ではなく、Linuxの
  makefileに実装されているものです。`Kconfig`で定義された変数はカーネルの
  ソースコードだけでなく、ネストされたmakefileにも公開されます。変数の値は
  カーネルの構成段階で設定することができます。（たとえば、`make menuconfig`と
  入力すると、コンソールGUIが表示されます。これを使うことにより、すべての
  カーネル変数の値をカスタマイズすることができ、その値は`.config`に保存
  されます。カーネルの構成に使用できるすべてのオプションを表示するには
  `make help`コマンドを使用してください。)

* Linuxは再帰的ビルドを採用しています。これはLinuxカーネルはサブフォルダ
  ごとにが独自の`Makefile`と`Kconfig`を定義できることを意味します。ネスト
  されたMakefileのほとんどは非常にシンプルであり、どのオブジェクトファイルを
  コンパイルする必要があるかを定義しているだけです。通常、このような定義は
  次のような形式になっています。

  ```
  obj-$(SOME_CONFIG_VARIABLE) += some_file.o
  ```

  この定義は、`SOME_CONFIG_VARIABLE`が設定されている場合にのみ、`some_file.c`が
  コンパイルされ、カーネルにリンクされることを意味しています。無条件に
  ファイルをコンパイル・リンクしたい場合は、この定義を次のように変更する
  必要があります。

  ```
  obj-y += some_file.o
  ```

  ネストしたMakefileの例は[この`Makefile`](https://github.com/torvalds/linux/tree/v4.14/kernel/Makefile)にあります。

* 先に進む前に、基本的なmakeルールの構造を理解し、make用語に慣れておく
  必要があります。一般的なルールの構造を以下に図示します。

  ```
  targets : prerequisites
          recipe
          …
  ```

    * `target`は、空白で区切られたファイル名です。targetはルールの
    実行後に生成されます。通常、1つのルールには1つのtargetだけです。。
    * `prerequisites`は、ターゲットを更新する必要があるか否かを`make`が
    確認するためのファイルです。
    * `recipe`は、bashスクリプトです。makeはprerequisitesの一部が更新されると
    このスクリプトを呼び出します。recipeはtargetの生成を担当します。
    * targetとprerequisitesにはワイルドカード（`%`）を含めることができます。
    ワイルドカードが使用されると、recipeはマッチしたprerequisitesのそれぞれに
    対して個別に実行されます。この場合、recipe内でprerequisiteとtargetを
    参照するために各々、変数`$<`と`$@`を使用することができます。これは
    [RPi OSのmakefile](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/Makefile#L14)ですでに使用しています。

  makeルールの詳細については、[公式ドキュメント](https://www.gnu.org/software/make/manual/html_node/Rule-Syntax.html#Rule-Syntax)を参照してください。

* `make`は、前提条件が変更されていないかどうかを検出し、再構築が必要な
  ターゲットだけを更新することは得意ですが、レシピが動的に更新されても
  `make`はこの変更を検出できません。どうしてそうなるのでしょうか。とても
  簡単です。一つの良い例は、何らかの設定変数を変更した結果、レシピに
  オプションが追加される場合です。このような場合、`make`はデフォルトでは
  生成済みのオブジェクトファイルを再コンパイルしません。レシピが変更された
  だけで、その前提条件のファイルには変更がないからです。この問題を解決する
  ために、Linuxは[if_changed](https://github.com/torvalds/linux/blob/v4.14/scripts/Kbuild.include#L264)
  関数を導入しました。この関数がどのように機能するか、次の例を考えて
  みましょう。

  ```
  cmd_compile = gcc $(flags) -o $@ $<

  %.o: %.c FORCE
      $(call if_changed,compile)
  ```

  ここでは、各`.c`ファイルに対して、`compile`という引数で`if_changed`関数を
  呼び出して対応する`.o`ファイルをビルドします。`if_changed`は（最初の引数に
  `cmd_`というプレフィックスを追加した）`cmd_compile`変数を探し、この変数が
  前回の実行後に更新されていないか、また、前提条件が変更されていないかを
  チェックします。変更されている場合、`cmd_compile`コマンドが実行され
  オブジェクトファイルが再生成されます。このサンプルルールでは、2つの前提
  条件があります。ソースファイル`.c`と`FORCE`です。`FORCE`は特別な前提条件で
  あり、`make`コマンドが呼び出されるたびにレシピが強制的に呼び出されます。
  これがないと、`.c`ファイルが変更された場合にしかレシピが呼び出されません。
  `FORCE`ターゲットの詳細については[こちら](https://www.gnu.org/software/make/manual/html_node/Force-Targets.html)をご覧ください。

### カーネルのビルド

さて、Linuxのビルドシステムに関する重要な概念を学んだところで、`make`コマンドを
入力した後に何が起こっているのかを正確に見ていきましょう。このプロセスは非常に
複雑であり、多くの詳細が含まれていますが、そのほとんどは省略します。ここでは
次の2つの質問に答えることを目標とします。

1. ソースファイルはどのようにオブジェクトファイルにコンパイルされるのか。
2. オブジェクトファイルはどのようにOSイメージにリンクされるのか。

ここでは、まず2つ目の質問に取り組みます。

#### リンクステージ

* `make help`コマンドの出力を見るとわかるかもしれませんが、カーネルのビルドを
  担当するデフォルトのターゲットは`vmlinux`と呼ばれています。

* `vmlinux`ターゲットの定義は[`Makefile#L1004`](https://github.com/torvalds/linux/blob/v4.14/Makefile#L1004)にあり、次のようになっています。

  ```
  cmd_link-vmlinux =                                                 \
      $(CONFIG_SHELL) $< $(LD) $(LDFLAGS) $(LDFLAGS_vmlinux) ;    \
      $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

  vmlinux: scripts/link-vmlinux.sh vmlinux_prereq $(vmlinux-deps) FORCE
      +$(call if_changed,link-vmlinux)
  ```

  このターゲットは、すでにおなじみの`if_changed`関数を使用しています。
  前提条件のいずれかが更新されると`cmd_link-vmlinux`コマンドが実行されます。
  このコマンドは[scripts/link-vmlinux.sh](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh)
  スクリプトを実行します（`cmd_link-vmlinux`コマンドで自動変数 [$<](https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html)
  が使用されていることに注意してください）。また、アーキテクチャ固有の
  [postlink script](https://github.com/torvalds/linux/blob/v4.14/Documentation/kbuild/makefiles.txt#L1229)
  スクリプトも実行されますが、ここではあまり興味がありません。

* [scripts/link-vmlinux.sh](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh)が実行される際、必要なすべてのオブジェクトファイルは
既にビルドされており、そのロケーションが3つの変数、`KBUILD_VMLINUX_INIT`,
`KBUILD_VMLINUX_MAIN`, `KBUILD_VMLINUX_LIBS`に格納されていると仮定されています。

* `link-vmlinux.sh`スクリプトはまず、利用可能なすべてのオブジェクトファイル
から`thin archive`を作成します。`thin archive`はオブジェクトファイルセット
だけでなく、その合成されたシンボルテーブルも含んでいる特別なオブジェクトです。
これは[archive_builtin](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L56)
関数で行われます。この関数は`thin archive`を作成するために[ar](https://sourceware.org/binutils/docs/binutils/ar.html)
ユーティリティを使用します。生成されたthin archive`は`built-in.o`ファイルに
格納され、リンカが理解可能なフォーマットになっています。そのため、その他の
普通のオブジェクトファイルのように使用することができます。

* 次に、[modpost_link](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L69)を呼び出します。この関数はリンカを呼び出して`vmlinux.o`
オブジェクトファイルを生成します。このオブジェクトファイルは[セクションミスマッチ解析](https://github.com/torvalds/linux/blob/v4.14/lib/Kconfig.debug#L308)
の実行に必要です。この解析は[modpost](https://github.com/torvalds/linux/tree/v4.14/scripts/mod)
プログラムにより行われ、[`link-vmlinux.sh#L260`](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L260)行によりトリガされます。

* 次に、カーネルシンボルテーブルが生成されます。このファイルにはすべての関数と
グローバル変数に関する情報とその`vmlinux`バイナリ内のロケーションが含まれて
います。主な作業は[kallsyms](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L146)
関数で行われます。この関数は、まず[nm](https://sourceware.org/binutils/docs/binutils/nm.html)を
使用して`vmlinux`バイナリからすべてのシンボルを抽出します。そして、[scripts/kallsyms](https://github.com/torvalds/linux/blob/v4.14/scripts/kallsyms.c)
ユーティリティを使用して、Linuxカーネルが理解できる特別なフォーマットで
すべてのシンボルを含む特別なアセンブラファイルを生成します。次に、この
アセンブラファイルをコンパイルし、元のバイナリとリンクします。このプロセスは
数回繰り返されます。最終的なリンク後にシンボルのアドレスが変更されることが
あるからです。カーネルシンボルテーブルの情報は実行時に'/proc/kallsyms'
ファイルを生成する際に使用されます。

* 最後に、`vmlinux`バイナリが完成し、`System.map`が作成されます。
`System.map`には`/proc/kallsyms`と同じ情報が含まれていますが、これは静的な
ファイルであり、`/proc/kallsyms`とは異なり、実行時には生成されません。
`System.map`は主に[kernel oops](https://en.wikipedia.org/wiki/Linux_kernel_oops)
でアドレスをシンボル名に解決するために使用されます。`System.map`の作成には
同じ`nm`ユーティリティが使用されます。これは[`mksysmap#L44`](https://github.com/torvalds/linux/blob/v4.14/scripts/mksysmap#L44)で行われています。

#### ビルドステージ

* ここで一歩戻って、ソースコードファイルがどのようにオブジェクトファイルに
コンパイルされるかを見てみましょう。覚えていると思いますが`vmlinux`ターゲットの
前提条件のひとつに`$(vmlinux-deps)`変数があります。この変数がどのように
作られるかを示すために、Linuxの主たるmakefileから関連する数行を取り出しました。

  ```
  init-y        := init/
  drivers-y    := drivers/ sound/ firmware/
  net-y        := net/
  libs-y        := lib/
  core-y        := usr/

  core-y        += kernel/ certs/ mm/ fs/ ipc/ security/ crypto/ block/

  init-y        := $(patsubst %/, %/built-in.o, $(init-y))
  core-y        := $(patsubst %/, %/built-in.o, $(core-y))
  drivers-y    := $(patsubst %/, %/built-in.o, $(drivers-y))
  net-y        := $(patsubst %/, %/built-in.o, $(net-y))

  export KBUILD_VMLINUX_INIT := $(head-y) $(init-y)
  export KBUILD_VMLINUX_MAIN := $(core-y) $(libs-y2) $(drivers-y) $(net-y) $(virt-y)
  export KBUILD_VMLINUX_LIBS := $(libs-y1)
  export KBUILD_LDS          := arch/$(SRCARCH)/kernel/vmlinux.lds

  vmlinux-deps := $(KBUILD_LDS) $(KBUILD_VMLINUX_INIT) $(KBUILD_VMLINUX_MAIN) $(KBUILD_VMLINUX_LIBS)
  ```

  それはすべて`init-y`、`core-y`などの変数から始まります。これらの組み合わせは
  ビルド可能なソースコードを含むLinuxカーネルのすべてのサブフォルダを含んで
  います。次に、すべてのサブフォルダ名に`built-in.o`が追加され、たとえば、
  `drivers/`は`drivers/built-in.o`になります。`vmlinux-deps`は結果として
  得られたすべての値を集約します。これで`vmlinux`が最終的にすべての
  `built-in.o`ファイルに依存するようになることが説明できます。

* 次の疑問は、すべての`builtin.o`オブジェクトがどのように作られるかです。
もう一度、関連するすべての行をコピーして、すべての仕組みを説明しましょう。

  ```
  $(sort $(vmlinux-deps)): $(vmlinux-dirs) ;

  vmlinux-dirs    := $(patsubst %/,%,$(filter %/, $(init-y) $(init-m) \
               $(core-y) $(core-m) $(drivers-y) $(drivers-m) \
               $(net-y) $(net-m) $(libs-y) $(libs-m) $(virt-y)))

  build := -f $(srctree)/scripts/Makefile.build obj               #Copied from `scripts/Kbuild.include`

  $(vmlinux-dirs): prepare scripts
      $(Q)$(MAKE) $(build)=$@

  ```

  最初の行から`vmlinux-deps`は`vmlinux-dirs@に依存していることがわかります。
  次に、`vmlinux-dirs`は最後に`/`文字がない、すべてのダイレクトルート
  サブフォルダを含む変数であることがわかります。そして、ここで最も重要な行は
  `$(vmlinux-dirs)`ターゲットを構築するためのレシピです。すべての変数を
  置換した後にこのレシピは次のようになります（例として`driver`フォルダを
  使用していますが、このルールはすべてのルートサブフォルダに対して実行
  されます）。

  ```
  make -f scripts/Makefile.build obj=drivers
  ```

  この行は別のmakefile（[scripts/Makefile.build](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build)）を
  呼び出して、コンパイルするフォルダを含む`obj`変数を渡しているだけです。

* 次の論理的ステップは[scripts/Makefile.build](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build)を見てみましょう。これの実行後に起きる
最初に重要なことは、カレントディレクトリで定義されている`Makefile`または
`Kbuild`ファイルのすべての変数が含まれることです。カレントディレクトリとは
`obj`変数により参照されるディレクトリを意味します。この包摂は[`Makefile.build#L43-L45`の三行](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L43-L45)で行われています。

  ```
  kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
  kbuild-file := $(if $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Kbuild,$(kbuild-dir)/Makefile)
  include $(kbuild-file)
  ```

  ネストされた`makefile`はほとんどが`obj-y`のような変数の初期化を担当します。
  簡単に思い出すと、`obj-y`変数にはカレントディレクトリにあるすべてのソース
  コードファイルのリストが入っているはずです。入れ子のmakefileで初期化される
  もう  一つ重要な変数は`subdir-y`です。この変数にはカレントディレクトリにある
  ソースコードをビルドする前に訪れる必要のあるすべてのサブフォルダのリストが
  含まれています。`subdir-y`はサブフォルダへの再帰的な降下を実装するために
  使用されます。

* ターゲットを指定せずに`make`が呼び出された場合（`scripts/Makefile.build`が
実行された場合がそうです）は最初のターゲットが使用されます。`scripts/Makefile.build`の最初のターゲットは`__build`と呼ばれ、[`Makefile.build#L96`](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L96)に
あります。それでは見てみましょう。

  ```
  __build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
       $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
       $(subdir-ym) $(always)
      @:
  ```

  ご覧のように `__build`ターゲットはレシピを持っていませんが、他の多くの
  ターゲットに依存しています。ここで興味のあるのは、`built-in.o`ファイルの
  作成を担当する`$(builtin-target)`とネストしたディレクトリへの降下を担当
  する`$(subdir-ym)` だけです。

* `subdir-ym`を見てみましょう。この変数は[`Makefile.lib#L48`](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.lib#L48)で
初期化されており、変数`subdir-y`と`subdir-m`を連結しただけです（`subdir-m`
変数は`subdir-y`と似ていますが、個別の[カーネルモジュール](https://en.wikipedia.org/wiki/Loadable_kernel_module)に含まれる必要のあるサブフォルダを定義します。
今のところ、集中力を保つためにモジュールの議論は省略します）。

*  `subdir-ym`ターゲットは[`Makefile.build#L572`](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L572)で
定義されており、既におなじみでしょう。

  ```
  $(subdir-ym):
      $(Q)$(MAKE) $(build)=$@
  ```

  このターゲットはネストされたサブフォルダの一つにある`scripts/Makefile.build`
  の実行をトリガーするだけです。

* ようやく[builtin-target](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L467)
ターゲットを検証する時になりました。もう一度言いますが、ここでは関連する行
だけをコピーしています。

  ```
  cmd_make_builtin = rm -f $@; $(AR) rcSTP$(KBUILD_ARFLAGS)
  cmd_make_empty_builtin = rm -f $@; $(AR) rcSTP$(KBUILD_ARFLAGS)

  cmd_link_o_target = $(if $(strip $(obj-y)),\
                $(cmd_make_builtin) $@ $(filter $(obj-y), $^) \
                $(cmd_secanalysis),\
                $(cmd_make_empty_builtin) $@)

  $(builtin-target): $(obj-y) FORCE
      $(call if_changed,link_o_target)
  ```

  このターゲットは`$(obj-y)`ターゲットに依存しており、`obj-y`は現在の
  カレントフォルダでビルドする必要のあるすべてのオブジェクトファイルのリスト
  です。そのようなファイルの準備ができたら`cmd_link_o_target`コマンドが実行
  されます。`obj-y`変数が空の場合は`cmd_make_empty_builtin`が呼び出され、
  空の`built-in.o`が作成されます。そうでない場合は`cmd_make_builtin`コマンドが
  実行され、おなじみの`ar`ツールを使用してthin_archive `built-in.o`が作成
  されます。

* ついに何かをコンパイルしなければならないところまで来ました。思い出して
ください。まだ調べていない最後の依存関係は`$(obj-y)`です。`obj-y`は、単なる
オブジェクトファイルのリストです。すべてのオブジェクトファイルを対応する
`.c`ファイルからコンパイルするターゲットは[`Makefile.build#L313`](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L313)で
定義されています。このターゲットを理解するために必要なすべての行を調べて
みましょう。

  ```
  cmd_cc_o_c = $(CC) $(c_flags) -c -o $@ $<

  define rule_cc_o_c
      $(call echo-cmd,checksrc) $(cmd_checksrc)              \
      $(call cmd_and_fixdep,cc_o_c)                      \
      $(cmd_modversions_c)                          \
      $(call echo-cmd,objtool) $(cmd_objtool)                  \
      $(call echo-cmd,record_mcount) $(cmd_record_mcount)
  endef

  $(obj)/%.o: $(src)/%.c $(recordmcount_source) $(objtool_dep) FORCE
      $(call cmd,force_checksrc)
      $(call if_changed_rule,cc_o_c)
  ```

  このターゲットはそのレシピで`rule_cc_o_c`を呼び出します。このルールは
  たくさんのことを担当します。たとえば、一般的なエラーがないかソースコードを
  チェック (`cmd_checksrc`)、エクスポートされたモジュールシンボルの
  バージョニングの有効化 (`cmd_modversions_c`)、[objtool](https://github.com/torvalds/linux/tree/v4.14/tools/objtool)を使用した生成されたオブジェクト
  ファイルの検証、[ftrace](https://github.com/torvalds/linux/blob/v4.14/Documentation/trace/ftrace.txt)の迅速な検索を可能にする`mcount`関数の
  呼び出しリストの作成などです。しかし、最も重要なのは実際にすべての
  `.c`ファイルをオブジェクトファイルにコンパイルする`cmd_cc_o_c`コマンドの
  呼び出しです。

### 結論

カーネルビルドシステムの内部についての長い旅でした。それでも多くの詳細を
省略しました。この主題についてもっと詳しく知りたい方は、[このドキュメント](https://github.com/torvalds/linux/blob/v4.14/Documentation/kbuild/makefiles.txt)を
読み、Makefilesのソースコードを読み続けることをお勧めします。ここでは
この章で覚えておいてほしい重要な点を強調しておきます。

1. `.c`ファイルはどのようにオブジェクトファイルにコンパイルされるか。
2. オブジェクトファイルはどのように`built-in.o`ファイルにまとめられるか。
3. 再帰的ビルドはどのようにすべての子`builtin.o`ファイルを選び出し、1つの
ファイルにまとめるか。
4. `vmlinux`はどのようにすべてのトップレベル`builtin.o`ファイルからリンク
されるか。

私の主な目標は、この章を読んだ後に上記のすべてについて一般的な理解を
得られるようにすることでした。

##### 前ページ

1.2 [カーネルの初期化: Linuxプロジェクトの構造](../../../ja/lesson01/linux/project-structure.md)

##### 次ページ

1.4 [カーネルの初期化: Linux起動シーケンス](../../../ja/lesson01/linux/kernel-startup.md)
