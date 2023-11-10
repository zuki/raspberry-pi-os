## 6.1: 仮想メモリ管理

RPi OSは現在、ユーザープロセスの実行とスケジュールができますが、プロセス間の隔離は
完全ではなく、すべてのプロセスとカーネルが同じメモリを共有しています。このため、
どのプロセスも他のプロセスのデータはもちろん、カーネルのデータにさえ簡単にアクセス
できてしまいます。また、たとえすべてのプロセスが悪意を持っていないと仮定しても、
別の欠点があります。各プロセスはメモリを割り当てる前にどのメモリ領域がすでに占有
されているかを知る必要があり、これがプロセスのメモリ割り当てをより複雑にします。

### 変換過程

今回のレッスンでは、仮想メモリを導入することにより上記の問題点をすべて解決します。
仮想メモリは、各プロセスに利用可能なすべてのメモリを占有していると思わせる抽象化を
提供します。プロセスはあるメモリ位置にアクセスする必要がある場合は常に仮想アドレスを
使用し、それが物理アドレスに変換されます。この変換過程はプロセスにとっては完全に
透過的に行われ、MMU（Memory Mapping Unit）という特別なデバイスによって実行されます。
MMUは変換テーブルを使用して仮想アドレスを物理アドレスに変換します。変換過程は
次の図で説明できます。

```
                           Virtual address                                                                 Physical Memory
+-----------------------------------------------------------------------+                                +------------------+
|         | PGD Index | PUD Index | PMD Index | PTE Index | Page offset |                                |                  |
+-----------------------------------------------------------------------+                                |                  |
63        47     |    38      |   29     |    20    |     11      |     0                                |     Page N       |
                 |            |          |          |             +--------------------+           +---->+------------------+
                 |            |          |          +---------------------+            |           |     |                  |
          +------+            |          |                                |            |           |     |                  |
          |                   |          +----------+                     |            |           |     |------------------|
+------+  |        PGD        |                     |                     |            +---------------->| Physical address |
| ttbr |---->+-------------+  |           PUD       |                     |                        |     |------------------|
+------+  |  |             |  | +->+-------------+  |          PMD        |                        |     |                  |
          |  +-------------+  | |  |             |  | +->+-------------+  |          PTE           |     +------------------+
          +->| PUD address |----+  +-------------+  | |  |             |  | +->+--------------+    |     |                  |
             +-------------+  +--->| PMD address |----+  +-------------+  | |  |              |    |     |                  |
             |             |       +-------------+  +--->| PTE address |----+  +-------------_+    |     |                  |
             +-------------+       |             |       +-------------+  +--->| Page address |----+     |                  |
                                   +-------------+       |             |       +--------------+          |                  |
                                                         +-------------+       |              |          |                  |
                                                                               +--------------+          +------------------+
```

この図とメモリ変換過程を理解するためには以下の事実が重要です。

* プロセス用のメモリは常にページ単位で割り当てられます。ページとは4KBサイズの連続した
メモリ領域のことです（ARMプロセッサはより大きなページをサポートしていますが、4KBが
最も一般的なケースなのでここではこのページサイズに限定して説明します）。
* ページテーブルは階層構造になっています。どのテーブルのエントリも階層内の次のテーブルの
アドレスを格納しています。
* テーブル階層は次の4レベルです。PGD (Page Global Directory)、PUD (Page
Upper Directory)、PMD (Page Middle Directory)、PTE (Page Table Entry)です。PTEが
階層構造の最後のテーブルであり、物理メモリ上の実際のページを指しています。
* メモリ変換過程は、PGDテーブルのアドレスを位置づけることから始まります。この
テーブルのアドレスは`ttbr0_el1`レジスタに格納されます。各プロセスはPGDを含む
すべてのページテーブルの独自のコピーを持つので、各プロセスはPGDアドレスを保持
しなければなりません。コンテキストスイッチの際には、次のプロセスのPGDアドレスが`ttbr0_el1`レジスタにロードされます。
* 次に、MMUはPGDポインタと仮想アドレスを使って対応する物理アドレスを計算します。
すべての仮想アドレスは利用可能な64ビットのうち48ビットのみを使用します。変換を行う際、
MMUはアドレスを次の4つの部分に分割します。
  * ビット [39 - 47] はPGDテーブルのインデックスです。MMUはこのインデックスを使って、
  PUDの位置を見つけます。
  * ビット [30 - 38] はPUDテーブルのインデックスです。MMUはこのインデックスを使って、
  PMDの位置を見つけます。
  * ビット [21 - 29] はPMDテーブルのインデックスです。MMUはこのインデックスを使って、
  PTEの位置を見つけます。
  * ビット [12 - 20] はPTEテーブルのインデックスです。MMUはこのインデックスを使って、
  物理メモリのページを見つけます。
  * ビット [0 - 11] は物理ページ内のオフセットです。MMUはこのオフセットを使って、
  先に見つけたページの仮想アドレスに対応する正確な位置を決定します。

では、ちょっとした練習として、ページテーブルのサイズを計算してみましょう。上の図
から、ページテーブルのインデックスには9ビットが使われえていることがわかります
（これはすべてのページテーブルレベルに当てはまります）。つまり、各ページテーブルには
`2^9 = 512`個のエントリがあることになります。ページテーブルの各エントリは階層内の次の
ページテーブルのアドレスか、PTEの場合は物理ページのアドレスです。私たちは64ビットの
プロセッサを使用していますので、各アドレスは64ビット、つまり8バイトのサイズで
なければなりません。これらを総合すると、ページテーブルのサイズは`512×8 ＝ 4096`
バイト（4KB）となります。これはちょうど1ページの大きさです。MMUの設計者がなぜ
このような数字を選んだのかが直感的に理解できるのではないでしょうか。

### セクションマッピング

ソースコードを見る前にもうひとつセクションマッピングについて説明しておきます。時には、
連続した物理メモリの大きな領域をマッピングする必要がある場合もあります。この場合、
4KBのページではなく、セクションと呼ばれる2MBのブロックを直接マッピングすることが
できます。これにより、1レベルの変換を省略することができます。この場合の変換図は
次のようになります。

