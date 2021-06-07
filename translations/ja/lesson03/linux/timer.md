## 3.4: タイマ

前章はグローバル割り込みコントローラの調査で終わりました。タイマ割り込みの経路に
ついては[bcm2836_chained_handle_irq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L246)
関数まで追跡できています。次の論理的ステップは、タイマドライバがこの割り込みを
どのように処理するかを見ることです。しかし、これを行う前に、タイマ機能に関する
いくつかの重要な概念を理解しておく必要があります。これらはすべて[公式カーネルドキュメント](https://github.com/torvalds/linux/blob/v4.14/Documentation/timers/timekeeping.txt)で
説明されていますので、このドキュメントを読むことを強く勧めます。しかし、忙しくて
読んでいる隙がないという方のために私なりに簡単な説明をしておきます。

1. **クロックソース** 現在の時刻を正確に知る必要がある時は常にクロックソース
フレームワークを使用します。通常、クロックソースは単調でアトミックなnビット
カウンタとして実装されており、0から2^(n-1)までカウントした後、0に戻り、最初から
カウントし直します。また、クロックソースはカウンタをナノ秒値に変換する方法も
提供しています。
2. **クロックイベント** この抽象化はタイマ割り込みを誰でも購読できるようにする
ために導入されました。クロックイベントフレームワークは、次のイベント用に指定
された時間を入力とし、それに基づいて、タイマハードウェアレジスタに設定する適切な
値を計算します。
3. **sched_clock()** この関数はシステム起動後のナノ秒数を返します。通常はタイマ
レジスタを直接読み込むことでこれを実現します。この関数は非常に頻繁に呼び出される
ためパフォーマンスを最適化する必要があります。

次の節では、クロックソース、クロックイベント、sched_clock関数を実装するために
システムタイマがどのように使用されているかを見ていきます。

### BCM2835システムタイマ

いつものように、特定のデバイスがデバイスツリーのどこにあるかを見つけることから
調査を始めます。システムタイマノードは[`bcm283x.dtsi#L57`](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L57)で
定義されています。この定義は何度か参照することになるのでしばらくの間、
開いておいてください。

次に、`compatible`プロパティを使用して対応するドライバの場所を見つける必要が
あります。ドライバは[`bcm2835_timer.c`](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c)にあることがわかります。最初に見るのは[bcm2835_timer](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L42)構造体です。

```
struct bcm2835_timer {
    void __iomem *control;
    void __iomem *compare;
    int match_mask;
    struct clock_event_device evt;
    struct irqaction act;
};
```

この構造体はドライバが機能するために必要なすべての状態を格納します。`control`
フィールドと`compare`フィールドには対応するメモリマップドレジスタのアドレスを格納
します。`match_mask`は利用可能な4つのタイマ割り込みのどれを使用するかを決定するために
使用されます。`evt`フィールドはクロックイベントフレームワークに渡される構造体を格納
します。`act`フィールドはカレントドライバを割り込みコントローラに接続するために
使用されるirqアクションです。

次に、ドライバの初期化関数である[bcm2835_timer_init](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L83)を
見てみましょう。この関数はサイズが大きいですが、最初に思うほど難しくはありません。

```
static int __init bcm2835_timer_init(struct device_node *node)
{
    void __iomem *base;
    u32 freq;
    int irq, ret;
    struct bcm2835_timer *timer;

    base = of_iomap(node, 0);
    if (!base) {
        pr_err("Can't remap registers\n");
        return -ENXIO;
    }

    ret = of_property_read_u32(node, "clock-frequency", &freq);
    if (ret) {
        pr_err("Can't read clock-frequency\n");
        goto err_iounmap;
    }

    system_clock = base + REG_COUNTER_LO;
    sched_clock_register(bcm2835_sched_read, 32, freq);

    clocksource_mmio_init(base + REG_COUNTER_LO, node->name,
        freq, 300, 32, clocksource_mmio_readl_up);

    irq = irq_of_parse_and_map(node, DEFAULT_TIMER);
    if (irq <= 0) {
        pr_err("Can't parse IRQ\n");
        ret = -EINVAL;
        goto err_iounmap;
    }

    timer = kzalloc(sizeof(*timer), GFP_KERNEL);
    if (!timer) {
        ret = -ENOMEM;
        goto err_iounmap;
    }

    timer->control = base + REG_CONTROL;
    timer->compare = base + REG_COMPARE(DEFAULT_TIMER);
    timer->match_mask = BIT(DEFAULT_TIMER);
    timer->evt.name = node->name;
    timer->evt.rating = 300;
    timer->evt.features = CLOCK_EVT_FEAT_ONESHOT;
    timer->evt.set_next_event = bcm2835_time_set_next_event;
    timer->evt.cpumask = cpumask_of(0);
    timer->act.name = node->name;
    timer->act.flags = IRQF_TIMER | IRQF_SHARED;
    timer->act.dev_id = timer;
    timer->act.handler = bcm2835_time_interrupt;

    ret = setup_irq(irq, &timer->act);
    if (ret) {
        pr_err("Can't set up timer IRQ\n");
        goto err_iounmap;
    }

    clockevents_config_and_register(&timer->evt, freq, 0xf, 0xffffffff);

    pr_info("bcm2835: system timer (irq = %d)\n", irq);

    return 0;

err_iounmap:
    iounmap(base);
    return ret;
}
```

では、この関数を詳しく見ていきましょう。

```
    base = of_iomap(node, 0);
    if (!base) {
        pr_err("Can't remap registers\n");
        return -ENXIO;
    }
```

関数はメモリレジスタのマッピングとレジスタのベースアドレスの取得から始まります。
この部分はすでにおなじみでしょう。

```
    ret = of_property_read_u32(node, "clock-frequency", &freq);
    if (ret) {
        pr_err("Can't read clock-frequency\n");
        goto err_iounmap;
    }

    system_clock = base + REG_COUNTER_LO;
    sched_clock_register(bcm2835_sched_read, 32, freq);
```

次に `sched_clock`サブシステムを初期化します。`sched_clock`は実行されるたびに
タイマカウンタレジスタにアクセスする必要があり、このタスクを支援するために
`bcm2835_sched_read`が第一引数として渡されます。第二引数はタイマカウンタの
ビット数（ここでは32）です。ビット数はカウンタが0に戻る時間を計算するために
使用されます。最後の引数にはタイマ周波数を指定します。これはタイマカウンタ値を
ナノ秒に変換するために使用されます。タイマ周波数はデバイスツリーの
[`bcm283x.dtsi#L65`](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L65)で
定義されています。

```
    clocksource_mmio_init(base + REG_COUNTER_LO, node->name,
        freq, 300, 32, clocksource_mmio_readl_up);
```

次の行はクロックソースフレームワークを初期化します。[clocksource_mmio_init](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/mmio.c#L52)は
メモリマップドレジスタに基づいたシンプルなクロックソースを初期化します。クロック
ソースフレームワークは`sched_clock`の機能と重複している部分があり、`sched_clock`と
同じ次の3つの基本パラメータにアクセスする必要があります。

* タイマカウンタレジスタのアドレス
* カウンタの有効ビット数
* タイマ周波数

残りの3つのパラメータは、クロックソースの名前、クロックソーデバイスの評価に使用
されるレーティング、タイマカウンタレジスタを読み込む関数です。

```
    irq = irq_of_parse_and_map(node, DEFAULT_TIMER);
    if (irq <= 0) {
        pr_err("Can't parse IRQ\n");
        ret = -EINVAL;
        goto err_iounmap;
    }
```

このコードスニペットは3番目のタイマ割り込みに対応するLinux irq番号を見つけるために
使用されます（番号3は[DEFAULT_TIMER](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L108)定数に
ハードコードされています）。ここでちょっと注意すると、Raspberry Piのシステムタイマには
4つの独立したタイマレジスタセットがあり、ここでは3番目のレジスタを使用しています。
デバイスツリーに戻ると[interrupts](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L60)プロパティがあります。このプロパティには、デバイスが
サポートする全ての割り込みとそれらの割り込みが割り込みコントローララインにどのように
マッピングされるているかが記述されています。このプロパティは配列であり、各項目が
1つの割り込みに対応します。項目のフォーマットは割り込みコントローラに固有のものです。
このケースでは、各項目は2つの数字で構成されています。最初の数字は割り込みバンクを、
2番めの数字はそのバンク内の割り込み番号を各々指定しています。[irq_of_parse_and_map](https://github.com/torvalds/linux/blob/v4.14/drivers/of/irq.c#L41)は
`interrupts`プロパティの値を読み込み、2番目の引数を使用してサポートされている
割り込みのどれに興味があるかを調べ、要求された割り込みのLinux irq番号を返します。

```
    timer = kzalloc(sizeof(*timer), GFP_KERNEL);
    if (!timer) {
        ret = -ENOMEM;
        goto err_iounmap;
    }
```

ここでは`bcm2835_timer`構造体用のメモリを割り当てています。

```
    timer->control = base + REG_CONTROL;
    timer->compare = base + REG_COMPARE(DEFAULT_TIMER);
    timer->match_mask = BIT(DEFAULT_TIMER);
```

次に、コントロールレジスタと比較レジスタのアドレスを計算し、`match_mask`に
`DEFAULT_TIMER`定数を設定します。

```
    timer->evt.name = node->name;
    timer->evt.rating = 300;
    timer->evt.features = CLOCK_EVT_FEAT_ONESHOT;
    timer->evt.set_next_event = bcm2835_time_set_next_event;
    timer->evt.cpumask = cpumask_of(0);
```

このコードスニペットでは[clock_event_device](https://github.com/torvalds/linux/blob/v4.14/include/linux/clockchips.h#L100)
構造体を初期化しています。ここで最も重要なプロパティは[bcm2835_time_set_next_event](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L57)
関数を指す`set_next_event`です。`bcm2835_time_set_next_event`は非常にシンプルで、
指定された間隔で割り込みがスケジュールされるようにコンペアレジスタを更新します。
これはRPi OSの[`timer.c#L17`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson03/src/timer.c#L17)で行っていることに相当します。

```
    timer->act.flags = IRQF_TIMER | IRQF_SHARED;
    timer->act.dev_id = timer;
    timer->act.handler = bcm2835_time_interrupt;
```

次に、irqアクションを初期化します。ここで最も重要なプロパティは`handler`で、
割り込みの発生後に呼び出される関数である[bcm2835_time_interrupt](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L67) を
指しています。この関数を見ると、クロックイベントフレームワークで登録された
イベントハンドラにすべての作業をリダイレクトしていることがわかります。このイベント
ハンドラについてはすぐ後で検討します。

```
    ret = setup_irq(irq, &timer->act);
    if (ret) {
        pr_err("Can't set up timer IRQ\n");
        goto err_iounmap;
    }
```

irqアクションを構成したら、タイマ割り込みのirqアクションテーブルに追加します。

```
    clockevents_config_and_register(&timer->evt, freq, 0xf, 0xffffffff);
```

最後に[clockevents_config_and_register](https://github.com/torvalds/linux/blob/v4.14/kernel/time/clockevents.c#L504)を
呼び出してクロックイベントフレームワークを初期化します。最初の2つの引数として
`evt`構造体とタイマ周波数が渡されます。残りの2つの引数は"ワンショット"タイマモードの
場合のみ使用されるもので今回の議論には関係ありません。

さて、タイマ割り込みの経路を`bcm2835_time_interrupt`関数まで辿ってきましたが、
まだ、実際の作業が行われている場所を見つけていません。次の節ではクロックイベント
フレームワークに入ってきた割り込みがどのように処理されるのかさらに深く掘り下げて
いきます。

### クロックいえbンとフレームワークに置いて割り込みはどのように処理されるか

前節では、タイマ割り込みを処理する実際の作業はクロックイベントフレームワークに
委託されていることを確認しました。これは[`bcm2835_timer.c#L74`](https://github.com/torvalds/linux/blob/v4.14/drivers/clocksource/bcm2835_timer.c#L74)の
数行で行われています。

```
        event_handler = ACCESS_ONCE(timer->evt.event_handler);
        if (event_handler)
            event_handler(&timer->evt);
```

さて、私たちの目標は`event_handler`が正確にどこで設定され、それが呼ばれると
何が起こるかを解明することです。

[clockevents_config_and_register](https://github.com/torvalds/linux/blob/v4.14/kernel/time/clockevents.c#L504)関数は探索を開始するのに適した場所です。なぜなら、ここは
クロックイベントフレームワークが構成される場所であり、この関数の論理に従うなら、
最終的に`event_handler`がどのように設定されるかがわかるはずだからです。

それでは、必要な場所に導いてくれる関数呼び出しの連鎖をお見せしましょう。

1. [clockevents_config_and_register](https://github.com/torvalds/linux/blob/v4.14/kernel/time/clockevents.c#L504) これはトップレベルの初期化関数です。
2. [clockevents_register_device](https://github.com/torvalds/linux/blob/v4.14/kernel/time/clockevents.c#L449) この関数の中で、タイマがクロックイベントデバイスの
グローバルリストに追加されます。
3. [tick_check_new_device](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-common.c#L300)  この関数はカレントデバイスが「tickデバイス」として使用
ための候補としてふさわしいかをチェックします。もしそうであれば、そのデバイスが周期
tickを生成するデバイスとして使用され、カーネルの他の部分が定期的に行う必要のあるすべての作業を行う際に使用されます。
4. [tick_setup_device](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-common.c#L177) この関数はデバイスの構成を開始します。
5. [tick_setup_periodic](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-common.c#L144) デバイスを周期tick用に構成します。
6. [tick_set_periodic_handler](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-broadcast.c#L432)  ようやくハンドラが割り当てられる場所に到着しました。

コールチェーンの最後の関数を見てみると、Linuxはブロードキャストが有効か否かに
応じて異なるハンドラを使用していることがわかります。tickブロードキャストは
アイドル状態のCPUを目覚めさせるために使われます。詳しくは[これ]](https://lwn.net/Articles/574962/)を読んでください。しかし、ここではそれを無視して、代わりにもっと
一般的なtickハンドラに集中することにします。

一般的なケースでは、[tick_handle_periodic](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-common.c#L99)と
[tick_periodic](https://github.com/torvalds/linux/blob/v4.14/kernel/time/tick-common.c#L79)
関数が呼び出されます。後者がまさに私たちが興味を持っている関数です。その内容を
次に示します。

```
/*
 * Periodic tick
 */
static void tick_periodic(int cpu)
{
    if (tick_do_timer_cpu == cpu) {
        write_seqlock(&jiffies_lock);

        /* Keep track of the next tick event */
        tick_next_period = ktime_add(tick_next_period, tick_period);

        do_timer(1);
        write_sequnlock(&jiffies_lock);
        update_wall_time();
    }

    update_process_times(user_mode(get_irq_regs()));
    profile_tick(CPU_PROFILING);
}
```

この関数では、いくつかの重要なことが行われます。

1. 次のtickイベントがスケジューリングするための`tick_next_period`を計算します。
2.  'jiffies'の設定を行う[do_timer](https://github.com/torvalds/linux/blob/v4.14/kernel/time/timekeeping.c#L2200)を呼び出します。`jiffies`は最新のシステム再起動
からのtick数です。`jiffies`は`sched_clock`関数と同じように、ナノ秒単位の精度を
必要としない場合に使用できます。
3. [update_process_times](https://github.com/torvalds/linux/blob/v4.14/kernel/time/timer.c#L1583)を呼び出します。これは現在実行中のプロセスに定期的に行う必要のある
すべての作業を行う機会を与える場所です。この作業には、たとえば、ローカルプロセス
タイマの実行や、最も重要な作業としてはtickイベントのスケジューラへの通知などが
あります。

### 結論

普通のタイマ割り込みの道のりがどれほど長いかはお分かりいただけたと思いますが、
最初から最後まで追ってみました。最も重要なことの一つは、最終的にスケジューラが
呼び出される場所に到達したことです。スケジューラはOSの中でも最も重要な要素の
一つであり、タイマ割り込みに大きく依存しています。スケジューラの機能がどこで
起動されるかがわかったので、その実装について議論する時がきました。これについては
次のレッスンで行いましょう。

##### 前ページ

3.3 [割り込み処理: 割り込みコントローラ](../../../ja/lesson03/linux/interrupt_controllers.md)

##### 次ページ

3.5 [割り込み処理: 演習](../../../ja/lesson03/exercises.md)
