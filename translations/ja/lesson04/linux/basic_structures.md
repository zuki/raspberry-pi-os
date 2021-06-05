## 4.2: スケジューラの基本構造体

これまでのレッスンでは、主にアーキテクチャ固有のコードやドライバのコードを
扱ってきました。今回は初めてLinuxカーネルのコア部分を深く掘り下げていきます。
この作業は簡単ではなく、いくつかの準備が必要です。Linuxスケジューラのソース
コードを理解する前に、スケジューラの基本となるいくつかの主要な概念に精通して
おく必要があります。

### [task_struct](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L519)

これは全カーネル中で最も重要な構造体の一つであり、実行中のタスクに関する
すべての情報を含んでいます。`task_struct`についてはレッスン2で簡単に触れましたし、
RPi OS用に独自の`task_struct`を実装していますので、この時点ですでにそれが
どのように使われるかについては基本的に理解していると思います。そこで、ここでは
今後の説明に関連するこの構造体の重要なフィールドに注目します。

* [thread_info](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L525) これは`task_struct`の最初のフィールドで、低レベルアーキテクチャコードが
アクセスするすべてのフィールドを含んでいます。それがどのように行われるかについては
レッスン2ですでに見てきましたが、今後他の例も見ることになります。 [thread_info](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/thread_info.h#L39)は
アーキテクチャ固有です。`arm64`の場合はいくつかのフィールドを持つ単純な構造体です。

  ```
  struct thread_info {
          unsigned long        flags;        /* low level flags */
          mm_segment_t        addr_limit;    /* address limit */
  #ifdef CONFIG_ARM64_SW_TTBR0_PAN
          u64            ttbr0;        /* saved TTBR0_EL1 */
  #endif
          int            preempt_count;    /* 0 => preemptable, <0 => bug */
  };
  ```

  `flags`フィールドは非常に頻繁に使用され、現在のタスクの状態（トレース中で
  あるか、シグナルが保留中であるかなど）に関する情報を含んでいます。フラグの
  すべての可能な値は[`thread_info.h#L79`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/thread_info.h#L79)にあります。
* [state](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L528)
タスクの現在の状態（現在、実行中か、割込み待ちか、終了したかなど）です。タスクの
すべての可能な状態は[sched.h#L69](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L69)で記載されています。
* [stack](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L536)
RPi OSでは、`task_struct`は常にタスクスタックの底に保持されることを知っている
ので、`task_struct`へのポインタをスタックへのポインタとして使用できます。
カーネルスタックは一定の大きさなので、スタックの端を見つけることも簡単です。
同じ方法が初期のLinuxカーネルでは使われていたと思いますが、[仮想マップドスタック](https://lwn.net/Articles/692208/)が
導入された現在では、`stack`フィールドはカーネルスタックへのポインタを格納する
ために使われています。
* [thread](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L1108)
もうひとつ重要なアーキテクチャ固有の構造体が[thread_struct](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/processor.h#L81)です。
この構造体にはコンテキストスイッチの際に使用される（[cpu_context](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/processor.h#L65)などの）
すべての情報が格納されます。実際、RPi OSではオリジナルの`cpu_context`とまったく
同じように使用される独自の`cpu_context`を実装しています。
* [sched_classとsched_entity](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L562-L563) これらのフィールドはスケジュールアルゴリズムで使用
されます。詳細は後述します。

### スケジューラクラス

Linuxでは、各タスクが独自のスケジューリングアルゴリズムの使用を可能にする拡張
可能な機構があります。この機構では[sched_class](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L1400)という構造体を使用します。この構造体は
スケジュールクラスが実装しなければならないすべてのメソッドを定義したインタ
フェースと考えることができます。`sched_class`インタフェイスにはどのような
メソッドが定義されているか見てみましょう（ただし、すべてのメソッドが示すのでは
なく、私たちにとって最も重要と思うものだけを示します）。

* [enqueue_task](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L1403) 新規タスクがスケジューラクラスに追加される度に実行されます。
* [dequeue_task](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L1404) タスクがスケジューラから察駆除される際に呼び出されます。
* [pick_next_task](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L1418) コアスケジューラのコードが次に実行するタスクを決定する必要がある場合に
このメソッドを呼び出します。
* [task_tick](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L1437) タイマティックごとに呼び出され、スケジューラクラスがカレントタスクの
実行時間を測定する機会を与えたり、プリエンプトが必要な場合にコアスケジューラ
コードに通知する機会を与えます。

`sched_class`の実装は複数あります。最も一般的に使われているクラスは「完全に
公平なスケジューラ (CFS)」と呼ばれるもので、通常、すべてのユーザータスクで
使用されています。

### 完全に公平なスケジューラ (CFS)

CFSアルゴリズムの原理は非常にシンプです。

1. CFSはシステムの各タスクに対してタスクに割り当てられたCPU時間を測定します
（その値は`sched_entity`構造体の [sum_exec_runtime](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L385）
フィールドに格納されます）。
2. `sum_exec_runtime`はタスクの優先度に応じて調整され、[vruntime](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L386)フィールドに
保存されます。
3. CFSは実行する新規タスクを選択する必要がある場合、`vruntime`が最も小さいタスクを
選択します。

Linuxスケジューラにはもう一つ「ランキュー」と呼ばれる重要なデータ構造を使用します。
ランキューは[rq](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L667)
構造体で記述されています。ランキューのインスタンスはCPUごとに1つだけです。
実行する新しいタスクを選択する場合、その選択はローカルのランキューからのみ
行われます。しかし、必要があれば、タスクは異なる`rq`構造体間でバランスをとる
ことができます。

ランキューは、CFSだけでなく、すべてのスケジューラクラスで使用されます。
CFS固有のすべての情報は[cfs_rq](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L420)構造体に保持されます。`cfs_rq`は`rq`構造体に組み込まれて
います。`cfs_rq`構造体の重要なフィールドの1つに[min_vruntime](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L425)があります。これはランキューに
割り当てられている全てのタスクの中で最も低い`vruntime`です。`min_vruntime`は新たに
フォークされるタスクに割り当てられますが、これによりそのタスクが次に選択される
ことが保証されます。なぜなら、CFSは常に`vruntime`の最も小さいタスクを選択する
からです。この方法は、新しいタスクがプリエンプトされるまでの時間が不当に長く
ならないことも保証しています。

特定のランキューに割り当てられ、CFSによって追跡されているすべてのタスクは
`cfs_rq`構造体の[tasks_timeline](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/sched.h#L430)
フィールドに保持されます。`tasks_timeline`は[赤黒木](https://en.wikipedia.org/wiki/Red%E2%80%93black_tree)で構成されており`vruntime`値に並んだタスクから選択できる
ようになっています。赤黒木には重要な属性があります。すべての操作（検索、挿入、削除）
が[O(log n)](https://en.wikipedia.org/wiki/Big_O_notation)時間で
実行できることです。これは、システムに何千もの同時タスクがあったとしても、
すべてのスケジューラメソッドが非常に高速に実行されることを意味します。赤黒木の
もう一つ重要な属性は、木のすべてのノードにおいて、その右の子が常に親よりも
大きな`vruntime`値を持ち、左の子の`vruntime`が常に親の`vruntime`以下である
ことです。これには重要な意味があります。最左端のノードは常に最小の`vruntime`を
持つことです。

### [struct pt_regs](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/ptrace.h#L119)

割り込みが生じた際にすべての汎用レジスタがどのようにスタックに保存されるかに
ついてはすでに示しました。そのプロセスがLinuxカーネルでどのように行われている
かを調べ、RPi OSに同様のものを実装しました。そこでまだ触れていなかったのは、
割り込み処理中にこれらのレジスタを操作することは完全に合法であるということです。
あらかじめレジスタを用意してスタックに置くことも同じく合法です。これはカーネル
スレッドをユーザモードに移行する際に行いますが、同じ機能を次のレッスンで実装
する予定です。今のところは、保存されるレジスタの記述に`pt_regs`構造体が使用され、
そのフィールドは`kernel_entry`マクロでレジスタが保存される際と同じ順番に並べ
られていなければならないことを覚えておく必要があるだけです。このレッスンでは、
この構造体が使用されるいくつかの例を見ていくつもりです。

### 結論

スケジューリングには関連する非常に重要な構造体、アルゴリズムと概念があります。
今回紹介したものは次の2章を理解するために必要な最小限のものです。では、これらが
実際にどのように機能するのかを見ていきましょう。

##### 前ページ

4.1 [プロセススケジューラ: RPi OSスケジューラ](../../../ja/lesson04/rpi-os.md)

##### 次ページ

4.3 [プロセススケジューラ: タスクをフォークする](../../../ja/lesson04/linux/fork.md)
