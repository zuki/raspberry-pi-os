## 1.4: Linux起動シーケンス

### エントリポイントを探す

Linuxプロジェクトの構造を確認し、どのようにビルドされるかを検討したので、
次の論理的なステップは、プログラムのエントリポイントを見つけることです。
このステップは多くのプログラムにとっては些細なことかもしれませんが、
Linuxカーネルにとってはそうではありません。

まず最初に私たちがすることは[arm64リンカスクリプト](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/vmlinux.lds.S)を
見ることです。リンカスクリプトが主たるmakefileでそのように使われているかは
すでに見たとおりです。[この行](https://github.com/torvalds/linux/blob/v4.14/Makefile#L970)から特定のアーキテクチャ用のリンカスクリプトがどこに
あるかを簡単に推測することができます。

これから調べるファイルは実際のリンカスクリプトではないということをここで
述べておく必要があります。これはいくつかのマクロを実際の値に置き換えて
実際のリンカスクリプトを作るためのテンプレートです。しかし、正確には
このファイルはほとんどがマクロで構成されているため、非常に読みやすく、
異なるアーキテクチャへの移植も容易です。

リンカスクリプトの最初のセクションは[.head.text](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/vmlinux.lds.S#L96)と
呼ばれます。これは私たちにとって非常に重要です。エントリポイントはこの
セクションで定義されているからです。少し考えてみればそれは非常に理にかなって
います。カーネルがロードされた後、バイナリイメージの内容はあるメモリ領域に
コピーされ、その領域の先頭から実行が開始されるからです。これは`.head.text`
セクションを使ってものを検索すれば、エントリポイントを見つけることができる
ことを意味します。そして実際、`arm64`アーキテクチャには`.section    ".head.text","ax"`に展開される[__HEAD](https://github.com/torvalds/linux/blob/v4.14/include/linux/init.h#L90)
マクロを使っているファイルが1つあります。[head.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S)です。

`head.S`ファイルにある最初の実行可能な行が[`head.S#L85`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L85)です。
ここでは、armアセンブラの`b`（`branch`）命令を使って[stext](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L116)
関数にジャンプしています。そして、これがカーネル起動後に実行される
最初の関数です。

次の論理的ステップは`stext`関数の内部で何が行われているかを調べることですが、
それにはまだ準備ができていません。まず、同様の機能をRPi OSに実装する必要が
ありますが、これは次の数回のレッスンで取り上げるものです。今やるべきことは
カーネルブートに関連するいくつかの重要なコンセプトを調べることです。

### Linuxのブートローダとブートプロトコル

Linuxカーネルの起動時には、マシンのハードウェアが何らかの「既知の状態」に
なっていることを前提としています。この状態を定義する一連のルールは「ブート
プロトコル」と呼ばれており、`arm64`アーキテクチャの場合は[この文書](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt)に
記載されています。たとえば、実行はプライマリCPUでのみ開始すること、
メモリマッピングユニット（MMU）はオフになっていること、すべての割り込みは
無効になっていることなどが定義されています。

では、マシンをそのような状態にする責任は誰にあるのでしょうか？通常、
カーネルの前に実行され、すべての初期化を行う特別なプログラムがあります。
このプログラムは「ブートローダ」と呼ばれます。ブートローダのコードは
マシン固有の場合が多いですが、Raspberry PIの場合も同様です。Raspberry PI
にはボードに内蔵されているブートローダがあります。その動作をカスタマイズ
するには[config.txt](https://www.raspberrypi.org/documentation/configuration/config-txt/)を使うしかありません。

### UEFIブート

しかし、カーネルイメージ自体に組み込むことができるブートローダが1つあります。
このブートローダは[UEFI (Unified Extensible Firmware Interface)](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface)を
サポートしているプラットフォームでしか使用できません。UEFIをサポートする
デバイスは、実行中のソフトウェアに一連の標準化されたサービスを提供し、
それらのサービスをマシン自体とその機能に関するすべての必要な情報を把握する
ために使用することができます。また、UEFIはコンピュータのファームウェアが
[PE (Portable Executable)](https://en.wikipedia.org/wiki/Portable_Executable)
フォーマットの実行ファイルを実行できることを要求しています。Linuxカーネルの
UEFIブートローダはこの機能を利用しています。このローダは、コンピュータの
ファームウェアがイメージを通常の`PE`ファイルだと認識するようにLinuxカーネル
イメージの先頭に`PE`ヘッダを挿入します。これは[efi-header.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S)
ファイルで行われています。このファイルには[__EFI_PE_HEADER](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S#L13)
マクロが定義されており、[head.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L98)で使用されています。

`__EFI_PE_HEADER`で定義されている重要なプロパティの一つは[UEFIエントリポイントのロケーション](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-header.S#L33)を
示すものであり、エントリーポイント自体は [efi-entry.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/efi-entry.S#L32)に
あります。このロケーションからソースコードをたどることでUEFIブートローダが
何をしているのかを正確に調べることができます (ソースコード自体は多かれ少なかれ
簡単です)。しかし、このセクションの目的はUEFIブートローダを詳しく調べること
ではなく、UEFIが何であるか、Linuxカーネルがそれをどのように使用しているかに
ついての一般的なアイデアを与えることなので、ここで止めておきます。

### デバイスツリー

Linuxカーネルの起動コードを調べ始めると「デバイスツリー」に言及している記述を
数多く見ました。これは必須の概念のようです。私はこれについて議論する必要が
あると考えます。

`Raspberry PI OS`のカーネルを作成していた際、[BCM2837 ARM Peripherals manual](https://github.com/raspberrypi/documentation/files/1888662/BCM2837-ARM-Peripherals.-.Revised.-.V2-1.pdf)を
参考にして、特定のメモリマップドレジスタの正確なオフセットを調べました。
この情報は明らかにボードごとに異なるので、サポートしなければならないのは
そのうちの1つだけなのは幸運でした。しかし、何百ものボードをサポートしなければ
ならないとしたらどうでしょう。各ボードの情報をカーネルコードにハード
コーディングしようとすればまったくの混乱に陥るでしょう。仮にそれができたと
しても、今使っているボードが何であるかをどうやって知ることができるでしょうか。
たとえば、`BCM2837`はそのような情報を実行中のカーネルに伝える手段を提供
していません。

この問題を解決するのが上で述べたデバイスツリーです。デバイスツリーとは
コンピュータのハードウェアを記述するための特別なフォーマットです。
デバイスツリーの仕様は[こちら](https://www.devicetree.org/)を
ご覧ください。カーネルが実行される前に、ブートローダは適切なデバイスツリー
ファイルを選択し、カーネルに引数として渡します。Raspberry PI SDカードの
ブートパーティションにあるファイルを見るとたくさんの`.dtb`ファイルが
あります。`.dtb`はコンパイル済みのデバイスツリーファイルです。
`config.txt`でそれらの一部を選択することによりRaspberry PIのハードウェアを
有効にしたり無効にしたりすることができます。このプロセスについては
[Raspberry PIの公式ドキュメント](https://www.raspberrypi.org/documentation/configuration/device-tree.md)に詳しく説明されています。

では、実際のデバイスツリーがどのようになっているかを見てみましょう。
簡単な練習として、[Raspberry PI 3 Model B](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/)用の
デバイスツリーを見つけましょう。[ドキュメント](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2837/README.md)から、`Raspberry PI 3 Model B`は`BCM2837`というチップを使用していることがわかります。この名前で
検索すると[/arch/arm64/boot/dts/broadcom/bcm2837-rpi-3-b.dts](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/boot/dts/broadcom/bcm2837-rpi-3-b.dts)
ファイルが見つかります。ご覧のように、このファイルには`arm`アーキテクチャの
と同じファイルが含まれているだけです。`ARM.v8`プロセッサは32ビットモードも
サポートしているので、これは完全に理にかなっています。

次に、[bcm2837-rpi-3-b.dts](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts)は
[arm](https://github.com/torvalds/linux/tree/v4.14/arch/arm)アーキテクチャーに
属していることがわかります。デバイスツリーが別のデバイスツリーを含んでいる
場合があることは既に見ました。`bcm2837-rpi-3-b.dts`がそうでした。この
ファイルは`BCM2837`固有の定義だけを含んでおり、それ以外はすべて再利用して
います。たとえば、`bcm2837-rpi-3-b.dts`では[デバイスが1GBのメモリを搭載している](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts#L18)ことを明記しています。

先に述べたように、`BCM2837`と`BCM2835`の周辺ハードウェアは同じなので、
インクルードの連鎖をたどることにより、実際にこのハードウェアのほとんどを
定義している[bcm283x.dtsi](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi)を
見つけることができます。

デバイスツリーの定義はブロックの入れ子構造で構成されています。トップ
レベルには通常、[cpus](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L30)や
[memory](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts#L17)などのブロックがあります。これらのブロックの意味は
非常にわかりやすいと思います。`bcm283x.dtsi`で見られるもう一つの興味深い
トップレベル要素は[SoC](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L52)です。
これは[System on a chip](https://en.wikipedia.org/wiki/System_on_a_chip)を意味します。
これはすべてのペリフェラルデバイスはメモリマップドレジスタを介して直接
何らかのメモリ領域にマッピングされていることを示しています。`soc`要素は
すべてのペリフェラルデバイスの親要素を提供しています。その子要素の一つが
[gpio](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L147)要素です。
この要素は[reg = <0x7e200000 0xb4>](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L149)というプロパティを定義しており、
GPIOのメモリマップドレジスタが`[0x7e200000 : 0x7e2000b4]`の領域にあることを
示しています。`gpio`要素の子要素の一つに[次の定義](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L474)があります。

```
uart1_gpio14: uart1_gpio14 {
        brcm,pins = <14 15>;
        brcm,function = <BCM2835_FSEL_ALT5>;
};
```

この定義は、ピン 14と15に代替機能 5が選択された場合、これらのピンが`uart1`
デバイスに接続されることを示しています。`uart1`デバイスはこれまでに使用
してきたMini UARTであることは容易に推測できます。

デバイスツリーについて知っておくべき重要なことは、そのフォーマットは拡張
可能であることです。各デバイスは独自のプロパティやネストされたブロックを
定義することができます。それらのプロパティはデバイスドライバに透過的に
渡され、それを解釈するのはドライバの責任です。では、カーネルはデバイス
ツリーのブロックと正しいドライバとの対応関係をどのように把握するので
しょうか。カーネルは`compatible`プロパティを使ってこれを行います。
たとえば、`uart1`デバイスの`compatible`プロパティは次のように指定されて
います。

```
compatible = "brcm,bcm2835-aux-uart";
```

実際、Linuxのソースコードで`bcm2835-aux-uart`を検索すると、一致する
ドライバが見つかります。これは[8250_bcm2835aux.c](https://github.com/torvalds/linux/blob/v4.14/drivers/tty/serial/8250/8250_bcm2835aux.c)で
定義されています。

### 結論

本章は、`arm64`のブートコードを読むための準備と考えることができます。
ここで説明してきた概念を理解していなければ、それを学ぶことは難しいで
しょう。次のレッスンでは`stext`関数に戻って、その動作を詳しく調べます。

##### 前ページ

1.3 [カーネルの初期化: カーネルビルドシステム](../../../docs/lesson01/linux/build-system.md)

##### 次ページ

1.5 [カーネルの初期化: 演習](../../../docs/lesson01/exercises.md)