```
                           Virtual address                                               Physical Memory
+-----------------------------------------------------------------------+              +------------------+
|         | PGD Index | PUD Index | PMD Index |      Section offset     |              |                  |
+-----------------------------------------------------------------------+              |                  |
63        47     |    38      |   29     |    20            |           0              |    Section N     |
                 |            |          |                  |                    +---->+------------------+
                 |            |          |                  |                    |     |                  |
          +------+            |          |                  |                    |     |                  |
          |                   |          +----------+       |                    |     |------------------|
+------+  |        PGD        |                     |       +------------------------->| Physical address |
| ttbr |---->+-------------+  |           PUD       |                            |     |------------------|
+------+  |  |             |  | +->+-------------+  |            PMD             |     |                  |
          |  +-------------+  | |  |             |  | +->+-----------------+     |     +------------------+
          +->| PUD address |----+  +-------------+  | |  |                 |     |     |                  |
             +-------------+  +--->| PMD address |----+  +-----------------+     |     |                  |
             |             |       +-------------+  +--->| Section address |-----+     |                  |
             +-------------+       |             |       +-----------------+           |                  |
                                   +-------------+       |                 |           |                  |
                                                         +-----------------+           |                  |
                                                                                       +------------------+
```

ここでの違いは、PMDが物理セクションへのポインタを含むようになったことです。また、
オフセットには12ビットではなく21ビットを使用しています（2MBの範囲をエンコードする
には21ビットが必要だからです）。

### ページディスクリプタのフォーマット

MMUは、PMDがPTEを指しているのか、2MBの物理セクションを指しているのかをどうやって
知るのかという疑問があるかもしれません。この疑問に答えるためにはページテーブルエントリの
構造を詳しく見てみる必要があります。ここで、ページテーブルのエントリには常に次の
ページテーブルか物理ページのアドレスが含まれていると主張したのは正確ではなかった
ことを告白します。そのようなエントリは他にも情報を含んでいるからです。ページテーブルの
エントリは「ディスクリプタ」と呼ばれます。ディスクリプタは次のような特別なフォーマットを持っています。

```
                           Descriptor format
`+------------------------------------------------------------------------------------------+
 | Upper attributes | Address (bits 47:12) | Lower attributes | Block/table bit | Valid bit |
 +------------------------------------------------------------------------------------------+
 63                 47                     11                 2                 1           0
```

ここで理解すべき重要な点は、各ディスクリプタは常にページアラインされたもの（物理ページ、
セクション、または次の階層のページテーブル）を指しているということです。これは、
ディスクリプタに格納されるアドレスの最後の12ビットが常に0であることを意味します。事実
そうなっています。ここで、ディスクリプタのすべてのビットの意味を説明します。

* **ビット 0** このビットは有効なディスクリプタではすべて1に設定されなければなりません。
MMUは変換過程中に無効なディスクリプタに遭遇すると同期例外を生成します。カーネルはこの例外を
処理して、新しいページを割り当て、正しいディスクリプタを準備します（この仕組みについては
後ほど詳しく説明します）。
* **ビット 1** このビットは，現在のディスクリプタが階層内の次のページテーブルを指して
いるか（このようなディスクリプタを「テーブルディスクリプタ」と呼ぶ），物理的なページかセクションを
指しているか（このようなディスクリプタを「ブロックディスクリプタ」と呼ぶ）を示します。
* **ビット [11:2]** これらのビットはテーブルディスクリプタでは無視されます。ブロック
ディスクリプタでは、たとえば、マッピングされたページがキャッシュ可能か，実行可能か、など
を制御するいくつかの属性が含まれます。
* **ビット [47:12]**. ディスクリプタが指し示すアドレスが格納される場所です。先に述べた
ように、他のビットは常に0であるため、格納する必要があるのはアドレスの[47:12]ビットだけです。
* **ビット [63:48]** もう一つの属性セットです。

### ページ属性を構成する

前節で述べたように、ブロックディスクリプタには様々な仮想ページパラメータを制御する一連の
属性が含まれています。しかし、今回の議論にとって最も重要な属性はディスクリプタには直接
構成されていません。代わりに、ARMプロセッサはあるトリックを実装しており、ディスクリプタの
属性セクションのスペースを節約できるようにしています。

ARM.v8アーキテクチャには`mair_el1`レジスタが導入されています。このレジスタは
8つのセクションで構成されており、それぞれ8ビットの長さです。各セクションは
共通属性セットを構成します。ディスクリプタはすべての属性を直接指定するのではなく、
`mair`セクションのインデックスだけを指定します。これによりディスクリプタの3ビットを
使うだけで`mair`セクションを参照することができます。`mair`セクションの各ビットの
意味は`AArch64-Reference-Manual`の2609ページに記載されています。RPi OSでは
利用可能な属性オプションのほんの少ししか使用していません。[`mmu.h#L11`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/arm/mmu.h#L11)が`mair`レジスタの値を準備しているコードです。

```
/*
 * Memory region attributes:
 *
 *   n = AttrIndx[2:0]
 *            n    MAIR
 *   DEVICE_nGnRnE    000    00000000
 *   NORMAL_NC        001    01000100
 */
#define MT_DEVICE_nGnRnE         0x0
#define MT_NORMAL_NC             0x1
#define MT_DEVICE_nGnRnE_FLAGS   0x00
#define MT_NORMAL_NC_FLAGS       0x44
#define MAIR_VALUE            (MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE)) | (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NC))
```

ここでは`mair`レジスターの8つの利用可能なスロットのうち、2つだけを使用しています。
最初のスロットはデバイスメモリに，2番目のスロットは通常のキャッシュ不能なメモリに
対応します。`MT_DEVICE_nGnRnE`と`MT_NORMAL_NCP`がブロックディスクリプタで使用する
インデックスであり、`MT_DEVICE_nGnRnE_FLAGS`と`MT_NORMAL_NC_FLAGS`が`mair_el1`
レジスタの最初の2スロットに格納する値です。

### カーネル仮想メモリとユーザ仮想メモリ

