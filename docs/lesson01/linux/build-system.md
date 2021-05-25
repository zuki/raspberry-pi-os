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

  ネストしたMakefileの例は[ここ](https://github.com/torvalds/linux/tree/v4.14/kernel/Makefile)にあります。

* 先に進む前に、基本的なmakeルールの構造を理解し、make用語に慣れておく
  必要があります。一般的なルールの構造を以下に図示します。

  ```
  targets : prerequisites
          recipe
          …
  ```

    * `target`は、空白で区切られたファイル名です。targetはルールの
    実行後に生成されます。通常、1つのルールには1つのtargetだけです。。
    * `prerequisites`は、ターゲットを更新する必要があるかどうかを`make`が
    確認するためのファイルです。
    * `recipe`は、bashスクリプトです。makeはprerequisitesの一部が更新されると
    このスクリプトを呼び出します。recipeはtargetの生成を担当します。
    * targetとprerequisitesにはワイルドカード（`%`）を含めることができます。
    ワイルドカードが使用されると、recipeはマッチしたprerequisitesのそれぞれに
    対して個別に実行されます。この場合、recipe内でprerequisiteとtargetを
    参照するために各々、変数`$<`と`$@`を使用することができます。これは
    [RPi OSのmakefile](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson01/Makefile#L14)ですでに使用しています。

  makeルールの詳細については、[奇しきドキュメント](https://www.gnu.org/software/make/manual/html_node/Rule-Syntax.html#Rule-Syntax)を参照してください。

* `make`は、前提条件が変更されていないかどうかを検出し、再構築が必要な
  ターゲットだけを更新することは得意ですが、レシピが動的に更新されると
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
  呼び出して対応する`.o`ファイルをビルドします。`if_changed`は`cmd_compile`
  変数を探し（最初の引数に`cmd_`というプレフィックスを追加します）、この変数が
  前回の実行後に更新されていないか、また、前提条件が変更されていないかを
  チェックします。変更されている場合、`cmd_compile`コマンドが実行され
  オブジェクトファイルが再生成されます。このサンプルルールでは、2つの前提
  条件があります。ソースファイル`.c`と`FORCE`です。`FORCE`は特別な前提条件で
  あり、`make`コマンドが呼び出されるたびにレシピが強制的に呼び出されます。
  これがないと、`.c`ファイルが変更された場合にのみレシピが呼び出されます。
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

* `vmlinux`ターゲットの定義は[ここ](https://github.com/torvalds/linux/blob/v4.14/Makefile#L1004)にあり、以下のようになっています。

  ```
  cmd_link-vmlinux =                                                 \
      $(CONFIG_SHELL) $< $(LD) $(LDFLAGS) $(LDFLAGS_vmlinux) ;    \
      $(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

  vmlinux: scripts/link-vmlinux.sh vmlinux_prereq $(vmlinux-deps) FORCE
      +$(call if_changed,link-vmlinux)
  ```

  This target uses already familiar to us `if_changed` function. Whenever some of the prerequsities are updated `cmd_link-vmlinux` command is executed. This command executes [scripts/link-vmlinux.sh](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh) script (Note usage of [$<](https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html) automatic variable in the `cmd_link-vmlinux` command). It also executes architecture specific [postlink script](https://github.com/torvalds/linux/blob/v4.14/Documentation/kbuild/makefiles.txt#L1229), but we are not very interested in it.

* When [scripts/link-vmlinux.sh](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh) is executed it assumes that all required object files are already built and their locations are stored in 3 variables: `KBUILD_VMLINUX_INIT`, `KBUILD_VMLINUX_MAIN`, `KBUILD_VMLINUX_LIBS`.

* `link-vmlinux.sh` script first creates `thin archive` from all available object files. `thin archive` is a special object that contains references to a set of object files as well as their combined symbol table. This is done inside [archive_builtin](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L56) function. In order to create `thin archive` this function uses [ar](https://sourceware.org/binutils/docs/binutils/ar.html) utility. Generated `thin archive` is stored as `built-in.o` file and has the format that is understandable by the linker, so it can be used as any other normal object file.

* Next [modpost_link](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L69) is called. This function calls linker and generates `vmlinux.o` object file. We need this object file to perform [Section mismatch analysis](https://github.com/torvalds/linux/blob/v4.14/lib/Kconfig.debug#L308). This analysis is performed by the [modpost](https://github.com/torvalds/linux/tree/v4.14/scripts/mod) program and is triggered at [this](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L260) line.

* Next kernel symbol table is generated. It contains information about all functions and global variables as well as their location in the `vmlinux` binary. The main work is done inside [kallsyms](https://github.com/torvalds/linux/blob/v4.14/scripts/link-vmlinux.sh#L146) function. This function first uses [nm](https://sourceware.org/binutils/docs/binutils/nm.html) to extract all symbols from `vmlinux` binary. Then it uses [scripts/kallsyms](https://github.com/torvalds/linux/blob/v4.14/scripts/kallsyms.c)  utility to generate a special assembler file containing all symbols in a special format, understandable by the Linux kernel. Next, this assembler file is compiled and linked together with the original binary.  This process is repeated several times because after the final link addresses of some symbols can be changed.  Information from the kernel symbol table is used to generate '/proc/kallsyms' file at runtime.

* Finally `vmlinux` binary is ready and `System.map`  is build. `System.map` contains the same information as `/proc/kallsyms` but this is static file and unlike `/proc/kallsyms` it is not generated at runtime. `System.map` is mostly used to resolve addresses to symbol names during [kernel oops](https://en.wikipedia.org/wiki/Linux_kernel_oops). The same `nm` utility is used to build `System.map`. This is done [here](https://github.com/torvalds/linux/blob/v4.14/scripts/mksysmap#L44).

#### Build stage

* Now let's take one step backward and examine how source code files are compiled into object files. As you might remember one of the prerequisites of the `vmlinux` target is `$(vmlinux-deps)` variable. Let me now copy a few relevant lines from the main Linux makefile to demonstrate how this variable is built.

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

  It all starts with variables like `init-y`, `core-y`, etc., which combined contains all subfolders of the Linux kernel that contains buildable source code. Then `built-in.o` is appended to all the subfolder names, so, for example, `drivers/` becomes `drivers/built-in.o`. `vmlinux-deps` then just aggregates all resulting values. This explains how `vmlinux` eventually becomes dependent on all `built-in.o` files.

* Next question is how all `built-in.o` objects are created? Once again, let me copy all relevant lines and explain how it all works.

  ```
  $(sort $(vmlinux-deps)): $(vmlinux-dirs) ;

  vmlinux-dirs    := $(patsubst %/,%,$(filter %/, $(init-y) $(init-m) \
               $(core-y) $(core-m) $(drivers-y) $(drivers-m) \
               $(net-y) $(net-m) $(libs-y) $(libs-m) $(virt-y)))

  build := -f $(srctree)/scripts/Makefile.build obj               #Copied from `scripts/Kbuild.include`

  $(vmlinux-dirs): prepare scripts
      $(Q)$(MAKE) $(build)=$@

  ```

  The first line tells us that `vmlinux-deps` depends on `vmlinux-dirs`. Next, we can see that `vmlinux-dirs` is a variable that contains all direct root subfolders without `/` character at the end. And the most important line here is the recipe to build `$(vmlinux-dirs)` target. After substitution of all variables, this recipe will look like the following (we use `drivers` folder as an example, but this rule will be executed for all root subfolders)

  ```
  make -f scripts/Makefile.build obj=drivers
  ```

  This line just calls another makefile  ([scripts/Makefile.build](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build)) and passes `obj` variable, which contains a folder to be compiled.

* Next logical step is to take a look at [scripts/Makefile.build](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build). The first important thing that happens after it is executed is that all variables from `Makefile` or `Kbuild` files, defined in the current directory, are included. By current directory I mean the directory referenced by the `obj` variable. The inclusion is done in the [following 3 lines](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L43-L45).

  ```
  kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))
  kbuild-file := $(if $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Kbuild,$(kbuild-dir)/Makefile)
  include $(kbuild-file)
  ```
  Nested makefiles are mostly responsible for initializing variables like `obj-y`. As a quick reminder: `obj-y` variable should contain list of all source code files, located in the current directory. Another important variable that is initialized by the nested makefiles is `subdir-y`. This variable contains a list of all subfolders that need to be visited before the source code in the curent directory can be built. `subdir-y` is used to implement recursive descending into subfolders.

* When `make` is called without specifying the target (as it is in the case when `scripts/Makefile.build` is executed) it uses the first target. The first target for `scripts/Makefile.build` is called `__build` and it can be found [here](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L96) Let's take a look at it.

  ```
  __build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
       $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
       $(subdir-ym) $(always)
      @:
  ```

  As you can see `__build` target doesn't have a receipt, but it depends on a bunch of other targets. We are only interested in `$(builtin-target)` - it is responsible for creating `built-in.o` file, and `$(subdir-ym)` - it is responsible for descending into nested directories.

* Let's take a look at `subdir-ym`. This variable is initialized [here](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.lib#L48) and it is just a concatenation of `subdir-y` and `subdir-m` variables.  (`subdir-m` variable is similar to `subdir-y`, but it defines subfolders need to be included in a separate [kernel module](https://en.wikipedia.org/wiki/Loadable_kernel_module). We skip the discussion of modules, for now, to keep focused.)

*  `subdir-ym` target is defined [here](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L572) and should look familiar to you.

  ```
  $(subdir-ym):
      $(Q)$(MAKE) $(build)=$@
  ```

  This target just triggers execution of the `scripts/Makefile.build` in one of the nested subfolders.

* Now it is time to examine the [builtin-target](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L467) target. Once again I am copying only relevant lines here.

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

  This target depends on `$(obj-y)` target and `obj-y` is a list of all object files that need to be built in the current folder. After those files become ready `cmd_link_o_target` command is executed. In case if `obj-y` variable is empty `cmd_make_empty_builtin` is called, which just creates an empty `built-in.o`. Otherwise, `cmd_make_builtin` command is executed; it uses familiar to us `ar` tool to create `built-in.o` thin archive.

* Finally we got to the point where we need to compile something. You remember that our last unexplored dependency is `$(obj-y)` and `obj-y` is just a list of object files. The target that compiles all object files from corresponding `.c` files is defined [here](https://github.com/torvalds/linux/blob/v4.14/scripts/Makefile.build#L313). Let's examine all lines, needed to understand this target.

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

  Inside it's recipe this target calls `rule_cc_o_c`. This rule is responsible for a lot of things, like checking the source code for some common errors (`cmd_checksrc`), enabling versioning for exported module symbols (`cmd_modversions_c`), using [objtool](https://github.com/torvalds/linux/tree/v4.14/tools/objtool) to validate some aspects of generated object files and constructing a list of calls to `mcount` function so that [ftrace](https://github.com/torvalds/linux/blob/v4.14/Documentation/trace/ftrace.txt) can find them quickly. But most importantly it calls `cmd_cc_o_c` command that actually compiles all `.c` files to object files.

### Conclusion

Wow, it was a long journey inside kernel build system internals! Still, we skipped a lot of details and, for those who want to learn more about the subject, I can recommend to read the following [document](https://github.com/torvalds/linux/blob/v4.14/Documentation/kbuild/makefiles.txt) and continue reading Makefiles source code. Let me now emphasize the important points, that you should take as a take-home message from this chapter.

1. How `.c` files are compiled into object files.
1. How object files are combined into `built-in.o` files.
1. How  recursive build pick up all child `built-in.o` files and combines them into a single one.
1. How `vmlinux` is linked from all top-level `built-in.o` files.

My main goal was that after reading this chapter you will gain a general understanding of all above points.

##### Previous Page

1.2 [Kernel Initialization: Linux project structure](../../../docs/lesson01/linux/project-structure.md)

##### Next Page

1.4 [Kernel Initialization: Linux startup sequence](../../../docs/lesson01/linux/kernel-startup.md)
