## 4.4: スケジューラ

Linuxのスケジューラの内部動作についてすでに多くのことを学んできましたので、
残されているものはそれほど多くありません。本章では、全体像を把握するために、
次の2つの重要なスケジューラのエントリポイントを見ていきます。

1. [scheduler_tick()](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L3003) タイマ割り込みごとに呼び出される関数。
2. [schedule()](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L3418) カレントタスクを再スケジュールする必要があるたびごとに呼び出される関数。

この章で調査する3番めの主要なものはコンテキストスイッチという概念です。
コンテキストスイッチとは、カレントタスクを中断して、代わりに別のタスクを実行する
処理のことです。この処理はアーキテクチャに強く依存しており、RPi OSを動作させる
際に私たちが行っていることに密接に関連します。

### scheduler_tick

この関数は次の2つの理由で重要です。

1. スケジューラに時間統計とカレントタスクに関するランタイム情報を更新する
方法を提供する。
2. ランタイム情報をカレントタスクをプリエンプトするか否かの判断のために
使用し、もしするのであれば、`schedule()`を呼び出す。

これまで説明してきたほとんどの関数と同様に、`scheduler_tick`は複雑すぎて
完全には説明できませんが、いつものように最も重要な部分だけを紹介します。

1. 主な作業はCFSのメソッドである[task_tick_fair](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L9044)で
行われます。このメソッドはカレントタスクに対応する`sched_entity`の[entity_tick](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3990)を
呼び出します。ソースコードを見ると、なぜカレント`sched_entry`に対して`entry_tick`を
呼び出すのではなく、`for_each_sched_entity`マクロが使用されるのか不思議に思う
かもしれません。`for_each_sched_entity`はシステムのすべての`sched_entry`に
ついて処理するわけではありません。ルートまで`sched_entry`継承木を走査する
だけです。これはタスクがグループ化されている場合に便利であり、特定のタスクの
ランタイム情報を更新した後、グループ全体に対応する`sched_entry`も更新します。

2. [entity_tick](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3990) は主に次の2つのことを行います。
  * [update_curr](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L827)を
  呼び出します。これはタスクの`vruntime`の更新とランキューの`min_vruntime`を
  更新します。ここで覚えておくべき重要なことは、`bruntime`は常に2つのこと、
  すなわち、タスクが実際どのくらいな長く実行されているかとタスクの優先度に
  基づいていることです。
  * [check_preempt_tick](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3834)を
  呼び出します。これはカレントタスクをプリエンプトする必要があるか否かを
  チェックします。プリエンプトは次の2つの場合に起きます。
    1. カレントタスクの実行時時間が長すぎる場合（比較は`vruntime`ではなく、
   通常時間で行われます）。[リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3842)
    2. より小さな`vruntime`を持つタスクが存在し、`vruntime`値の差が閾値より
   大きい場合。 [リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3866)

    どちらの場合も[resched_curr](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L479)
    関数を呼び出してカレントタスクにプリエンプトするためのマークを付けます。

`resched_curr`の呼び出しにより、カレントタスクに`TIF_NEED_RESCHED`フラグが
設定され、最終的に`schedule`が呼び出されることは前章で既に見てきました。

`schedule_tick`については以上です。これでようやく`schedule`関数を見ていく
準備ができました。

### schedule

`schedule`が使われている例はすでにたくさん見てきましたので、この関数が実際に
どのように動作するのかを知りたいと思っているはずです。この関数の内部が思って
いたよりシンプルであることを知って驚くでしょう。

1. 主な作業は[__schedule](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L3278)関数で行われています。
1. `__schedule`は[pick_next_task](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L3199)を呼び出しますが、この関数は作業のほとんどをCFS
スケジューラの[pick_next_task_fair](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L6251)メソッドにリダイレクトします。
2. ご想像のとおり、通常のケースでは`pick_next_task_fair`は赤黒木の最も左の要素を
選択して返すだけです。これは[`fair.c#L3915`](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L3915)で行われています。
1. `__schedule`は次に[context_switch](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L2750)を呼び出します。この関数は準備作業をした後、
アーキテクチャ固有の[__switch_to](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/process.c#L348)
関数を呼び出します。そこではスイッチのための低レベルなアーキテクチャ固有のタスク
パラメタが準備されます。
2. `__switch_to`はまず、TLS (スレッドローカルストア)や浮動小数点/NEON保存レジスタ
のような追加のタスクコンポーネントを切り替えます。
3. 実際の切り替えはアセンブラの関数[cpu_switch_to](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L914)で行われています。この関数はすでに
おなじみのはずです。ほとんど変更を加えずにRPi OSにコピーしたものだからです。
覚えていると思いますが、この関数はcalleee-savedなレジスタとタスクスタックを
切り替えます。この関数が復帰すると、新規タスクが独自のカーネルスタックを使って
実行されます。

### 結論

これでLinuxのスケジューラについては終了です。非常に基本的なワークフローだけに
集中すれば、それほど難しく思えないのは良かったです。基本的なワークフローを
理解した後は、スケジュールコードの別のパスからさらに細部に注意を払いたくなる
かもしれません。それは非常に沢山あるからです。しかし、私たちは現時点では現在の
理解で満足しており、ユーザプロセスとシステムコールを説明する次のレッスンに
移る準備ができています。

##### 前ページ

4.3 [プロセススケジューラ: タスクをフォークする](../../../ja/lesson04/linux/fork.md)

##### 次ページ

4.5 [プロセススケジューラ: 演習](../../../ja/lesson04/exercises.md)