MMUを有効にした後は、メモリアクセスには物理メモリではなく仮想メモリを使用しなければ
なりません。このため、カーネル自身も仮想メモリを使用する準備をして、独自のページ
テーブルを保持しなければならなくなります。この解決策として、ユーザモードから
カーネルモードに切り替わるたびに`pgd`レジスタをリロードすることが考えられます。
問題は`pgd`の切り替えは非常に高価な操作であるということです。なぜなら、すべての
キャッシュを無効にする必要があるからです。ユーザモードからカーネルモードへの
切り替えをどれほど頻繁に行わないといけないかを考えると、この解決策ではキャッシュが
全く役に立たなくなるため、OS開発ではこの解決策は決して使用されません。代わりにOSが
行っているのは、アドレス空間をユーザ空間とカーネル空間の2つに分けることです。
32ビットアーキテクチャでは、通常、アドレス空間の最初の3GBをユーザプログラムの
ための空間に割り当て、最後の1GBをカーネル用に確保します。64ビットアーキテクチャは
アドレス空間が巨大なのでこの点でははるかに有利です。さらに、それだけではありません。
ARM.v8アーキテクチャには、ユーザアドレスとカーネルアドレスの分割を簡単に実現する
ネイティブな機能が備わっています。

PGDのアドレスを保持できるレジスタが2つ存在するのです。`ttbr0_el1`と`ttbr1_el1`です。
覚えていると思いますが、我々はアドレスに利用可能な64 ビットのうち48 ビットしか
使っていません。ですので、上位16ビットを使って`ttbr0`と`ttbr1`の変換プロセスを区別
することができます。上位16ビットがすべて0であれば、`ttbr0_el1`に格納されている
PGDアドレスが使われ、アドレスが`0xffff`で始まる（最初の16ビットがすべて1の場合)
場合は、`ttbr1_el1`に格納されている PGD アドレスが選択されるようになっています。
また、EL0で実行されているプロセスは、同期例外を発生させること以外には、`0xffff`で
始まる仮想アドレスに絶対にアクセスできないようになっています。この説明から、
カーネルPGDへのポインタは`ttbr1_el1`に格納され、カーネルが存在する限りそこに
保持されること、`ttbr0_el1`はカレントユーザプロセスのPGDを格納するために使用される
ことが容易に推測できるでしょう。

