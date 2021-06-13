## 1.2: Linuxプロジェクトの構成

今回は初めてLinuxについて話します。まず、自分たちのカーネルを書くための
小さなステップを完了させ、その後に同じことがLinuxでどのように動作しているかを
見てみようというものです。今のところ、私たちはほとんど何もしていません。
最初のベアメタルのhello worldプログラムを実装しただけです。それでも
RPi OSとLinuxにはいくつかの類似点があることがわかります。これからその
いくつかを探っていきます。

### プロジェクトの構成

大規模なソフトウェアプロジェクトの調査を始める際にはいつでもプロジェクトの
構造をざっと見てみることに価値があります。これは非常に重要です。どんなモジュールで
プロジェクトが構成されているか、高水準のアーキテクチャがどのように
なっているかを理解できるからです。それでは、Linuxカーネルのプロジェクトの構成を
調べてみましょう。

まず最初に、Linux リポジトリをクローンします。

```
git clone -b v4.14 --depth 1 https://github.com/torvalds/linux.git
```

ここでは`v4.14`を使用します。執筆時点での最新バージョンであったからです。
Linuxのソースコードを参照する場合はすべてこのバージョンを使用します。

次に、Linuxリポジトリにあるディレクトリを見てみましょう。ここでは
すべてのディレクトリを見るのではなく、最も重要と思われるディレクトリだけを
見ていきます。

* [arch](https://github.com/torvalds/linux/tree/v4.14/arch) このディレクトリ
  にはサブディレクトリがあり、それぞれが特定のプロセッサアーキテクチャ用に
  なっています。ここで見るのはほとんどが[arm64](https://github.com/torvalds/linux/tree/v4.14/arch/arm64)です。これはARM.v8プロセッサと互換性があるものです。
* [init](https://github.com/torvalds/linux/tree/v4.14/init) カーネルは
  常にアーキテクチャ固有のコードにより起動され、次に実行が[start_kernel](https://github.com/torvalds/linux/blob/v4.14/init/main.c#L509)
  関数に渡されます。この関数は一般的なカーネルの初期化を担当し、アーキテクチャ
  に依存しないカーネルの開始地点です。`start_kernel`関数は他の初期化関数と
  ともに`init`ディレクトリで定義されています。
* [kernel](https://github.com/torvalds/linux/tree/v4.14/kernel) Linuxカーネルの
  中核となるものです。ほとんどすべての主要なカーネルサブシステムはここで
  実装されています。
* [mm](https://github.com/torvalds/linux/tree/v4.14/mm) メモリ管理に関連
  するすべてのデータ構造とメソッドが定義されています。
* [drivers](https://github.com/torvalds/linux/tree/v4.14/drivers) Linux
  カーネルの中で最大のディレクトリです。すべてのデバイスドライバの実装が
  含まれています。
* [fs](https://github.com/torvalds/linux/tree/v4.14/fs) さまざまな
  ファイルシステムの実装をここで見ることができます。

以上の説明は非常に高水準のものですが今はこれで十分です。次の章では
Linuxのビルドシステムについて詳しく見ていきます。


##### 前ページ

1.1 [カーネル初期化: RPi OSの導入、ベアメタルで"Hello, World!"](../../../ja/lesson01/rpi-os.md)

##### 次ページ

1.3 [カーネル初期化: カーネルビルドシステム](../../../ja/lesson01/linux/build-system.md)
