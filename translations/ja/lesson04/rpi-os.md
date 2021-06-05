## 4.1: スケジューラ

RPi OSはもうすでにかなり複雑なベアメタルプログラムになっていますが、正直なところ、
まだOSとは呼べません。なぜなら、OSが行うべきコアタスクが何もできないからです。
そのようなコアタスクの一つにプロセススケジューリングというものがあります。
スケジューリングとは、OSがCPU時間を複数のプロセス間で共有できるようにすることです。
難しいのはプロセスにはスケジューリングが行われていることを意識させてはならない
ことです。プロセスには自分だけがCPUを独占しているように思わせる必要があります。
このレッスンでは、この機能をRPi OSに追加しましょう。

### task_struct

プロセスを管理するためには、まず、プロセスを記述する構造体を作る必要があります。
Linuxにはそのような構造体があり、`task_struct`と呼ばれています（Linuxでは
スレッドとプロセスはタイプが異なるタスクにすぎません）。ここでは、ほとんど
Linuxの実装を真似ているので、同じようにします。RPi OSの[task_struct](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/sched.h#L36)は
次のようになっています。

```
struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long sp;
    unsigned long pc;
};

struct task_struct {
    struct cpu_context cpu_context;
    long state;
    long counter;
    long priority;
    long preempt_count;
};
```

この構造体には次のメンバーがあります。

* `cpu_context` これは、切り替えられるタスク間で異なる可能性のあるすべての
レジスタの値を含む埋め込みの構造体です。なぜすべてのレジスタではなく，レジスタ
`x19 - x30`と`sp`だけ（`fp`は`x29`、`pc`は`x30`なので）を保存するのかという
疑問は当然です。その答えは，実際のコンテキストスイッチはタスクが[cpu_switch_to](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.S#L4)
関数を呼び出したときに初めて行われるからです。つまり、切り替えられるタスクの観点
から言えば、`cpu_switch_to`関数を呼び出して、ある程度の（長い場合もありますが）
時間が経ってから復帰するだけです。この間に別のタスクが実行されたことをタスクは
知りません。ARMの呼び出し規約では、レジスタ`x0 - x18`は呼び出された関数によって
上書きされる可能性があるため、呼び出し側はこれらのレジスタの値が関数呼び出し後に
保存されていると仮定してはいけません。これがレジスタ`x0 - x18`の保存が意味のない
理由です。
* `state` これは現在実行中のタスクの状態です。たった今CPU上で何らかの作業をしている
タスクの状態は常に[TASK_RUNNING](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/sched.h#L15)です。実は、今のところRPi OSがサポートして
いる状態はこの状態だけです。しかし、後でいくつかの状態を追加しなければならなくなり
ます。たとえば、割り込みを待っているタスクは別の状態に移さなければなりません。
必要な割り込みがまだ起こっていないのにタスクを目覚めさせるのは意味がないからです。
* `counter` このフィールドは、現在のタスクがどれだけの間、実行されてきたかを決定する
ために使用されます。`counter`はタイマの1ティックごとに1ずつ減少し、0になると別の
タスクがスケジュールされます。
* `priority`  新しいタスクがスケジュールされると、その`priority`が`counter`に
コピーされます。タスクの優先度を設定することにより、他のタスクと比較してタスクが
取得するプロセッサ時間を調整することができます。
* `preempt_count` このフィールドがゼロ以外の値を持つ場合、現在のタスクが割り込みを
受けてはならないクリティカルな機能を実行していることを示します (たとえば、
スケジューリング機能を実行している場合)。そのような時にタイマティックが発生しても、
それは無視され、再スケジューリングはトリガされません。

カーネルの起動後は1つのタスクだけが実行されています。これは[kernel_main](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/kernel.c#L19)
関数を実行するタスクです。これは「initタスク」と呼ばれます。スケジューラ機能を有効に
する前に、initタスクに対応する`task_struct`を埋める必要があります。これは
 [sched.h#L53](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/sched.h#L53)で
 行っています。

すべてのタスクは[task](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L7)
配列に格納されます。この配列には64のスロットしかありません。これが、RPi OSで同時に
実行できるタスクの最大数です。もちろんこれは製品レベルのOSとしてはベストな
ソリューションではありませんが、私たちの目的には問題ありません。

また、常に現在実行中のタスクを指している[current](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L6)と
いう非常に重要な変数もあります。`current`も`task`配列も初期状態ではinitタスクへの
ポインタが設定されています。また、システムで現在実行中のタスクの数を保持する
[nr_tasks](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L8)と
いうグローバル変数もあります。

以上がスケジューラ機能を実装するために使用するすべての構造体とグローバル変数です。
スケジューリングの仕組みについてはすでに`task_struct`の説明の中で簡単に触れましたが、
それは、特定の`task_struct`フィールドの意味を理解するにはそのフィールドがどのように
使われているかを理解することが不可欠だからです。今からはスケジューリングアルゴリズムの
詳細を見ていきますが、まずは`kernel_main`関数から始めましょう。

### `kernel_main`関数

スケジューラの実装を掘り下げる前に、スケジューラが実際に動作することを証明する方法を
簡単に紹介したいと思います。これを理解するには、[kernel.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/kernel.c)
ファイルを見てみる必要があります。関連する内容をここにコピーしました。

```
void kernel_main(void)
{
    uart_init();
    init_printf(0, putc);
    irq_vector_init();
    timer_init();
    enable_interrupt_controller();
    enable_irq();

    int res = copy_process((unsigned long)&process, (unsigned long)"12345");
    if (res != 0) {
        printf("error while starting process 1");
        return;
    }
    res = copy_process((unsigned long)&process, (unsigned long)"abcde");
    if (res != 0) {
        printf("error while starting process 2");
        return;
    }

    while (1){
        schedule();
    }
}
```

このコードにはいくつかの重要な点があります。

1. 新しい関数[copy_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/fork.c#L5)が
導入されています。`copy_process`は2つの引数を取ります。新しいスレッドで実行する
関数と、この関数に渡す引数です。`copy_process`は新しく`task_struct`を割り当て、
スケジューラが利用可能にします。
1. もうひとつの新しい関数は[schedule](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L21)です。
これはスケジューラの中核的な関数です。この関数は、現在のタスクをプリエンプト
する必要のある新しいタスクがないかチェックします。タスクはその時点で行うべき
ことがない場合、自発的に`schedule`を呼び出すことができます。 `schedule`は
タイマ割り込みハンドラからも呼び出されます。

ここでは`copy_process`を2回呼び出していますが、毎回第一引数に[process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/kernel.c#L9)
関数へのポインタを渡しています。`process`関数は非常にシンプルです。

```
void process(char *array)
{
    while (1){
        for (int i = 0; i < 5; i++){
            uart_send(array[i]);
            delay(100000);
        }
    }
}
```

引数として渡された配列の文字を画面に表示し続けるだけです。最初に呼び出された時の
引数は"12345"で、2回目の引数は"abcde"です。スケジューラの実装が正しければ、両
スレッドからの出力が混在して画面に表示されるはずです。

### メモリ割り当て

システム内の各タスクは専用のスタックを持つ必要があります。そのため、新しいタスクを
作成する際にはメモリを割り当てる方法が必要です。今のところ、私たちのメモリアロケータは
極めて原始的なものです(実装は[mm.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/mm.c)ファイルにあります)。

```
static unsigned short mem_map [ PAGING_PAGES ] = {0,};

unsigned long get_free_page()
{
    for (int i = 0; i < PAGING_PAGES; i++){
        if (mem_map[i] == 0){
            mem_map[i] = 1;
            return LOW_MEMORY + i*PAGE_SIZE;
        }
    }
    return 0;
}

void free_page(unsigned long p){
    mem_map[(p - LOW_MEMORY) / PAGE_SIZE] = 0;
}
```

このアロケータはメモリページ単位（1ページのサイズは4KB）でのみ動作します。システム
内の各ページの状態を保持する`mem_map`と呼ばれる配列があり、ページが割り当て
済みか、未使用かを示しています。そして、新しいページを割り当てる必要がある度に、
この配列をループして、最初の空きページを返すだけです。この実装は2つの仮定に
基づいています。

1. システムに搭載されているメモリの総量を知っている。それは`1GB - 1MB`です（最後の
1メガバイトはデバイスレジスタ用に予約されています）。この値は、[HIGH_MEMORY](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/mm.h#L14)
定数に格納されています。
2. メモリの最初の4MBは、カーネルイメージとinitタスクスタック用に予約されている。
この値は、[LOW_MEMORY](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/mm.h#L13)
定数に格納されています。すべてのメモリの割り当てはこのアドレスの直後から始まります。

### 新しいタスクを作成する

新しいタスクの割当は[copy_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/fork.c#L5)
関数で実装されています。

```
int copy_process(unsigned long fn, unsigned long arg)
{
    preempt_disable();
    struct task_struct *p;

    p = (struct task_struct *) get_free_page();
    if (!p)
        return 1;
    p->priority = current->priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; //disable preemtion until schedule_tail

    p->cpu_context.x19 = fn;
    p->cpu_context.x20 = arg;
    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)p + THREAD_SIZE;
    int pid = nr_tasks++;
    task[pid] = p;
    preempt_enable();
    return 0;
}
```

では、これを詳しく見ていきましょう。

```
    preempt_disable();
    struct task_struct *p;
```

この関数はプリエンプションを無効にし、新しいタスクのためのポインタを割り当てる
ことから始まります。プリエンプションを無効にするのは`copy_process`関数の途中で
別のタスクにリスケジュールされたくないからです。

```
    p = (struct task_struct *) get_free_page();
    if (!p)
        return 1;
```

次に、新しいページを割り当てます。このページの最下位アドレスから新しく
作成したタスクの`task_struct`を配置します。このページの残りの部分は
タスクスタックとして使用されます。

```
    p->priority = current->priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; //disable preemtion until schedule_tail
```

`task_struct`を割り当てたら、そのプロパティを初期化します。優先度と初期カウンタは
カレントタスクの優先度に基づいて設定します。状態には`TASK_RUNNING`を設定し、
新しいタスクは開始する準備ができていることを示します。`preempt_count`には1を設定
します。これは、タスクの実行後、何らかの初期化作業を完了するまではリスケジューリング
してはいけないことを意味します。

```
    p->cpu_context.x19 = fn;
    p->cpu_context.x20 = arg;
    p->cpu_context.pc = (unsigned long)ret_from_fork;
    p->cpu_context.sp = (unsigned long)p + THREAD_SIZE;
```

これはこの関数の最も重要な部分です。ここで`cpu_context`を初期化します。スタック
ポインタには新しく割り当てられたメモリページの最上位アドレスを設定します。`pc`には [ret_from_fork](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L146)関数を設定します。`cpu_context`レジスタの残りの部分を
なぜこのように初期化するかを理解するためにはこの関数を見てみる必要があります。

```
.globl ret_from_fork
ret_from_fork:
    bl    schedule_tail
    mov    x0, x20
    blr    x19         //should never return
```

`ret_from_fork`は、まず[schedule_tail](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L65)を呼び出してプリエンプションを有効にし、
次に、`x19`レジスタに格納されている関数を`x20`に格納されている引数で呼び出している
のがわかります。`x19`と`x20`は`ret_from_fork`関数が呼び出される直前に`cpu_context`
から復元されます。

では、`copy_process`に戻りましょう。

```
    int pid = nr_tasks++;
    task[pid] = p;
    preempt_enable();
    return 0;
```

最後に、`copy_process`は新しく作成したタスクを`task`配列に追加し、カレントタスクの
プリエンプションを有効にします。

`copy_process`関数について理解すべき重要な点は、この関数の実行終了後にコンテキスト
スイッチは起きないことです。この関数は新しい`task_struct`を用意して`task`配列に
追加するだけです。このタスクが実行されるのは`schedule`関数が呼び出された後です。

### 誰が`schedule`を呼び出すのか？

`schedule`関数の詳細を説明する前に、まず、`schedule`がどのように呼び出されるかを
把握しましょう。2つのシナリオがあります。

1. あるタスクが今はやることがないが終了させることはできない場合、自発的に
`schedule`を呼び出すことができます。これは`kernel_main`関数が行っているものです。
2. `schedule`は[タイマ割り込みハンドラ](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/timer.c#L21)ハンドラからも定期的に
呼び出されます。

では、タイマ割り込みから呼び出される[timer_tick](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L70)関数を見てみましょう。

```
void timer_tick()
{
    --current->counter;
    if (current->counter>0 || current->preempt_count >0) {
        return;
    }
    current->counter=0;
    enable_irq();
    _schedule();
    disable_irq();
```

まず最初に、カレントタスクのカウンタを減少させます。カウンタが0より大きい場合、
または現在プリエンプションが禁止されている場合、関数は復帰します。そうでない場合、
割り込みを有効にして`schedule`が呼び出されます（これは割り込みハンドラの中であり、
割り込みはデフォルトで無効になっています）。スケジューラの実行時に割り込みを
有効にしなければならない理由は次のセクションで説明します。

### スケジューリングアルゴリズム

ようやくスケジューラのアルゴリズムを見る準備ができました。このアルゴリズムは
Linuxカーネルの最初のリリースからほぼ正確にコピーしました。オリジナルのバージョンは
[ここ](https://github.com/zavg/linux-0.01/blob/master/kernel/sched.c#L68)にあります。

```
void _schedule(void)
{
    preempt_disable();
    int next,c;
    struct task_struct * p;
    while (1) {
        c = -1;
        next = 0;
        for (int i = 0; i < NR_TASKS; i++){
            p = task[i];
            if (p && p->state == TASK_RUNNING && p->counter > c) {
                c = p->counter;
                next = i;
            }
        }
        if (c) {
            break;
        }
        for (int i = 0; i < NR_TASKS; i++) {
            p = task[i];
            if (p) {
                p->counter = (p->counter >> 1) + p->priority;
            }
        }
    }
    switch_to(task[next]);
    preempt_enable();
}
```

アルゴリズムは以下のようになっています。

* 内側の最初の`for`ループは、すべてのタスクをイテレートし、状態が`TASK_RUNNING`で
最大のカウンタを持つタスクを探します。そのようなタスクが見つかり、そのカウンタが
0より大きい場合、すぐに外側の`while`ループを抜けてそのタスクに切り替えます。
そのようなタスクが見つからない場合は、現在、`TASK_RUNNING`状態のタスクが存在
しないか、`TASK_RUNNING`状態のタスクのカウンタがすべて0であることを意味します。
実際のOSでは前者のケースは起きる可能性があります。たとえば、すべてのタスクが
割り込みを待っている場合です。この場合、2つ目の`for`ループが実行されます。
このループは（その状態にかかわらず）すべてのタスクのカウンタを増加させます。
このカウンタの増加は非常にスマートな方法で行われます。

    1. タスクが2回目の`for`ループを通過すればするだけ、そのカウンタは増大します。
    2. タスクのカウンタは絶対に`2 * priority`よりも大きくなりません。

* その後、このプロセスが繰り返されます。`TASK_RUNNIG`状態のタスクが1つでもあれば
外側の`while`ループの2回目の繰り返しが最後になります。なぜなら、1回目の繰り返しで
すべてのタスクのカウンタはゼロではなくなるからです。しかし、`TASK_RUNNING`状態の
タスクが1つもなければ、どれかのタスクが`TASK_RUNNING`状態に移行するまで、この
プロセスが何度も繰り返されます。しかし、シングルCPUで動作している場合、どのような
場合に、このループの実行中にタスクの状態が変化するのでしょうか。その答えは、ある
タスクが割り込みを待っている場合です。`schedule`関数の実行中にこの割り込みが発生し、
割り込みハンドラがタスクの状態を変更することができるからです。これが`schedule`の
実行中に割り込みを有効にしなければならない理由です。これは割り込みの無効化と
プリエンプションの無効化の重要な違いも示しています。プリエンプションの無効化により、
元の関数の実行中に入れ子になった`schedule`が呼び出されることはありません。
しかし、割り込みは`schedule` 関数の実行中に合法的に発生することができます。

あるタスクが割り込みを待っているという状況にはかなり注意を払いましたが、
この機能はRPi OSにはまだ実装されていません。しかし、このケースはスケジューラの
コアアルゴリズムの一部であり、同様の機能を後に追加する予定ですので理解して
おく必要があると考えています。

### タスクを切り替える

カウンタが0でない`TASK_RUNNING`状態のタスクが見つかると[switch_to](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L56)関数が
呼び出されます。これは次のようなものです。

```
void switch_to(struct task_struct * next)
{
    if (current == next)
        return;
    struct task_struct * prev = current;
    current = next;
    cpu_switch_to(prev, next);
}
```

ここでは、次のプロセスがカレントプロセスと同じでないかチェックし、同じで
なければ`current`変数を更新します。実際の作業は[cpu_switch_to](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.S)関数に
リダイレクトされます。

```
.globl cpu_switch_to
cpu_switch_to:
    mov    x10, #THREAD_CPU_CONTEXT
    add    x8, x0, x10
    mov    x9, sp
    stp    x19, x20, [x8], #16        // store callee-saved registers
    stp    x21, x22, [x8], #16
    stp    x23, x24, [x8], #16
    stp    x25, x26, [x8], #16
    stp    x27, x28, [x8], #16
    stp    x29, x9, [x8], #16
    str    x30, [x8]
    add    x8, x1, x10
    ldp    x19, x20, [x8], #16        // restore callee-saved registers
    ldp    x21, x22, [x8], #16
    ldp    x23, x24, [x8], #16
    ldp    x25, x26, [x8], #16
    ldp    x27, x28, [x8], #16
    ldp    x29, x9, [x8], #16
    ldr    x30, [x8]
    mov    sp, x9
    ret
```

ここが本当の意味でのコンテクストスイッチが行われる場所なのです。一行ずつ見て
いきましょう。

```
    mov    x10, #THREAD_CPU_CONTEXT
    add    x8, x0, x10
```

`THREAD_CPU_CONTEXT`定数には`task_struct`における`cpu_context`構造体のオフセットが
格納されています。`x0`にはこの関数の第1引数へのポインタが格納されており、それは
現在の`task_struct`です（ここで言う現在とは、prevプロセスの`task_struct`を意味します）。
最初の2行が実行された後、`x8`には現在の`cpu_context`へのポインタが格納されます。

```
    mov    x9, sp
    stp    x19, x20, [x8], #16        // store callee-saved registers
    stp    x21, x22, [x8], #16
    stp    x23, x24, [x8], #16
    stp    x25, x26, [x8], #16
    stp    x27, x28, [x8], #16
    stp    x29, x9, [x8], #16
    str    x30, [x8]
```

次に、calleeが保存すべきすべてのレジスタを`cpu_context`構造体で定義された順番で
保存します。リンクレジスタであり，関数のリターンアドレスを保持している`x30`は
`pc`として，カレントスタックポインタは`sp`として、`x29`は`fp`（フレームポインタ）と
して保存します。

```
    add    x8, x1, x10
```

現在`x10`には`task_struct`における`cpu_context`構造体のオフセットが入っており、
`x1`は次（切り替え先）のプロセスの`task_struct`へのポインタなので、`x8`には次の
プロセスの`cpu_context`へのポインタが入ることになります。

```
    ldp    x19, x20, [x8], #16        // restore callee-saved registers
    ldp    x21, x22, [x8], #16
    ldp    x23, x24, [x8], #16
    ldp    x25, x26, [x8], #16
    ldp    x27, x28, [x8], #16
    ldp    x29, x9, [x8], #16
    ldr    x30, [x8]
    mov    sp, x9
```

Calleeが保存したレジスタを次のプロセスの`cpu_context`から復元します。

```
    ret
```

関数はリンクレジスタ(`x30`)で指定された場所に復帰します。何らかのタスクに初めて
切り替わる場合は、それは[ret_from_fork](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L148)関数です。
それ以外の場合は、`cpu_switch_to`関数により`cpu_context`に保存された場所です。

### 例外の入出力があるスケジューリングはどのように動くのか

前回のレッスンでは、[kernel_entry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L17)マクロと
[kernel_exit](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L4)マクロを
使ってプロセッサの状態の保存と復元をする方法を説明しました。スケジューラを導入する
と新たな問題が発生します。あるタスクで割込みに入り、別のタスクで割込みから抜ける
ことができるようになったからです。これが問題となるのは、割り込みから復帰するために
使用する`eret`命令は、復帰アドレスは`elr_el1`に、プロセッサの状態は`spsr_el1`
レジスタに格納されている必要があるという事実に依存しているからです。そのため、
割り込み処理中にタスクを切り替えたい場合は、すべての汎用レジスタと共にこの2つの
レジスタも保存・復元しなければなりません。これを行うコードは非常に簡単で、保存の
部分は[`entry.S#L35`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L35)、復元の部分は[`entry.S#L46`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L46)で見ることができます。

### コンテキストスイッチ中のシステムの状態を追跡する

これまでにコンテキストスイッチに関連するすべてのソースコードを調べてきました。
しかし、このコードには数多くの非同期の動作が関与しており、システム全体の状態が
時間とともにどのように変化するかを完全に理解することは困難です。この節ではこの
理解を容易にしたいと思います。ここでは、システムの起動から2回目のコンテキスト
スイッチが行われるまでの一連のイベントを説明したいと思います。そのようなイベント
ごとに、イベント発生時のメモリの状態を表す図も掲載します。このような方法により
スケジューラがどのように動作するのかを深く理解する手助けとなることを期待します。
では、始めましょう。

1. カーネルが初期化され、[kernel_main](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/kernel.c#L19)
関数が実行されます。初期スタックは、4MBの位置にある[LOW_MEMORY](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/include/mm.h#L13)から
始まるように設定されています。

    ```
             0 +------------------+
               | kernel image     |
               |------------------|
               |                  |
               |------------------|
               | init task stack  |
    0x00400000 +------------------+
               |                  |
               |                  |
    0x3F000000 +------------------+
               | device registers |
    0x40000000 +------------------+
    ```
2. `kernel_main`が[copy_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/fork.c#L5)を
初めて呼び出します。新たに4KBのメモリページが割り当てられ、このページの最下位アドレスに
`task_struct`が配置されます（この時点で作成されたタスクは以後「タスク 1」と呼びます）。

    ```
             0 +------------------+
               | kernel image     |
               |------------------|
               |                  |
               |------------------|
               | init task stack  |
    0x00400000 +------------------+
               | task_struct 1    |
               |------------------|
               |                  |
    0x00401000 +------------------+
               |                  |
               |                  |
    0x3F000000 +------------------+
               | device registers |
    0x40000000 +------------------+
    ```
3. `kernel_main`が2回目の`copy_process`を呼び出し、同じ処理を繰り返します。タスク 2が
作成され、タスクリストに追加されます。

    ```
             0 +------------------+
               | kernel image     |
               |------------------|
               |                  |
               |------------------|
               | init task stack  |
    0x00400000 +------------------+
               | task_struct 1    |
               |------------------|
               |                  |
    0x00401000 +------------------+
               | task_struct 2    |
               |------------------|
               |                  |
    0x00402000 +------------------+
               |                  |
               |                  |
    0x3F000000 +------------------+
               | device registers |
    0x40000000 +------------------+
    ```
4. `kernel_main`は自発的に[schedule](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L21)
関数を呼び出し、タスク　1の実行を決定します。
5. [cpu_switch_to](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.S#L4)は
calee保存レジスタをカーネルイメージ内にあるinitタスクの`cpu_context`に保存します。
6. `cpu_switch_to`はタスク 1の`cpu_context`からcalee保存レジスタを復元します。この
段階で、`sp`は`0x00401000`を、リンクレジスタは[ret_from_fork](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L146)
関数を指しており、`x19`は[process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/kernel.c#L9)
関数へのポインタを、`x20`はカーネルイメージ内のどこかにある文字列 "12345" への
ポインタを保持しています。
7. `cpu_switch_to`は`ret`命令を呼び出し、`ret_from_fork`関数にジャンプします。
8. `ret_from_fork`はレジスタ`x19`と`x20`を読み込み、`process`関数を引数 "12345"で
呼び出します。`process`関数が実行されると、そのスタックが増え始めます。
    ```
             0 +------------------+
               | kernel image     |
               |------------------|
               |                  |
               |------------------|
               | init task stack  |
    0x00400000 +------------------+
               | task_struct 1    |
               |------------------|
               |                  |
               |------------------|
               | task 1 stack     |
    0x00401000 +------------------+
               | task_struct 2    |
               |------------------|
               |                  |
    0x00402000 +------------------+
               |                  |
               |                  |
    0x3F000000 +------------------+
               | device registers |
    0x40000000 +------------------+
    ```
9.  タイマ割り込みが発生します。[kernel_entry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/entry.S#L17)
マクロはすべての汎用レジスタに加えて`elr_el1`と`spsr_el1`をタスク　1のスタックの
下位アドレスに保存します。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 saved registers |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
10. `schedule`が呼び出され、タスク　2の実行を決定します。しかし、タスク　1はまだ実行
しており、そのスタックはタスク　1のレジスタ保存領域の下に成長中です。以下の図では、
スタックのこの部分を(int)と表示しており、これは "割り込みスタック "を意味します。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 stack (int)     |
               |------------------------|
               | task 1 saved registers |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
11. `cpu_switch_to`はタスク 2を実行します。これを行うためにタスク 1とまったく同じ
手順を実行します。タスク 2の実行が開始され、スタックが増えていきます。この時点では
割込みから復帰していないことに注意してください。割込みが有効になっているのでこれで
問題はありません（割込みは`schedule`が呼ばれる前の[timer_tick](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L70)で
有効になっています）。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 stack (int)     |
               |------------------------|
               | task 1 saved registers |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
               |------------------------|
               | task 2 stack           |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
12. 別のタイマ割り込みが発生し、`kernel_entry`はすべての汎用レジスタと`elr_el1`、
`spsr_el1`をタスク 2のスタックの下位アドレスに保存します。タスク 2の割り込み
スタックが増え始めます。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 stack (int)     |
               |------------------------|
               | task 1 saved registers |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
               |------------------------|
               | task 2 stack (int)     |
               |------------------------|
               | task 2 saved registers |
               |------------------------|
               | task 2 stack           |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
13. `schedule`が呼び出されます。すべてのタスクのカウンタが0になっていることを
確認し、カウンタをタスク優先度に合わせて設定します。
14. `schedule` はinitタスクを選択して実行します。(これはすべてのタスクのカウンタが
1に設定されており、initタスクがリストの最初のタスクだからです)。しかし、実際には、
`schedule`がこの時点でタスク1またはタスク2を選択することは完全に合法です。すべての
タスクのカウンタ値は同じだからです。ここではタスク1が選択された場合に興味があるので
それが起こったと仮定してみます。
15. `cpu_switch_to`が呼び出され、タスク 1の`cpu_context`から以前に保存されていた
callee保存レジスタを復元します。現在、リンクレジスタは[`sched.c#L63`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L63)を
指しています。そこが、前回タスク 1の実行中に`cpu_switch_to`が呼び出された場所
だからです。`sp`はタスク 1の割り込みスタックの底を指しています。
16. `timer_tick`関数が[sched.c#L79](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L79)から実行を再開します。そして、割り込みを
無効にし、最後に[kernel_exit](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/sched.c#L79)を実行します。`kernel_exit` が呼ばれるまでに、
タスク 1 の割り込みスタックは0に戻されています。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 saved registers |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
               |------------------------|
               | task 2 stack (int)     |
               |------------------------|
               | task 2 saved registers |
               |------------------------|
               | task 2 stack           |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
17. `kernel_exit`はすべての汎用レジスタと`elr_el1`、`spsr_el1`を復元します。
`elr_el1`は`process`関数の真ん中のどこかを、`sp`はタスク 1のスタックの底を
指しています。

    ```
             0 +------------------------+
               | kernel image           |
               |------------------------|
               |                        |
               |------------------------|
               | init task stack        |
    0x00400000 +------------------------+
               | task_struct 1          |
               |------------------------|
               |                        |
               |------------------------|
               | task 1 stack           |
    0x00401000 +------------------------+
               | task_struct 2          |
               |------------------------|
               |                        |
               |------------------------|
               | task 2 stack (int)     |
               |------------------------|
               | task 2 saved registers |
               |------------------------|
               | task 2 stack           |
    0x00402000 +------------------------+
               |                        |
               |                        |
    0x3F000000 +------------------------+
               | device registers       |
    0x40000000 +------------------------+
    ```
18. `kernel_exit`は`eret`命令を実行し、`elr_el1`レジスタを使用して`process`
関数にジャンプバックします。そして、タスク 1は、通常の実行を再開します。

以上の一連の手順は非常に重要であり、個人的にはチュートリアル全体の中で最も重要な
ものの一つだと考えています。もし、理解するのが難しい場合はこのレッスンの演習1を
やってみることを勧めます。

### 結論

スケジューリングができるようになりましたが、現在、カーネルが管理できるのは
カーネルスレッドだけです。カーネルスレッドはEL1で実行され、カーネルの関数や
データに直接アクセスできてしまいます。次の2つのレッスンでは、この問題を解決する
ために、システムコールと仮想メモリを導入します。


##### 前ページ

3.5 [割り込み処理: 演習](../../ja/lesson03/exercises.md)

##### 次ページ

4.2 [プロセススケジューラ: スケジューラの基本構造](../../ja/lesson04/linux/basic_structures.md)