このアプローチが意味することの一つは、カーネルの絶対アドレスはすべて`0xffff`で
始まらなければならないことです。RPi OSのソースコードにはこれを処理する箇所が
2箇所あります。[リンカスクリプト](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/linker.ld#L3)では
イメージのベースアドレスを`0xffff000000000000`と指定しています。これにより
コンパイラはイメージがアドレス`0xffff000000000000`にロードされると考え、絶対番地を
生成する必要がある際には常に正しいアドレスを生成します(リンカスクリプトにはさらに
いくつかの変更点がありますが、それについては後述します)。

カーネルの絶対ベースアドレスがハードコードされている場所がもう一つあります。
デバイスのベースアドレスを定義している[ヘッダ](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/peripherals/base.h#L7)です。
これにより`0xffff00003F000000`から始まるすべてのデバイスメモリにアクセスできるように
なります。実際には、これを実現するためには、まずカーネルがアクセスする必要のある
すべてのメモリをマッピングする必要があります。次節では、このマッピングを作成する
コードについて詳しく見ていきます。

### カーネルページテーブルを初期化する

カーネルページテーブルを作成するプロセスは、ブートプロセスの非常に早い段階で処理する必要があります。このプロセスは[boot.S](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/boot.S#L42)
ファイルで始まります。EL1に切り替えてBSSをクリアした直後に[__create_page_tables](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/boot.S#L92)
関数が呼び出されます。一行ずつ見ていきましょう。

```
__create_page_tables:
    mov    x29, x30                        // save return address
```

まず、この関数は`x30`(リンクレジスタ)を保存します。`create_page_tables`
から他の関数を読んでいく過程で`x30`は上書きされるからです。通常、`x30`は
スタックに保存されますが、`__create_page_tables`の実行中に再帰は
使用されず、`x29`が使用されないことも分かっているので、リンクレジスタを
保存するこの単純な方法でも問題ありません。

```
    adrp   x0, pg_dir
    mov    x1, #PG_DIR_SIZE
    bl     memzero
```

次に、初期ページテーブル領域をクリアします。ここで覚えておくべき重要な
ことは、この領域がどこにあり、そのサイズをどうやって知るかということです。
初期ページテーブル領域は[リンカスクリプト](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/linker.ld#L20)
で定義されています。これはカーネルイメージの中にこの領域の場所を確保して
いることを意味します。この領域のサイズの計算は少し大変です。まず、初期の
カーネルページテーブルの構造を理解する必要があります。私たちのマッピングは
すべて1GBの領域内にあることがわかっています（これはRPiのメモリの大きさ
です）。1つのPGDディスクリプタで`2^39 = 512GB`、1つのPUDディスクリプタで`2^30 = 1GB`の
連続した仮想マッピング領域をカバーできます。(これらの値は、仮想アドレス
におけるPGDとPUDのインデックスの位置に基づいて計算されます)。これは1つの
PGDと1つのPUDだけでRPiのすべてのメモリをマッピングすることができ、さらに、
PGDもPUDも1つのディスクリプタだけが含まれることを意味します。PUDのエントリが1つで
あれば、PMDテーブルも1つでなければならず、PUDエントリはこのテーブルを指す
ことになります。（1つのPMDエントリは2MBをカバーし、PMDには512個のエントリが
あるので、全体では、1つのPUDディスクリプタがカバーする範囲と同じ1GBのメモリを
PMDテーブルがカバーすることになります）。次に、1GBのメモリ領域をマッピング
する必要があることがわかっており、これは2MBの倍数であるので、セクション
マッピングを使用することができます。つまり、PTEは不要だということです。
結局、必要なのは3ページだけです。PGD、PUD、PMD用に各1ページずつです。
これが初期ページテーブル領域の正確なサイズです。

次に、`__create_page_tables`関数の外に出て、2つの重要なマクロである[create_table_entry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/boot.S#L68)と[create_block_map](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/boot.S#L77)を
見てみましょう。

`create_table_entry`は、新しいページテーブル（ここではPGDまたはPUD）を
割り当てる役割を果たします。ソースコードを以下に示します。

```
    .macro    create_table_entry, tbl, virt, shift, tmp1, tmp2
    lsr    \tmp1, \virt, #\shift
    and    \tmp1, \tmp1, #PTRS_PER_TABLE - 1            // table index
    add    \tmp2, \tbl, #PAGE_SIZE
    orr    \tmp2, \tmp2, #MM_TYPE_PAGE_TABLE
    str    \tmp2, [\tbl, \tmp1, lsl #3]
    add    \tbl, \tbl, #PAGE_SIZE                    // next level table page
    .endm
```

このマクロは、次の引数を受け付けます。

* `tbl` - 新しいテーブルを割り当てるメモリ領域へのポインタ
* `virt` - 現在マッピングしている仮想アドレス
* `shift` - 現在のテーブルインデックスを抽出するために仮想アドレスに
適用するシフト（PGDの場合は39、PUDの場合は30）
* `tmp1`, `tmp2` - 一時的なレジスタ

このマクロは非常に重要なので、これから時間をかけて理解していきます。

```
    lsr    \tmp1, \virt, #\shift
    and    \tmp1, \tmp1, #PTRS_PER_TABLE - 1   // tmp1: table index: #PTRS_PER_TABLE-1 = 0x1ff
```

マクロの最初の2行では仮想アドレスからテーブルインデックスを抽出しています。
最初に右シフトを適用してインデックスの右にあるものをすべて取り除いだ後に
`and`演算を使用して左にあるものをすべて取り除いています。

```
    add    \tmp2, \tbl, #PAGE_SIZE      // tmp2: addres of next tbl page
```

そして、次のページテーブルのアドレスが計算されます。ここでは、すべての
初期ページテーブルは1つの連続したメモリ領域に配置されるという慣習を
利用しています。つまり、階層の次のページテーブルは現在のページテーブルに
隣接していると仮定しています。

```
    orr    \tmp2, \tmp2, #MM_TYPE_PAGE_TABLE // (tmp2 | 0b11)
```

次に、階層の次のページテーブルへのポインタをテーブルディスクリプタに変換します
（ディスクリプタの下位2ビットは1にする必要があります）。

```
    str    \tmp2, [\tbl, \tmp1, lsl #3]    // tbl[index * 8]
```

そして、ディスクリプタを現在のページテーブルに格納します。以前に計算した
インデックスを使って、テーブル中の正しい場所を探します。

```
    add    \tbl, \tbl, #PAGE_SIZE       // 次レベルのテーブルページ
```

最後に、`tbl`パラメータを次階層のページテーブルを指すように変更します。
これは利便性のためであり、`tbl`パラメータを調整することなく次階層の
テーブルに対して`create_table_entry`を再度呼び出すことができます。
これはまさに[create_pgd_entry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/boot.S#L63)
マクロで行っていることであり、このマクロはPGDとPUDの双方を割り当てる
ラッパーに過ぎません。

次に重要なマクロは`create_block_map`です。ご想像のとおり、このマクロは
PMDテーブルのエントリを生成する役割を担っています。ソースは以下のように
なっています。

```
    .macro    create_block_map, tbl, phys, start, end, flags, tmp1
    lsr    \start, \start, #SECTION_SHIFT
    and    \start, \start, #PTRS_PER_TABLE - 1            // table index
    lsr    \end, \end, #SECTION_SHIFT
    and    \end, \end, #PTRS_PER_TABLE - 1                // table end index
    lsr    \phys, \phys, #SECTION_SHIFT
    mov    \tmp1, #\flags
    orr    \phys, \tmp1, \phys, lsl #SECTION_SHIFT        // table entry
9999:    str    \phys, [\tbl, \start, lsl #3]             // store the entry
    add    \start, \start, #1                             // next entry
    add    \phys, \phys, #SECTION_SIZE                    // next block
    cmp    \start, \end
    b.ls    9999b
    .endm
```

ここでのパラメータは少し異なります。

* `tbl` - PMDテーブルへのポインタ
* `phys` - マッピングする物理領域の開始点
* `start` - マッピングする最初のセクションの仮想アドレス
* `end` - マッピングする最後のセクションの仮想アドレス
* `flags` - ブロックディスクリプタの下位属性にコピーするフラグ
* `tmp1` - 一時的なレジスタ

では，ソースを見てみましょう。

```
    lsr    \start, \start, #SECTION_SHIFT
    and    \start, \start, #PTRS_PER_TABLE - 1            // table index
```

この2行は*start*仮想アドレスからテーブルインデックスを抽出しています。
これは、既に見た`create_table_entry`マクロで行った方法と全く同じ方法で
行っています。

```
    lsr    \end, \end, #SECTION_SHIFT
    and    \end, \end, #PTRS_PER_TABLE - 1                // table end index
```

同じことを`end`アドレスにも行います。これで`start`と`end`は仮想アドレス
ではなく、元のアドレスに対応するPMDテーブルのインデックスになりました。

```
    lsr    \phys, \phys, #SECTION_SHIFT
    mov    \tmp1, #\flags
    orr    \phys, \tmp1, \phys, lsl #SECTION_SHIFT        // table entry
```

次に，ブロックディスクリプタを作成し，変数`tmp1`に格納します。ディスクリプタを
作成するために、`phys`パラメータをまず右にシフトしてからシフトバック
し、`orr`命令で`flags`パラメータをマージします。なぜアドレスを前後に
ずらすのかというと、`phys`アドレスの最初の21ビットをクリアすることで、
マクロを汎用化して、セクションの最初のアドレスだけでなく、どのアドレス
でも使用できるようにするためです。

```
9999:    str    \phys, [\tbl, \start, lsl #3]             // store the entry
    add    \start, \start, #1                             // next entry
    add    \phys, \phys, #SECTION_SIZE                    // next block
    cmp    \start, \end
    b.ls    9999b
```

このマクロの最後の部分はループ内で実行されます。ここではまず
現在のディスクリプタをPMDテーブルの正しいインデックスに格納します。次に，
現在のインデックスを1つ増やし，次のセクションを指すようにディスクリプタを
更新します。現在のインデックスが最後のインデックスと等しくなるまで、
同じ処理を繰り返します。

`create_table_entry`マクロと`create_block_map`マクロがどのように
動作するかを理解したら、`__create_page_tables`関数の残りの部分は
簡単に理解できでしょう。

```
    adrp   x0, pg_dir
    mov    x1, #VA_START
    create_pgd_entry x0, x1, x2, x3
```

ここでPGDとPUDを作成します。このテーブルのマッピングを[VA_START](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/mm.h#L6)
仮想アドレスから開始するように構成しています。`create_table_entry`マクロの
作りにより、`create_pgd_entry`が完了した際に`x0`には次階層のテーブル、
すなわちPMDのアドレスが格納されています。

```
    /* Mapping kernel and init stack*/
    mov    x1, xzr                              // start mapping from physical offset 0
    mov    x2, #VA_START                        // first virtual address
    ldr    x3, =(VA_START + DEVICE_BASE - SECTION_SIZE)        // last virtual address
    create_block_map x0, x1, x2, x3, MMU_FLAGS, x4
```

次に、デバイスレジスタ領域を除く、全メモリ領域の仮想マッピングを作成
します。`flags`パラメータとして[MMU_FLAGS](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/arm/mmu.h#L24)
定数を使用します。これはマッピングされるすべてのセクションを通常の
キャッシュなしメモリとしてマークします（`MMU_FLAGS`定数の一部として、`MM_ACCESS`フラグも指定される
ことに注意してください。このフラグを指定しないとメモリアクセスのたびに同期例外が発生します）。

```
    /* Mapping device memory*/
    mov    x1, #DEVICE_BASE                                   // start mapping from device base address
    ldr    x2, =(VA_START + DEVICE_BASE)                      // first virtual address
    ldr    x3, =(VA_START + PHYS_MEMORY_SIZE - SECTION_SIZE)  // last virtual address
    create_block_map x0, x1, x2, x3, MMU_DEVICE_FLAGS, x4
```

次にデバイスレジスタ領域をマッピングします。これは前述のコードサンプルと
全く同じ方法で行われますが、開始アドレスと終了アドレス、フラグが
異なります。

```
    mov    x30, x29                        // restore return address
    ret
```

最後に、リンクレジスタを復元し、呼び出し元に戻ります。

### ページ変換を構成する

ページテーブルを作成したので、`el1_entry`関数に戻ります。ただし、
MMUのスイッチを入れるにはまだやるべきことがあります。ここでは
その作業を紹介します。

```
    mov    x0, #VA_START
    add    sp, x0, #LOW_MEMORY
```

initタスクのスタックポインタを更新します。これにより物理アドレスでは
なく、仮想アドレスを使用するようになります(そのため、initタスクは
MMUがオンになった後でしか使用できなくなります)。

```
    adrp   x0, pg_dir
    msr    ttbr1_el1, x0
```

`ttbr1_el1`を先に作成したPGDテーブルを指すように更新します。

```
    ldr    x0, =(TCR_VALUE)
    msr    tcr_el1, x0
```

変換制御レジスタである`tcr_el1`はMMUの一般的なパラメータを設定する役割を
担っています（たとえば、ここでは、カーネルとユーザーのページテーブルが
4KBページを使用するように設定しています）。

```
    ldr    x0, =(MAIR_VALUE)
    msr    mair_el1, x0
```

`mair`レジスタについては「ページ属性を構成する」の節ですでに説明しました。
ここでは、その値を設定しているだけです。

```
    ldr    x2, =kernel_main

    mov    x0, #SCTLR_MMU_ENABLED
    msr    sctlr_el1, x0

    br     x2
```

`msr sctlr_el1, x0`が実際にMMUが有効になる行です。ようやく`kernel_main`
関数にジャンプすることができます。しかし、なぜ単に`br kernel_main`命令を
実行できないのかという興味深い疑問が残ります。実際できません。MMUが有効に
なる前には、物理メモリで作業しており、カーネルは物理オフセット0にロード
されています。これは現在のプログラムカウンタが0に非常に近い値であることを
意味します。MMUをオンにしてもプログラムカウンタは更新されません。今、
`br kernel_main`命令を実行すると、この命令は現在のプログラムカウンタ
相対のオフセットを使用し、MMUをオンにしなかったら`kernel_main`があった
であろう場所にジャンプします。一方、`ldr    x2, =kernel_main`は
`kernel_main`関数の絶対アドレスである`x2`をロードします。リンカ
スクリプトでイメージのベースアドレスに`0xffff000000000000`を設定した
ので、`kernel_main`関数の絶対アドレスはイメージの先頭からのオフセットに
`0xffff000000000000`を加えたものとして計算されます。これはまさに私たち
が必要とするものです。もうひとつ理解する必要のある重要なことは、
`ldr x2, =kernel_main`命令はなぜMMUをオンにする前に実行しなければならない
かです。その理由は`ldr`も`pc`相対オフセットを使用するからです。MMUを
オンにしたがイメージベースアドレスにまだジャンプしていない時にこの命令を
実行しようとすると、ページフォルトが発生してしまいます。

### ユーザプロセスを割り当てる

実際のOSを使っている場合、おそらくプログラムをファイルシステムから
読み込んで実行できることを期待するでしょう。これはRPi OSでは違います。
RPi OSはまだファイルシステムをサポートしていないからです。これまでの
レッスンでは、ユーザプロセスはカーネルと同じアドレス空間を共有していた
ので、この事実に悩まされることはありませんでした。しかし、今や状況は
変わり、各プロセスは独自のアドレス空間を持つ必要があります。そこで、
ユーザプログラムを保存し、後で新しく作成したプロセスにロードできるように
する方法を考えなければなりません。最終的に実装したトリックは、ユーザ
プログラムをカーネルイメージの別のセクションに格納するというもんです。
これを行うためのリンカスクリプトの関連セクションは次のとおりです。

```
    . = ALIGN(0x00001000);
    user_begin = .;
    .text.user : { build/user* (.text) }
    .rodata.user : { build/user* (.rodata) }
    .data.user : { build/user* (.data) }
    .bss.user : { build/user* (.bss) }
    user_end = .;
```

私は、ユーザレベルのソースコードは`user`という接頭辞を持つファイルに
定義しなければならないという規約を作りました。これにより、リンカス
クリプトはユーザ関連のコードを連続した領域に分離し、その領域の始まりと
終わりを示す`user_begin`と`user_end`変数を定義することができます。
このような方法で`user_begin`と`user_end`の間にあるすべてのものを
新しく割り当てたプロセスアドレス空間にコピーするだけで、ユーザ
プログラムのロードをシミュレートすることができます。この方法は
シンプルで、現在の目的にはうまく動きますが、ファイルシステムの
サポートを実装し、ELFファイルをロードできるようになったら、このハックは
取り除く予定です。

今のところ、ユーザ領域には2つのファイルがコンパイルされます。

* [user_sys.S](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/user_sys.S) このファイルにはシステムコールの
ラッパー関数の定義が含まれています。RPi OSは前回のレッスンと同じ
システムコールをサポートしていますが、`clone`システムコールの代わりに
`fork`システムコールを使用することが違います。その違いは`fork`は
プロセスの仮想メモリをコピーすることであり，それがやってみたいことで
です。
* [user.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/user.c) ユーザプログラムのソースコードです。前回の
レッスンで使ったものとほとんど同じです。

### 最初のユーザプロセスを作成する

先のレッスンでもそうでしたが、[move_to_user_mode](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/fork.c#L44)
関数は最初のユーザプロセスを作成する役割を担っています。この関数は
カーネルスレッドから呼び出します。以下はその方法です。

```
void kernel_process(){
    printf("Kernel process started. EL %d\r\n", get_el());
    unsigned long begin = (unsigned long)&user_begin;
    unsigned long end = (unsigned long)&user_end;
    unsigned long process = (unsigned long)&user_process;
    int err = move_to_user_mode(begin, end - begin, process - begin);
    if (err < 0){
        printf("Error while moving process to user mode\n\r");
    }
}
```

`move_to_user_mode`の呼び出しには3つの引数が必要です。ユーザコード領域の
先頭へのポインタ、領域のサイズ、領域中のスタートアップ関数のオフセット
です。この情報は、先に説明した`user_begin`と`user_end`変数に基づいて
計算されます。

`move_to_user_mode`関数のコードを以下に示します。

```
int move_to_user_mode(unsigned long start, unsigned long size, unsigned long pc)
{
    struct pt_regs *regs = task_pt_regs(current);
    regs->pstate = PSR_MODE_EL0t;
    regs->pc = pc;
    regs->sp = 2 *  PAGE_SIZE;
    unsigned long code_page = allocate_user_page(current, 0);
    if (code_page == 0)    {
        return -1;
    }
    memcpy(code_page, start, size);
    set_pgd(current->mm.pgd);
    return 0;
}
```

では、ここで何が行われているか詳しく見ていきましょう。

```
    struct pt_regs *regs = task_pt_regs(current);
```

前回のレッスンと同様に`pt_regs`領域へのポインタを取得し、`pstate`を
設定して`kernel_exit`後にEL0になるようにします。

```
    regs->pc = pc;
```

`pc`はユーザ領域におけるスタートアップ関数のオフセットを指すようになります。

```
    regs->sp = 2 *  PAGE_SIZE;
```

ユーザプログラムのサイズは1ページを超えないというシンプルな規約を
設けました。そして、スタック用にもう1ページ割り当てます。

```
    unsigned long code_page = allocate_user_page(current, 0);
    if (code_page == 0)    {
        return -1;
    }
```

`allocate_user_page`は1メモリページを確保して、第2引数として提供される
仮想アドレスにマッピングします。マッピングの過程でカレントプロセスに
関連するページテーブルが作成されます。この関数がどのように動作するかは
この章で後ほど詳しく調べます。

```
    memcpy(code_page, start, size);
```

次に、ユーザ領域のすべてを（マップしたばかりのページの）新規アドレス
空間にコピーします。オフセット0から開始するのでユーザ領域のオフセットは
開始時点における実際の仮想アドレスになります。

```
    set_pgd(current->mm.pgd);
```

最後に、[set_pgd](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/utils.S#L24)
を呼び出します。これは`ttbr0_el1`レジスタを更新し、カレントプロセスの
変換テーブルをアクティブにします。

### TLB (Translation lookaside buffer)

`set_pgd`関数を見ると`ttbr0_el1`をセットした後、[TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer)
 (Translation lookaside buffer)をクリアしていることがわかります。
TLBは、物理ページと仮想ページのマッピングを保存するために特別に設計
されたキャッシュです。ある仮想アドレスが初めて物理アドレスにマッピング
された際にこのマッピングはTLBに格納されます。これにより、次回、同じ
ページにアクセスする際にはページテーブルウォークを行う必要がなくなり
ます。そのため、ページテーブルを更新した後にTLBを無効にするのはまったく
理にかなっています。そうしないと、TLBにすでに格納されているページには
変更が適用されません。

通常、私たちは単純化のためにキャッシュの使用はすべて避けていますが、
TLBなしではあらゆるメモリアクセスが極めて非効率的になりますし、TLBを
完全に無効にできるとは思えません。それに、`ttbr0_el1`の切り替え後に
クリアしなければならないこと以外は、TLBはOSに複雑さを加えることは
ありません。

### 仮想ページをマッピングする

[allocate_user_page](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L14)
関数がどのように使用されるかについてはすでに見てきました。次は、その
内部がどの様になっているかを見ていく番です。

```
unsigned long allocate_user_page(struct task_struct *task, unsigned long va) {
    unsigned long page = get_free_page();
    if (page == 0) {
        return 0;
    }
    map_page(task, va, page);
    return page + VA_START;
}
```

この関数は新しいページを割り当て、指定された仮想アドレスにマッピングし。
そのページへのポインタを返します。これからは「ポインタ」と言う場合には
3つのポインタを区別する必要があります。物理ページへのポインタと
カーネルアドレス空間のポインタ、ユーザアドレス空間のポインタです。
これら3つの異なるポインタはすべてメモリ内の同じ場所に導くことができます。
この例では`page`変数は物理ポインタであり、返り値はカーネルアドレス
空間のポインタです。このポインタは観点に計算することができます。
物理メモリ全体を`VA_START`仮想アドレスを起点にしてリニアにマッピング
しているからです。また、新しいカーネルページテーブルの割り当てに関しても
心配する必要はありません。`boot.S`ですべてのメモリがマッピングされて
いるらです。ユーザマッピングは依然として作成する必要があり、これは
次に説明する[map_page](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L62)関数で行われます。

```
void map_page(struct task_struct *task, unsigned long va, unsigned long page){
    unsigned long pgd;
    if (!task->mm.pgd) {
        task->mm.pgd = get_free_page();
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = task->mm.pgd;
    }
    pgd = task->mm.pgd;
    int new_table;
    unsigned long pud = map_table((unsigned long *)(pgd + VA_START), PGD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pud;
    }
    unsigned long pmd = map_table((unsigned long *)(pud + VA_START) , PUD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pmd;
    }
    unsigned long pte = map_table((unsigned long *)(pmd + VA_START), PMD_SHIFT, va, &new_table);
    if (new_table) {
        task->mm.kernel_pages[++task->mm.kernel_pages_count] = pte;
    }
    map_table_entry((unsigned long *)(pte + VA_START), va, page);
    struct user_page p = {page, va};
    task->mm.user_pages[task->mm.user_pages_count++] = p;
}
```

`map_page`は、`__create_page_tables`関数で行っていることとある意味で
重複しています。この関数はページテーブル階層を割り当て、生成します。
ただし、重要な違いが3点あります。コードはアセンブラではなくCで
書かれている点、`map_page`はメモリ全体ではなく単一のページを
マッピングする点、セクションマッピングではなく通常のページマッピングを
使用する点です。

このプロセスには[map_table](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L47)と
[map_table_entry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L40)という2つの重要な関数が関係します。

`map_table`を以下に示します。

```
unsigned long map_table(unsigned long *table, unsigned long shift, unsigned long va, int* new_table) {
    unsigned long index = va >> shift;
    index = index & (PTRS_PER_TABLE - 1);
    if (!table[index]){
        *new_table = 1;
        unsigned long next_level_table = get_free_page();
        unsigned long entry = next_level_table | MM_TYPE_PAGE_TABLE;
        table[index] = entry;
        return next_level_table;
    } else {
        *new_table = 0;
    }
    return table[index] & PAGE_MASK;
}
```

この関数は次の引数を取ります。

* `table` これは親ページテーブルへのポインタです。このページテーブルは
割り当て済みであると仮定されますが、空の場合もあります。
* `shift` この引数は指定された仮想アドレスからテーブルインデックスを
抽出するために使用されます。
* `va` 仮想アドレスそのものです。
* `new_table` これは出力パラメタです。新規子テーブルが割り当てられた
場合は1がセットされます。それ以外は0のままです。

この関数は`create_table_entry`マクロに相当するものだと考えることが
できます。この関数は仮想アドレスからテーブルインデックスを抽出し、
子テーブルを指す親テーブル内のディスクリプタを準備します。`create_table_entry`
マクロとは異なり、子テーブルがメモリ上親テーブルに隣接しているとは
想定しません。その代わり、利用可能なページを返す`get_free_table`関数に
依存します。また、子テーブルが割り当て済みの場合もあります（子ページ
テーブルが以前別のページに割り当てられていた領域をカバーする場合などに
起こります）。この場合は`new_table`を0に設定し、親テーブルから子ページ
テーブルのアドレスを読み込みます。

`map_page`は`map_table`を3回呼び出します。PGD、PUD、PMDに対して1回ずつ
です。最後の呼び出しはPTEを割り当て、PMDにディスクリプタを設定します。次に、
`map_table_entry`が呼び出されます。この関数を以下に示します。

```
void map_table_entry(unsigned long *pte, unsigned long va, unsigned long pa) {
    unsigned long index = va >> PAGE_SHIFT;
    index = index & (PTRS_PER_TABLE - 1);
    unsigned long entry = pa | MMU_PTE_FLAGS;
    pte[index] = entry;
}
```

`map_table_entry`は仮想アドレスからPTEインデックスを抽出し、PTEディスクリプタ
の準備と設定を行います。これは`create_block_map`マクロで行っていることと
同様です。

ユーザページテーブルの割り当てについては以上ですが、`map_page`には
もう秘湯重要な役割があります。それは仮想アドレスマッピングの過程で
割り当てられたページを追跡することです。そのようなページはすべて
[kernel_pages](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/sched.h#L53)
配列に格納されます。この配列はタスクの終了後に割り当てられたページを
開放できるようにするために必要です。また、[user_pages](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/include/sched.h#L51)
配列もあり、これも`map_page`関数によってデータが追加されます。この配列
には、プロセスの仮想ページと物理ページとの対応関係に関する情報が格納
されます。この情報は`fork`時にプロセスの仮想メモリをコピーするために
必要です（詳細は後述します）。

### プロセスをフォークする

先に進む前に、これまでの流れをまとめておきましょう。私たちはどのように
最初のユーザプロセスが作成され、そのページテーブルにデータが追加され、
ソースコードが適切な場所にコピーされ、スタックが初期化されるかを見て
きました。これらの準備を経て、プロセスは実行可能な状態になります。
ユーザプロセス内で実行されるコードを以下に示します。

```
void loop(char* str)
{
    char buf[2] = {""};
    while (1){
        for (int i = 0; i < 5; i++){
            buf[0] = str[i];
            call_sys_write(buf);
            user_delay(1000000);
        }
    }
}

void user_process()
{
    call_sys_write("User process\n\r");
    int pid = call_sys_fork();
    if (pid < 0) {
        call_sys_write("Error during fork\n\r");
        call_sys_exit();
        return;
    }
    if (pid == 0){
        loop("abcde");
    } else {
        loop("12345");
    }
}
```

コード自体はとてもシンプルです。唯一厄介なのは`fork`システムコールの
セマンティクスです。`clone`とは異なり、`fork`を行う際には、新しい
プロセスで実行する必要のある関数を提供する必要はありません。また、
[forkラッパー関数](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/user_sys.S#L26)は
`clone`のものよりずっと簡単です。これらのことが可能なのは、`fork`は
プロセスの仮想アドレス空間の完全なコピーを作成するためです。そのため、
forkラッパー関数は2回返ります。1回は元のプロセスで、もう1回は新しいプロセスです。この時点で、同じスタックと`pc`を持つ2つの同じプロセスが存在する
ことになります。唯一の違いは`fork`システムコールの返り値です。親プロセス
では子のPIDを、子プロセスでは0を返します。この時点から両プロセスは完全に
独立に実行され、スタックを変更したり、メモリ上の同じアドレスに異なる
内容を書き込んだりすることができます。

それでは`fork`システムコールがどのように実装されているか見てみましょう。
[copy_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/fork.c#L7)関数がほとんどの仕事をしています。

```
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg)
{
    preempt_disable();
    struct task_struct *p;

    unsigned long page = allocate_kernel_page();
    p = (struct task_struct *) page;
    struct pt_regs *childregs = task_pt_regs(p);

    if (!p)
        return -1;

    if (clone_flags & PF_KTHREAD) {
        p->cpu_context.x19 = fn;
        p->cpu_context.x20 = arg;
    } else {
        struct pt_regs * cur_regs = task_pt_regs(current);
        *childregs = *cur_regs;
        childregs->regs[0] = 0;
        copy_virt_memory(p);        // この行に注目
    }
    p->flags = clone_flags;
    p->priority = current->priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; //disable preemtion until schedule_tail

    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)childregs;
    int pid = nr_tasks++;
    task[pid] = p;

    preempt_enable();
    return pid;
}
```

この関数は前回のレッスンとほとんど同じですが、1つだけ違いがあります。
ユーザプロセスをコピーする際には、新規プロセスのスタックポインタと
プログラムカウンタを変更する代わりに[copy_virt_memory](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L87)
を呼び出しています。`copy_virt_memory`のコードは次のようなものです。

```
int copy_virt_memory(struct task_struct *dst) {
    struct task_struct* src = current;
    for (int i = 0; i < src->mm.user_pages_count; i++) {
        unsigned long kernel_va = allocate_user_page(dst, src->mm.user_pages[i].virt_addr);
        if( kernel_va == 0) {
            return -1;
        }
        memcpy(kernel_va, src->mm.user_pages[i].virt_addr, PAGE_SIZE);
    }
    return 0;
}
```

これは、カレントプロセスによって割り当てられたすべてのページを含む
`user_pages`配列にわたり繰り返し処理します。`user_pages`配列には
そのプロセスが実際に利用可能で、そのソースコードかデータを含む
ページのみが格納されていることに注意してください。ここには`kernel_pages`
配列に格納されているページテーブルページは含まれていません。次に、
各ページについて別の空ページを割り当てて元のページの内容をコピー
します。また、元のページで使用されていた同じ仮想アドレスを使って
新しいページをマッピングします。このようにして、元のプロセスの
アドレス空間の正確なコピーを取得します。

フォーク手順のその他の詳細は前回のレッスンのものとまったく同じように
動作します。

### 新しいページをオンデマンドで割り当てる

`move_to_user_mode`関数に戻って見てみると、オフセット0から始まる
1ページしかマッピングしていないことに気づくでしょう。しかし、
2ページ目がスタックとして使用されることが想定されます。なぜ、2ページ目も
マッピングしないのでしょうか。バグだと思うでしょう。しかし、バグでは
ありません。機能なのです。スタックページだけでなく、プロセスがアクセス
する必要のある他のページも初めて要求された時に初めてマッピングされます。
ここからはこのメカニズムの内部構造を見ていきましょう。

プロセスがまだマッピングされていないページに属するアドレスにアクセス
しようとすると同期例外が発生します。これは私たちがサポートする2つ目の
タイプの同期例外です（1つ目のタイプはシステムコールである`svc``命令に
よって発生する例外です）。同期例外ハンドラは次のようになっています。

```
el0_sync:
    kernel_entry 0
    mrs    x25, esr_el1                // read the syndrome register
    lsr    x24, x25, #ESR_ELx_EC_SHIFT        // exception class
    cmp    x24, #ESR_ELx_EC_SVC64            // SVC in 64-bit state
    b.eq    el0_svc
    cmp    x24, #ESR_ELx_EC_DABT_LOW        // data abort in EL0
    b.eq    el0_da
    handle_invalid_entry 0, SYNC_ERROR
```

ここでは`esr_el1`レジスタを使用して例外の種類を判断しています。
ページフォルト例外（あるいは、同じですが、データアクセス例外）の場合は、
`el0_da`関数が呼び出されます。

```
el0_da:
    bl    enable_irq
    mrs    x0, far_el1
    mrs    x1, esr_el1
    bl    do_mem_abort
    cmp x0, 0
    b.eq 1f
    handle_invalid_entry 0, DATA_ABORT_ERROR
1:
    bl disable_irq
    kernel_exit 0
```

`el0_da`は主な作業を[do_mem_abort](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson06/src/mm.c#L101)
関数にリダイレクトします。この関数は2つの引数を取ります。

1. アクセスしようとしたメモリアドレス。このアドレスは`far_el1`レジスタ
（フォールトアドレスレジスタ）から取得します。
2. `esr_el1`(例外シンドロームレジスタ)の内容。

`do_mem_abort`のコードは次のとおりです。
```
int do_mem_abort(unsigned long addr, unsigned long esr) {
    unsigned long dfs = (esr & 0b111111);
    if ((dfs & 0b111100) == 0b100) {
        unsigned long page = get_free_page();
        if (page == 0) {
            return -1;
        }
        map_page(current, addr & PAGE_MASK, page);
        ind++;
        if (ind > 2){
            return -1;
        }
        return 0;
    }
    return -1;
}
```

この機能を理解するためには`esr_el1`レジスタの仕様について少し知って
おく必要があります。このレジスタの[32:26]ビットは「例外クラス」と
呼ばれています。`el0_sync`ハンドラはこれらのビットをチェックして、
それがシスコールなのか、データアボート例外なのか、あるいは潜在的な
何か他のものなのかを判断します。例外クラスは[24:0]ビットの意味を
決定します。これらのビットは通常、例外に関する追加情報を提供する
ために使用されます。データアボート例外の場合の[24:0]ビットの意味は
`AArch64-Reference-Manual`の2460ページに記載されています。一般に、
データアボート例外はさまざまなシナリオで発生する可能性があります
（パーミッションフォールト、アドレスサイズフォールト、その他多くの
可能性があります）。ここでは、現在の仮想アドレスに対応するページ
テーブルの一部が初期化されていない場合に発生する変換フォールトにのみ
関心があります。そこで、`do_mem_abort`関数の最初の2行で、現在の例外が
実際に変換フォールトであるかチェックしています。もしそうであれば、
新しいページを割り当てて、要求された仮想アドレスにマッピングします。
これらの処理はユーザプログラムからは完全に透過的に行われます。ユーザ
プログラムは、一部のメモリアクセスが中断され、その間に新しいページ
テーブルが割り当てられたことには気付きません。

### 結論

この章は長くて難しいものでしたが、お役に立てれば幸いです。仮想メモリは
オペレーティングシステムの最も基本的な要素の1つです。この章を通して、
願わくば、仮想メモリが最も低いレベルでどのように動作しているかを理解し
始めたことを嬉しく思います。仮想メモリの導入により、プロセスの完全な
隔離が可能になりましたが、RPiのOSはまだ完成にはほど遠い状態です。
ファイルシステム、ドライバ、シグナルと割込み待ちリスト、ネットワーク、
その他多くの有用な概念がまだサポートされていません。今後のレッスンで
それらを引き続き明らかにしていきます。

##### 前ページ

5.3 [ユーザプロセスとシステムコール: 演習](../../ja/lesson05/exercises.md)

##### 次ページ

6.2 仮想メモリ管理: Linux (準備中)
6.3 [仮想メモリ管理: 演習](../../ja/lesson06/exercises.md)にジャンプ
