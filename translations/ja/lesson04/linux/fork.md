## 4.3: タスクをフォークする

スケジューリングとは、利用可能なタスクのリストから実行する適切なタスクを選択する
ことです。しかし、スケジューラが仕事をするにはこのリストをどうにかして埋める
必要があります。新しいタスクを作成する方法がこの章の主なテーマです。

今のところ、焦点はカーネルスレッドだけに当て、ユーザモードの機能性の議論は次の
レッスンまで延期したいと思います。ただし、どこでも可能というわけではありません
ので、ユーザモードでのタスク実行についても少し学ぶ準備をします。

### Initタスク

カーネルが起動すると1つのタスクが実行しています。initタスクです。対応する
`task_struct`は[`init_task.c#L20`](https://github.com/torvalds/linux/blob/v4.14/init/init_task.c#L20)で
定義されており、[INIT_TASK](https://github.com/torvalds/linux/blob/v4.14/include/linux/init_task.h#L226)マクロにより初期化されています。このタスクはシステムにとって
重要です。システムの他のすべてのタスクはこのタスクから派生するからです。

### 新規タスクを作成する

Linuxでは新規タスクをゼロから作ることはできません。代わりに、すべてのタスクは
現在実行中のタスクからフォークします。最初のタスクがどこから来たのかはすでに
知っているので、そこから新規タスクをどのように作るかを探って行きましょう。

新規タスクの作成には4つの方法があります。

1. [fork](http://man7.org/linux/man-pages/man2/fork.2.html) システムコールは、
カレントプロセスの仮想メモリを含む完全なコピーを作成し、新規プロセス（スレッドでは
ない）の作成に使用します。このシステムコールは[`fork.c#L2116`](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2116)で定義されています。
2. [vfork](http://man7.org/linux/man-pages/man2/vfork.2.html) システムコールは
`fork`と似ていますが、子が親の仮想メモリだけでなくスタックも再利用する点と
子の実行が終わるまで親がブロックされる点が異なります。このシステムコールの定義は
[`fork.c#L2128`](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2128)
にあります。
3. [clone](http://man7.org/linux/man-pages/man2/clone.2.html) システムコールは
最も柔軟な方法であす。このシステムコールもカレントタスクをコピーしますが
`flags`パラメータを使ってプロセスをカスタマイズしたり、子タスクのエントリ
ポイントを設定したりすることができます。次のレッスンでは、`glibc`のcloneラッパー
関数がどのように実装されているかを見ていきます。このラッパー関数は`clone`システム
コールを使って新規スレッドを作成することもできます。
4. 最後に、[kernel_thread](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2109) 関数は新規カーネルスレッドの作成に使用できます。

これらの関数はすべて[_do_fork](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2020)を呼び出しますが、次の引数を受け付けます。

* `clone_flags` フラグはフォークの振る舞いの設定に使用されます。フラグの完全な
リストは[`sched.h#L8`](https://github.com/torvalds/linux/blob/v4.14/include/uapi/linux/sched.h#L8)にあります。
* `stack_start` `clone`システムコールの場合、このパラメータは新規タスク用の
ユーザスタックの場所を示します。`kernel_thread`が`_do_fork`を呼び出す場合は
このパラメータはカーネルスレッドで実行する必要のある関数を指します。
* `stack_size` `arm64`アーキテクチャでは，このパラメータは`_do_fork`が
`kernel_thread'から呼び出される場合にのみ使用されます。これはカーネルスレッド
関数に渡す必要のある引数へのポインタです（そう、私もこの2つのパラメータの
名前は誤解を招くと思います）。
* `parent_tidptr` `child_tidptr` この2つのパラメータは`close`システムコールでのみ
使用されます。forkは小スレッドのIDを親のメモリの`parent_tidptr`の位置に格納
しますが、親のIDを`parent_tidptr`の位置に格納することもできます。
* `tls`  [スレッドローカルストレージ](https://en.wikipedia.org/wiki/Thread-local_storage)

### forkの手続き

次に、`_do_fork`の実行中に発生する最も重要なイベントを、その発生順に
紹介します。

1. [_do_fork](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2020) は
[copy_process](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L1539)を呼び出します。`copy_process`は新しい`task_struct`の設定を担当します。
2. `copy_process`は[dup_task_struct](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L512)を呼び出します。この関数は新しい`task_struct`を割り当て、
元のタスクからすべてのフィールドをコピーします。実際のコピー作業はアーキテクチャ
固有の[arch_dup_task_struct](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/process.c#L244)で行われます。
3. 新規カーネルタスクが割り当てられます。`CONFIG_VMAP_STACK`が有効な場合は、
カーネルは[仮想マップドスタック](https://lwn.net/Articles/692208/)を使って
カーネルスタックオーバーフローから保護します。[リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L525)
4. タスクのクレデンシャルがコピーされます。[・リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L1628)
5. スケジューラに新規タスクがフォークされたことが通知されます。[リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L1727)
6. CFSスケジューラクラスの[task_fork_fair](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L9063)メソッドが呼び出されます。このメソッドは現在
実行中の他スックの`vruntime`の値を更新します（これは[update_curr](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L827)
関数で行われ、（[update_min_vruntime](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L514)関数で）カレントランキューの`min_vruntime`を更新します）。
そして、`min_runtime`値がフォークされたタスクに割り当てられます。これにより
このタスクが次に選択されることが保証されます。なお、この時点では新規タスクはまだ
`task_timeline`に追加されていないことに注意してください。
7. ファイルシステムに関する情報やオープンファイル、仮想メモリ、シグナル、名前
空間など数多くの様々なプロパティがカレントタスクから再利用またはコピーされます。
カレントプロパティをコピーするか、再利用するかの判断は通常`clone_flags`パラメタ
に基づいて行われます。[リンク](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L1731-L1765)
8. [copy_thread_tls](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L1766)が呼び出され、それがアーキテクチャ固有の[copy_thread](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/process.c#L254)
関数を呼び出します。この関数は特に注目に値します。RPi OSにおける[copy_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson04/src/fork.c#L5)
関数のプロトタイプとなるからです。これについては更に深く調査したいと思います。

### `copy_thread`関数

関数のソースを以下に示します。

```
int copy_thread(unsigned long clone_flags, unsigned long stack_start,
        unsigned long stk_sz, struct task_struct *p)
{
    struct pt_regs *childregs = task_pt_regs(p);

    memset(&p->thread.cpu_context, 0, sizeof(struct cpu_context));

    if (likely(!(p->flags & PF_KTHREAD))) {
        *childregs = *current_pt_regs();
        childregs->regs[0] = 0;

        /*
         * Read the current TLS pointer from tpidr_el0 as it may be
         * out-of-sync with the saved value.
         */
        *task_user_tls(p) = read_sysreg(tpidr_el0);

        if (stack_start) {
            if (is_compat_thread(task_thread_info(p)))
                childregs->compat_sp = stack_start;
            else
                childregs->sp = stack_start;
        }

        /*
         * If a TLS pointer was passed to clone (4th argument), use it
         * for the new thread.
         */
        if (clone_flags & CLONE_SETTLS)
            p->thread.tp_value = childregs->regs[3];
    } else {
        memset(childregs, 0, sizeof(struct pt_regs));
        childregs->pstate = PSR_MODE_EL1h;
        if (IS_ENABLED(CONFIG_ARM64_UAO) &&
            cpus_have_const_cap(ARM64_HAS_UAO))
            childregs->pstate |= PSR_UAO_BIT;
        p->thread.cpu_context.x19 = stack_start;
        p->thread.cpu_context.x20 = stk_sz;
    }
    p->thread.cpu_context.pc = (unsigned long)ret_from_fork;
    p->thread.cpu_context.sp = (unsigned long)childregs;

    ptrace_hw_copy_thread(p);

    return 0;
}
```

このコードの中にはすでに少し馴染みのあるものもあるでしょう。それでは早速、
コードを見てみましょう。

```
struct pt_regs *childregs = task_pt_regs(p);
```

この関数は新規[pt_regs](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/ptrace.h#L119)
構造体の割り当てから始まります。この構造体は`kernel_entry`で保存されるレジスタ
へのアクセスを提供するために使用されます。`childregs`変数は新規作成されるタスクに
必要なあらゆる状態を準備するために使用できます。タスクがユーザーモードに移行する
場合、その状態は`kernel_exit`マクロによって復元されます。ここで理解すべき重要な
ことは[task_pt_regs](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/processor.h#L161)
マクロは何も割り当てないということです。このマクロはカーネルスタック上の位置を
計算するだけであり、それは`kernel_entry`がレジスタを保存する場所です。新規作成
されたタスクではこの位置は常にカーネルスタックのトップになります。

```
memset(&p->thread.cpu_context, 0, sizeof(struct cpu_context));
```

次に、フォークしたタスクの`cpu_context`をクリアします。

```
if (likely(!(p->flags & PF_KTHREAD))) {
```

そして、カーネルスレッドとユーザースレッドのどちらを作成しているかをチェック
します。今のところ、カーネルスレッドにのみ関心があります。2番目のオプションに
ついては次のレッスンで説明します。

  ```
  memset(childregs, 0, sizeof(struct pt_regs));
  childregs->pstate = PSR_MODE_EL1h;
  if (IS_ENABLED(CONFIG_ARM64_UAO) &&
      cpus_have_const_cap(ARM64_HAS_UAO))
          childregs->pstate |= PSR_UAO_BIT;
  p->thread.cpu_context.x19 = stack_start;
  p->thread.cpu_context.x20 = stk_sz;
  ```

カーネルスレッドを作成している場合、`cpu_context`の`x19`と`x20`レジスタは
実行する必要のある関数 (`stack_start`) とその引数 (`stk_sz`) を指すように
設定します。CPUがフォークされたタスクに切り替わると[ret_from_fork](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L942)は
これらのレジスタを使って必要な関数にジャンプします。（ここで、なぜ
`childregs->pstate`をセットする必要があるのかは私にはわかりません。
`ret_from_fork`は`x19`に格納されている関数にジャンプする前に`kernel_exit`を
呼び出すことはありませんし、仮にカーネルスレッドがユーザモードに移行することに
なっても`childregs`はいずれにせよ上書きされてしまうからです。誰か教えて下さい。）

```
p->thread.cpu_context.pc = (unsigned long)ret_from_fork;
p->thread.cpu_context.sp = (unsigned long)childregs;
```

次に、`cpu_context.pc`に`ret_from_fork`ポインタを設定します。これにより
最初のコンテキストスイッチ後に`ret_from_fork`に返ることが保証されます。
`cpu_context.sp`には`childregs`の直前の位置を設定します。依然として
`childregs`はカーネルトップにある必要があります。なぜなら、カーネルスレッドが
実行を終了すると、タスクはユーザーモードに移行し、`childregs`構造体が使用される
からです。次回のレッスンでは、これがどのように行われるかを詳しく説明します。

以上、`copy_thread`関数について説明しました。では、先ほどのfork手続きの続きの
場所に戻りましょう。

### forkの手続き（続き）

1. `copy_process`がフォークされるタスクの`task_struct`を正しく準備したら、
`_do_fork`は[wake_up_new_task](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L2438)を
呼び出してそのタスクを実行することができます。これは[`fork.c#L2074`](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2074)で
行っています。次に、タスクの状態を`TASK_RUNNING`に変更し、[enqueue_task_fair](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L4879)
 CFSメソッドが呼び出され、このメッソ度が実際にタスクを`task_timeline`赤黒木に
 追加する[__enqueue_entity](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L549)の
 実行を開始します。

2. [`core.c#L2463`](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L2463)では[check_preempt_curr](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L871)を
呼び出し、これが[check_preempt_wakeup](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/fair.c#L6167)
 CFSメソッドを呼び出します。このメソッドはカレントタスクを他のタスクでプリエンプト
するかチェックします。これはまさにするべきことです。なぜなら、最小の`vruntime`値を
持つ新規タスクをタイムラインに追加したからです。そして、[resched_curr](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L479)
関数がトリガされ、カレントタスクに`TIF_NEED_RESCHED`フラグを設定します。

3. カレントタスクが例外ハンドラ（`fork`, `vfork`, `clone`はいずれもシステム
コールであり、それぞれのシステムコールは特別なタイプの例外です）から抜け出る
直前に`TIF_NEED_RESCHED`がチェックされます。チェックは[`entry.S#L801`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L801)で
行われています。[_TIF_WORK_MASK](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/thread_info.h#L109)には`_TIF_NEED_RESCHED`が含まれていることに
注意してください。また、カーネルスレッドの作成の場合、新規スレッドは次のタイマ
ティックまで、あるいは親タスクが自発的に`schedule()`を呼び出すまで開始されない
ことを理解しておくことも重要です。

4. カレントタスクを再スケジューリングする必要がある場合は、[do_notify_resume]が
トリガーされ、これが[schedule](https://github.com/torvalds/linux/blob/v4.14/kernel/sched/core.c#L3418)を
呼び出します。ついにタスクのスケジューリングがトリガされるところまで来ましたので
ここで終了します。

### 結論

これで新規タスクが作成され、スケジューラに追加される方法を理解しました。
次は、スケジューラ自体がどのように動作し、コンテキストスイッチがどのように
実装されているかを見る時が来ました。これについては、次の章で紹介します。

##### 前ページ

4.2 [プロセススケジューラ: スケジューラの基本構造体](../../../ja/lesson04/linux/basic_structures.md)

##### 次ページ

4.4 [プロセススケジューラ: スケジューラ](../../../ja/lesson04/linux/scheduler.md)
