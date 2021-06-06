# LinuxカーネルとRaspberry Piを使ってOS開発を学ぶ

このリポジトリではシンプルなオペレーティングシステム(OS)のカーネルを
一から作成する方法をステップバイステップで説明しています。私はこのOSを
Raspberry Pi OS、または単に、RPi OSと呼んでいます。RPi OSのソースコードの
大部分は[Linuxカーネル](https://github.com/torvalds/linux)をベースに
していますが、OSの機能は非常に限られており、[Raspberry PI 3](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/)に
しか対応していません。

各レッスンは、まずカーネルの機能がRPi OSでどのように実装されているかを説明し、次に同じ機能がLinuxカーネルでどのように動作しているかを示すにデザイン
されています。各レッスンには[src](https://github.com/s-matyukevich/raspberry-pi-os/tree/master/src)ディレクトリに対応するフォルダがあり、
そこにはレッスンが終了した時点でのOSのソースコードのスナップショットが
格納されています。このようにして、新しい概念を徐々に導入することができ、
読者はRPi OSの進化を追うことができます。このガイドを理解するのに特別な
OS開発のスキルは必要ありません。

プロジェクトの目標や歴史については[プロジェクトの紹介](translations/ja/Introduction.md)を
お読みください。このプロジェクトは現在も活発に開発が行われていますが、
参加したいと思われる方は[参加ガイド](translations/ja/Contributions.md)を読んでください。

<p>
  <a href="https://twitter.com/RPi_OS" target="_blank">
    <img src="https://raw.githubusercontent.com/s-matyukevich/raspberry-pi-os/master/images/twitter.png" alt="Follow @RPi_OS on twitter" height="34" >
  </a>

  <a href="https://www.facebook.com/groups/251043708976964/" target="_blank">
    <img src="https://raw.githubusercontent.com/s-matyukevich/raspberry-pi-os/master/images/facebook.png" alt="Follow Raspberry Pi OS on facebook" height="34" >
  </a>

  <a href="https://join.slack.com/t/rpi-os/shared_invite/enQtNDQ1NTg2ODc1MDEwLWVjMTZlZmMyZDE4OGEyYmMzNTY1YjljZjU5YWI1NDllOWEwMjI5YzVkM2RiMzliYjEzN2RlYmUzNzBiYmQyMjY" target="_blank">
    <img src="https://raw.githubusercontent.com/s-matyukevich/raspberry-pi-os/master/images/slack.png" alt="Join Raspberry Pi OS in slack" height="34" >
  </a>

  <a href="https://www.producthunt.com/upcoming/raspberry-pi-os" target="_blank">
    <img src="https://raw.githubusercontent.com/s-matyukevich/raspberry-pi-os/master/images/subscribe.png" alt="Subscribe for updates" height="34" >
  </a>
</p>

## Table of Contents

* **[プロジェクト紹介](translations/ja/Introduction.md)**
* **[Contribution](translations/ja/Contributions.md)**
* **[Prerequisites](translations/ja/Prerequisites.md)**
* **Lesson 1: カーネルの初期化**
  * 1.1 [RPi OSの導入、ベアメタルで"Hello, World!"](translations/ja/lesson01/rpi-os.md)
  * Linux
    * 1.2 [Linuxプロジェクトの構成](translations/ja/lesson01/linux/project-structure.md)
    * 1.3 [カーネルビルドシステム](translations/ja/lesson01/linux/build-system.md)
    * 1.4 [起動シーケンス](translations/ja/lesson01/linux/kernel-startup.md)
  * 1.5 [演習](translations/ja/lesson01/exercises.md)
* **Lesson 2: プロセッサの初期化**
  * 2.1 [RPi OS](translations/ja/lesson02/rpi-os.md)
  * 2.2 [Linux](translations/ja/lesson02/linux.md)
  * 2.3 [演習](translations/ja/lesson02/exercises.md)
* **Lesson 3: 割り込み処理**
  * 3.1 [RPi OS](translations/ja/lesson03/rpi-os.md)
  * Linux
    * 3.2 [低レベル例外処理](translations/ja/lesson03/linux/low_level-exception_handling.md)
    * 3.3 [割り込みコントローラ](translations/ja/lesson03/linux/interrupt_controllers.md)
    * 3.4 [Timers](translations/ja/lesson03/linux/timer.md)
  * 3.5 [演習](translations/ja/lesson03/exercises.md)
* **Lesson 4: プロセススケジューラ**
  * 4.1 [RPi OS](translations/ja/lesson04/rpi-os.md)
  * Linux
    * 4.2 [スケジューラの基本構造体](translations/ja/lesson04/linux/basic_structures.md)
    * 4.3 [タスクをフォークする](translations/ja/lesson04/linux/fork.md)
    * 4.4 [スケジューラ](translations/ja/lesson04/linux/scheduler.md)
  * 4.5 [演習](translations/ja/lesson04/exercises.md)
* **Lesson 5: ユーザプロセスとシステムコール**
  * 5.1 [RPi OS](translations/ja/lesson05/rpi-os.md)
  * 5.2 [Linux](translations/ja/lesson05/linux.md)
  * 5.3 [演習](translations/ja/lesson05/exercises.md)
* **Lesson 6: Virtual memory management**
  * 6.1 [RPi OS](translations/ja/lesson06/rpi-os.md)
  * 6.2 Linux (In progress)
  * 6.3 [Exercises](translations/ja/lesson06/exercises.md)
* **Lesson 7: Signals and interrupt waiting** (To be done)
* **Lesson 8: File systems** (To be done)
* **Lesson 9: Executable files (ELF)** (To be done)
* **Lesson 10: Drivers** (To be done)
* **Lesson 11: Networking** (To be done)
