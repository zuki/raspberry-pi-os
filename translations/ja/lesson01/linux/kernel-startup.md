## 1.4: Linux起動シーケンス

### エントリポイントを探す

Linuxプロジェクトの構造を確認し、どのようにビルドされるかを検討したので、
次の論理的なステップは、プログラムのエントリポイントを見つけることです。
このステップは多くのプログラムにとっては些細なことかもしれませんが、
Linuxカーネルにとってはそうではありません。

まず最初に私たちがすることは[arm64リンカスクリプト](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/vmlinux.lds.S)を
見ることです。リンカスクリプトがメインのmakefileでどのように使われているかは
すでに見たとおりです。[`Makefile#L970`](https://github.com/torvalds/linux/blob/v4.14/Makefile#L970)から特定のアーキテクチャ用のリンカスクリプトがどこに
あるかを簡単に推測することができます。

これから調べるファイルは実際のリンカスクリプトではないということをここで
述べておく必要があります。これはいくつかのマクロを実際の値に置き換えて
実際のリンカスクリプトを作るためのテンプレートです。しかし、正確には
このファイルはほとんどがマクロで構成されているため、非常に読みやすく、
異なるアーキテクチャへの移植も容易です。

リンカスクリプトの最初のセクションは[.head.text](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/vmlinux.lds.S#L96)です。
これは私たちにとって非常に重要です。エントリポイントはこのセクションで定義されている
はずだからです。少し考えてみればそれは非常に理にかなっています。カーネルがロードされた後、
バイナリイメージの内容はあるメモリ領域にコピーされ、その領域の先頭から実行が開始されるからです。
つまり、`.head.text`セクションを使っているものを検索するだけでエントリポイントを見つけることが
できるはずです。実際、`arm64`アーキテクチャには`.section    ".head.text","ax"`に展開される[__HEAD](https://github.com/torvalds/linux/blob/v4.14/include/linux/init.h#L90)
マクロを使っているファイルが1つだけあります。[head.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S)です。

`head.S`ファイルにある実行可能な最初の行が[`head.S#L85`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L85)です。
ここでは、armアセンブラの`b`（`branch`）命令を使って[stext](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L116)
関数にジャンプしています。そして、これがカーネル起動後に実行される
最初の関数です。

次の論理的ステップは`stext`関数の内部で何が行われているかを調べることですが、
それにはまだ準備ができていません。まず、同様の機能をRPi OSに実装する必要が
ありますが、これは次の数回のレッスンで取り上げるものです。今やるべきことは
カーネルブートに関連するいくつかの重要な概念を調べることです。

### Linuxのブートローダとブートプロトコル

Linuxカーネルの起動時には、マシンのハードウェアが何らかの「既知の状態」に
なっていることを前提としています。この状態を定義する一連のルールは「ブート
プロトコル」と呼ばれており、`arm64`アーキテクチャの場合は[この文書](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt)に
記載されています。たとえば、実行はプライマリCPUでのみ開始すること、
メモリマッピングユニット（MMU）はオフになっていること、すべての割り込みは
無効になっていることなどが定義されています。

では、マシンをそのような状態にする責任は誰にあるのでしょうか。通常、
カーネルの前に実行され、すべての初期化を行う特別なプログラムがあります。
このプログラムは「ブートローダ」と呼ばれます。ブートローダのコードは
マシン固有の場合が多いのですが、Raspberry PIの場合も同様です。Raspberry PI
にはボードに内蔵されているブートローダがあります。その動作をカスタマイズ
するには[config.txt](https://www.raspberrypi.org/documentation/configuration/config-txt/)を使うしかありません。

### UEFIブート

しかし、カーネルイメージに組み込むことができるブートローダが1つあります。
このブートローダは[UEFI (Unified Extensible Firmware Interface)](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface)を
サポートしているプラットフォームでしか使用できません。UEFIをサポートする
デバイスは実行中のソフトウェアに一連の標準化されたサービスを提供します。そして、
これらのサービスを使用してマシンとその機能に関する必要なすべての情報を把握する
ことができます。UEFIはコンピュータのファームウェアが
[PE (Portable Executable)](https://en.wikipedia.org/wiki/Portable_Executable)
フォーマットの実行ファイルを実行できることも要求します。Linuxカーネルの
UEFIブートローダはこの機能を利用しています。このローダは、コンピュータの
ファームウェアがイメージを通常の`PE`ファイルだと認識するようにLinuxカーネル
イメージの先頭に`PE`ヘッダを挿入します。これは[efi-header.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S)
ファイルで行われています。このファイルには[__EFI_PE_HEADER](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S#L13)
マクロが定義されており、[head.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L98)で使用されます。

`__EFI_PE_HEADER`で定義されている重要なプロパティの一つは[UEFIエントリポイントのロケーション](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S#L33)を
示すプロパティです。そして、このエントリーポイント自体は [efi-entry.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-entry.S#L32)に
あります。このロケーションからソースコードをたどることでUEFIブートローダが
何をしているのかを正確に調べることができます (ソースコード自体はそれほど難しくは
ありません)。しかし、この節の目的はUEFIブートローダを詳しく調べることではなく、
UEFIが何であるか、Linuxカーネルがそれをどのように使用しているかについての概要を
提供することなので、ここで止めておきます。

### デバイスツリー

Linuxカーネルの起動コードを調べていくと「デバイスツリー」に言及している記述を
数多く見ました。これは必須の概念のようなので、これについて議論する必要が
あると考えます。

`Raspberry PI OS`のカーネルを作成する際、[BCM2837 ARM Peripherals manual](https://github.com/raspberrypi/documentation/files/1888662/BCM2837-ARM-Peripherals.-.Revised.-.V2-1.pdf)を
使って、特定のメモリマップドレジスタが位置する正確なオフセットを調べました。
この情報は明らかにボードごとに異なります。私たちはそのうちの1つしかサポート
する必要がなかったので良かったのですが、何百ものボードをサポートしなければ
ならないとしたらどうでしょう。各ボードの情報をカーネルコードにハード
コーディングしようとすればまったくの混乱に陥るでしょう。また、仮にそれができたと
しても、今使っているボードが何であるかをどうやって知ることができるでしょうか。
たとえば、`BCM2837`はそのような情報を実行中のカーネルに伝える手段を提供していません。

この問題を解決するのが上で述べたデバイスツリーです。デバイスツリーとは
コンピュータのハードウェアを記述するための特別なフォーマットです。
デバイスツリーの仕様は[こちらのサイト](https://www.devicetree.org/)を
ご覧ください。カーネルが実行される前に、ブートローダは適切なデバイスツリー
ファイルを選択し、カーネルに引数として渡します。Raspberry PI SDカードの
ブートパーティションにあるファイルを見るとたくさんの`.dtb`ファイルが
あります。`.dtb`はコンパイル済みのデバイスツリーファイルです。
`config.txt`でそれらを選択することによりRaspberry PIのハードウェアを
有効にしたり無効にしたりすることができます。このプロセスについては
[Raspberry PIの公式ドキュメント](https://www.raspberrypi.org/documentation/configuration/device-tree.md)に詳しく説明されています。

では、実際のデバイスツリーがどのようになっているかを見てみましょう。
簡単な練習として、[Raspberry PI 3 Model B](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/)用の
デバイスツリーを見つけましょう。[ドキュメント](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2837/README.md)から、`Raspberry PI 3 Model B`は`BCM2837`というチップを使用していることがわかります。この名前で
検索すると[/arch/arm64/boot/dts/broadcom/bcm2837-rpi-3-b.dts](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/boot/dts/broadcom/bcm2837-rpi-3-b.dts)
ファイルが見つかります。中を見るとこのファイルは`arm`アーキテクチャの同名のファイルを
インクルードしているだけです。`ARM.v8`プロセッサは32ビットモードもサポートしているので、
これは完全に理にかなっています。

そして、[arm](https://github.com/torvalds/linux/tree/v4.14/arch/arm)アーキテクチャーに
ある[bcm2837-rpi-3-b.dts](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts)を見つけることができます。デバイスツリーが別のデバイスツリーをインクルード
できることは既に見ました。`bcm2837-rpi-3-b.dts`もそうしています。このファイルでは`BCM2837`固有の
定義だけをしており、それ以外はすべて他の定義を再利用しています。たとえば、`bcm2837-rpi-3-b.dts`
では[デバイスが1GBのメモリを搭載している](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts#L18)ことを明記しています。

先に述べたように、`BCM2837`と`BCM2835`の周辺ハードウェアは同じなので、インクルードの連鎖を
たどることにより、実際にこのハードウェアのほとんどを定義している
[bcm283x.dtsi](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi)を
見つけることができます。

デバイスツリーの定義はブロックの入れ子構造で構成されています。トップレベルには通常、
[cpus](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L30)や
[memory](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts#L17)
などのブロックがあります。これらのブロックの意味は非常にわかりやすいと思います。
`bcm283x.dtsi`に見られるもう一つの興味深いトップレベル要素は
[SoC](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L52)です。
SoCは[System on a chip](https://en.wikipedia.org/wiki/System_on_a_chip)を意味します。
この定義はすべてのペリフェラルデバイスはメモリマップドレジスタを介して直接何らかのメモリ領域に
マッピングされていることを示しています。`soc`要素はすべてのペリフェラルデバイスの親要素です。
その子要素の1つが[gpio](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L147)要素です。
この要素は[reg = <0x7e200000 0xb4>](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L149)というプロパティを定義しており、
これはGPIOのメモリマップドレジスタが`[0x7e200000 : 0x7e2000b4]`の領域にあることを
示しています。`gpio`要素の子要素に[次の定義](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L474)があります。

```
uart1_gpio14: uart1_gpio14 {
        brcm,pins = <14 15>;
        brcm,function = <BCM2835_FSEL_ALT5>;
};
```

この定義は、ピン 14と15に代替機能 5が選択された場合、これらのピンが`uart1`デバイスに接続される
ことを示しています。`uart1`デバイスはこれまでに使用してきたMini UARTであることは容易に推測
できます。

デバイスツリーについて知っておくべき重要なことは、そのフォーマットが拡張可能であることです。
各デバイスは独自のプロパティやネストされたブロックを定義することができます。それらの
プロパティはデバイスドライバに透過的に渡され、それを解釈するのはドライバの責任です。
では、カーネルはデバイスツリーのブロックと正しいドライバとの対応関係をどのように把握するので
しょうか。カーネルは`compatible`プロパティを使ってこれを行います。たとえば、`uart1`デバイスの`compatible`プロパティは次のように指定されています。

```
compatible = "brcm,bcm2835-aux-uart";
```

実際、Linuxのソースコードで`bcm2835-aux-uart`を検索すると、一致するドライバが見つかります。
これは[8250_bcm2835aux.c](https://github.com/torvalds/linux/blob/v4.14/drivers/tty/serial/8250/8250_bcm2835aux.c)で
定義されています。

### 結論

本章は、`arm64`のブートコードを読むための準備と考えることができます。ここで説明してきた
概念を理解していなければそれを学ぶことは難しいでしょう。次のレッスンでは`stext`関数に
戻って、その動作を詳しく調べます。

##### 前ページ

1.3 [カーネルの初期化: カーネルビルドシステム](../../../ja/lesson01/linux/build-system.md)

##### 次ページ

1.5 [カーネルの初期化: 演習](../../../ja/lesson01/exercises.md)
