## 前提条件

### 1. [Raspberry Pi 3 Model B](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/)

これより古いバージョンのRaspberry Piは、このチュートリアルでは動作しません。なぜなら、
すべてのレッスンはARMv8アーキテクチャがサポートする64ビットプロセッサを使用するように
設計されており、そのようなプロセッサはRaspberry Pi 3にしか搭載されていないからです。
[Raspberry Pi 3 Model B+](https://www.raspberrypi.org/products/raspberry-pi-3-model-b-plus/)など、これより新しいバージョンであれば問題なく動作する
はずですが、まだテストしていません。

### 2. [USB-TTLシリアルケーブル](https://www.amazon.com/s/ref=nb_sb_noss_2?url=search-alias%3Daps&field-keywords=usb+to+ttl+serial+cable&rh=i%3Aaps%2Ck%3Ausb+to+ttl+serial+cable)

シリアルケーブルを入手したら、接続をテストする必要があります。まだテストをしていない
方はテストをする前に[このガイド](https://cdn-learn.adafruit.com/downloads/pdf/adafruits-raspberry-pi-lesson-5-using-a-console-cable.pdf)を読むことを勧めます。
このガイドにはシリアルケーブルを使ってRaspberry PIを接続する方法が詳しく説明されて
います。

また、このガイドには、シリアルケーブルを使ってRaspberry Piに電源を供給する方法も
説明されています。RPi OSはこのような設定でも問題なく動作しますが、この場合、ケーブルを
接続した直後にターミナルエミュレータを起動する必要があります。詳しくは[このレッスン](https://github.com/s-matyukevich/raspberry-pi-os/issues/2)をご覧ください。

### 3. [Raspbian OS](https://www.raspberrypi.org/downloads/raspbian/)がインストールされた
[SDカード](https://www.raspberrypi.org/documentation/installation/sd-cards.md)

最初にUSB-TTLケーブルの接続をテストするためにはRaspbianが必要です。もう一つの理由は、
インストールするとSDカードが正しい方法でフォーマットされるからです。

### 4. Docker

厳密に言えば、Dockerは必ずしも必要ではありません。ただ、Dockerを使うと、レッスンの
ソースコードのビルドが簡単になります。特に、MacやWindowsユーザには便利です。
各レッスンにはbuild.shスクリプト（Windowsユーザにはbuild.bat）があります。この
スクリプトはDockerを使ってレッスンのソースコードをビルドします。使用しているプラット
フォームにdockerをインストールする方法は[dockerの公式サイト](https://docs.docker.com/engine/installation/)にあります。

何らかの理由でDockerの使用を避けたい場合は、`aarch64-linux-gnu`ツールチェインと
[makeユーティリティ](http://www.math.tau.ac.il/~danha/courses/software1/make-intro.html)をインストールしてください。Ubuntuを使用している場合は、`gcc-arch64-linux-gnu`と`build-essential`パッケージをインストールするだけです。

##### 前ページ

[貢献ガイド](../ja/Contributions.md)

##### 次ページ

1.1 [カーネルの初期化: RPi OSの導入、ベアメタルで"Hello, World!"](lesson01/rpi-os.md)
