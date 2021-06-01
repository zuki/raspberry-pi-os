## 5.1: ユーザプロセスとシステムコール

私たちはこれまでにRPi OSが単なるベアメタルプログラムではなく、実際の
オペレーティン グシステムにみえるように多くの機能を追加してきました。
RPi OSはプロセスを管理できるようになりましたが、この機能にはまだ大きな
欠点があります。プロセスがまったく隔離されていないのです。このレッスンでは、
この問題を解決していきます。まず最初に、すべてのユーザプロセスをEL0に移行させ、
特権的なプロセッサ操作へのアクセスを制限します。このステップがないと、他の
隔離技術は意味をなしません。なぜなら、すべてのユーザプログラムがセキュリティ
設定を書き換えることができ、隔離が破られてしまうからです。

ユーザプログラムがカーネル機能に直接アクセスできないように制限すると、
別の問題が発生します。たとえば、ユーザプログラムがユーザに何かを印刷する
必要があるとしたらどうでしょう。ユーザプログラムがUARTデバイスを直接操作する
ことは絶対に避けたいものです。代わりに、OSがユーザプログラムにAPIメソッド
セットを提供できれば良いでしょうが、そのようなAPIは簡単なメソッドセットと
しては実装できません。なぜなら、ユーザプログラムがAPIメソッドを呼び出すたびに、
現在の例外レベルをEL1に引き上げる必要があるからです。このようなAPIの個々の
メソッドは「システムコール」と呼ばれます。このレッスンではRPi OSにシステム
コールセットを追加します。

プロセスの隔離には3つ目の側面もあります。それは、各プロセスがそれぞれ独立した
メモリビューを持つ必要があることです。この問題についてはレッスン6で取り上げます。

### システムコールの実装

システムコール（syscallと略します）の背後にある主たるアイデアは非常に
シンプルです。つまり、各システムコールは実際には同期例外です。ユーザ
プログラムがsyscallを実行する必要がある場合、まず必要なすべての引数を準備し、
次に`svc`命令を実行しなければなりません。この命令は同期例外を発生させます。
この例外はOSによりEL1で処理されます。OSはまず、すべての引数を検証し、要求
されたアクションを実行し、通常の例外リターンを実行します。これはユーザ
プログラムの実行が`svc`命令の直後からEL0で再開されることを保証します。
RPi OSは4つのシンプルなsyscallを定義しています。

1. `write` このsyscallはUARTデバイスを使って画面に何かを出力します。
このsyscallは、第1引数として出力するテキストを含むバッファを受け取ります。
2. `clone` このsyscallは新しいユーザスレッドを作成します。新しく作成される
スレッドのスタックアドレスが第1引数として渡されます。
3. `malloc` このsyscallはユーザプロセス用にメモリページを割り当てます。
Linuxにこのsyscallに相当するものはありません（他のどのOSにもないと思います）。
このsyscallが必要な唯一の理由は，RPi OSがまだ仮想メモリを実装しておらず，
すべてのユーザプロセスが物理メモリで動作しているからです。どのメモリページが
空きで使用可能を知る方法が各プロセスに必要なのはそのためです。`malloc`
syscallは新しく割り当てたページのポインタを、エラーの場合は`-1`を
返します。
4. `exit` 各プロセスは実行が終了したら必ずこのsyscallを呼ぶ必要があります。
このsyscallは必要なすべてのクリーンアップ処理を行います。

すべてのsyscallは[sys.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.c)ファイルで定義されています。
また，すべてのsyscallハンドラへのポインタを含む配列[sys_call_table](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.c)も
あります。各syscallは「syscall番号」を持っていますが、これは`sys_call_table`
配列のインデックスに過ぎません。すべてのsyscall番号は[`sys.h#L6`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/include/sys.h#L6)で
定義されています。この番号はアセンブラコードがどのシスコールに興味を持って
いるかを指定するために使用されます。`write` syscallを例として、syscallの
ラッパー関数を見てみましょう。ソースは[`sys.S#L4`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.S#L4)にあります。

```
.globl call_sys_write
call_sys_write:
    mov w8, #SYS_WRITE_NUMBER
    svc #0
    ret
```

この関数は非常にシンプルです。syscall番号を`w8`レジスタに格納し、`svc`命令を
実行して同期例外を発生させるだけです。`w8`は規約によりsyscall番号に使用されます。レジスタ`x0 - x7`はsyscallの引数として使用され、`x8`はsyscall番号の格納
に使用されます。この規約により、syscallは最大8つの引数を持つことができます。

通常、このようなラッパー関数はカーネルには含まれていません。[glibc](https://www.gnu.org/software/libc/)などの各言語用の標準ライブラリに含まれていることが多い
ようです。

### 同期例外を処理する

同期例外が発生すると例外テーブルに登録されている[ハンドラ](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/entry.S#L98)が
呼び出されます。ハンドラのソースは[`entry.S#L157`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/entry.S#L157)に
あり、次のようなコードで始まります。

```
el0_sync:
    kernel_entry 0
    mrs    x25, esr_el1                // syndromeレジスタを読み込む
    lsr    x24, x25, #ESR_ELx_EC_SHIFT // 例外クラスを取り出す
    cmp    x24, #ESR_ELx_EC_SVC64      // 64ビットSVC
    b.eq   el0_svc
    handle_invalid_entry 0, SYNC_ERROR
```

まず、他の例外ハンドラと同様に`kernel_entry`マクロが呼び出されます。
次に`esr_el1`（Exception Syndrome Register）がチェックされます。この
レジスタにはオフセット[ESR_ELx_EC_SHIFT](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/include/arm/sysregs.h#L46)に
「例外クラス」フィールドがあります。例外クラスが[ESR_ELx_EC_SVC64](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/include/arm/sysregs.h#L47)と
等しい場合、現在の例外が`svc`命令によって発生したものであり、それがシステム
コールであることを意味します。そうである場合、`el0_svc`ラベルにジャンプし、
そうでない場合はエラーメッセージを表示します。

```
sc_nr   .req    x25                  // システムコールの数
scno    .req    x26                  // syscall番号
stbl    .req    x27                  // syscallテーブルのポインタ

el0_svc:
    adr    stbl, sys_call_table      // syscallテーブルポインタをロード    uxtw   scno, w8                  // syscall番号をw8に設定
    mov    sc_nr, #__NR_syscalls
    bl     enable_irq
    cmp    scno, sc_nr               // syscall番号の上限チェック
    b.hs   ni_sys

    ldr    x16, [stbl, scno, lsl #3] // syscallテーブル中のアドレスを設定
    blr    x16                       // sys_* ルーチンを呼び出す
    b      ret_from_syscall
ni_sys:
    handle_invalid_entry 0, SYSCALL_ERROR
```

`el0_svc`は、まず、`syscall`テーブルのアドレス（`x27`レジスタのエイリアスに
過ぎない）を`stbl`に、syscall番号を`scno`変数にロードします。次に、割り込みを
有効にし、syscall番号とシステムのsyscallの総数を比較します。もし番号が総数
以上の場合、エラーメッセージを表示します。syscall番号が総数の範囲内に収まって
いる場合は、番号をsyscallテーブル配列のインデックスとして使用して、syscall
ハンドラへのポインタを取得します。そして，ハンドラを実行し，ハンドラの終了
後に`ret_from_syscall`を呼び出します。ここではレジスタ`x0 - x7`には触って
いないことに注意してください。これらのレジスタは透過的にハンドラに渡されます。

```
ret_from_syscall:
    bl    disable_irq
    str   x0, [sp, #S_X0]             // 返り値 x0
    kernel_exit 0
```

`ret_from_syscall`はまず割り込みを禁止します。そして、`x0`レジスタの値を
スタックに保存します。これは`kernel_exit`がすべての汎用レジスタを保存された
値から復元するので必要です。現在、`x0`にはsyscallハンドラからの戻り値が
入っているので、この値をユーザコードに渡したいからです。最後に`kernel_exit`を
呼びだし，ユーザコードに戻ります。

### EL0とEL1との間で切り替える

前回のレッスンを注意深く読んだ方は、`kernel_entry`マクロと`kernel_exit`マクロの
変化に気づいたかもしれません。どちらも追加の引数を受け付けるようになっています。
この引数はどの例外レベルからの例外を受け取るかを示します。発生元の例外レベルに
関する情報はスタックポインタを適切に保存/復元するために必要です。以下に `kernel_entry`と`kernel_exit`マクロの関連する2つの部分を示します。

```
    .if    \el == 0
    mrs    x21, sp_el0
    .else
    add    x21, sp, #S_FRAME_SIZE
    .endif /* \el == 0 */
```

```
    .if    \el == 0
    msr    sp_el0, x21
    .endif /* \el == 0 */
```

私たちはEL0用とEL1用に異なるスタックポインタを使用しています。それがEL0で
例外が発生した直後にスタックポインタが上書きされる理由です。元のスタック
ポインタは`sp_el0`レジスタにあります。このレジスタの値は例外ハンドラで
`sp_el0`を触らなくても例外を扱う前後に保存・復元する必要があります。これを
しないとコンテキストスイッチ後に`sp`レジスタの値がおかしくなります。

また、EL1で例外が発生した場合はなぜ`sp`レジスタの値を復元しないのか、という
疑問もあるでしょう。それは、同じカーネルスタックを例外ハンドラで再利用する
からです。たとえ例外処理中にコンテキストスイッチが起こったとしても
`kernel_exitの時点では、`sp`はすでに`cpu_switch_to`関数によって切り替えられて
います(ちなみに，Linuxでは割込みハンドラに別のスタックを使うため動作が
異なります)。

また，`eret`命令の前にどの例外レベルに戻るかを明示的に指定する必要はない
ことにも注目しましょう。この情報は`spsr_el1`レジスタにエンコードされており、
常に例外が発生したレベルに戻ることになるからです。

### タスクをユーザモードに移行する

システムコールを発生させるには、当然ながらユーザモードで動作しているタスクが
必要です。新しいユーザタスクを作成する方法には2つの可能性があります。カーネル
スレッドをユーザモードに移行させるか、ユーザタスクが自分自身をフォークして
新しいユーザタスクを作成するかです。このセクションでは、最初の可能性について
説明します。

実際にこの作業を行う関数は[move_to_user_mode](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/fork.c#Li47)と呼ばれています。
この関数を見ていく前に、まず、この関数がどのように使用されているかを調べて
みましょう。そのためには、[kernel.c](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/kernel.c)ファイルを開く必要が
あります。関連する行をここにコピーしました。

```
    int res = copy_process(PF_KTHREAD, (unsigned long)&kernel_process, 0, 0);
    if (res < 0) {
        printf("error while starting kernel process");
        return;
    }
```

まず、`kernel_main`関数で新しいカーネルスレッドを作成します。これは、前回の
レッスンで行ったのと同じ方法で行います。スケジューラが新規作成されたタスクを
実行すると`kernel_process`関数がカーネルモードで実行されます。

```
void kernel_process(){
    printf("Kernel process started. EL %d\r\n", get_el());
    int err = move_to_user_mode((unsigned long)&user_process);
    if (err < 0){
        printf("Error while moving process to user mode\n\r");
    }
}
```

`kernel_process`はステータスメッセージを表示し、`user_process`へのポインタを
第一引数として`move_to_user_mode`を呼び出します。では、`move_to_user_mode`
関数が何をしているのか見てみましょう。

```
int move_to_user_mode(unsigned long pc)
{
    struct pt_regs *regs = task_pt_regs(current);
    memzero((unsigned long)regs, sizeof(*regs));
    regs->pc = pc;
    regs->pstate = PSR_MODE_EL0t;
    unsigned long stack = get_free_page(); //allocate new user stack
    if (!stack) {
        return -1;
    }
    regs->sp = stack + PAGE_SIZE;
    current->stack = stack;
    return 0;
}
```

今、私たちは、initタスクからフォークして作成されたカーネルスレッドの実行の
ただ中にいます。前回のレッスンでは、フォークプロセスについて説明し、新しく
作成されたタスクのスタックの最上部に小さな領域（`pt_regs`領域）が確保されて
いることを確認しました。今回はこの領域を初めて使用します。手動で準備した
プロセッサの状態をここに保存します。この状態の形式は`kernel_exit`マクロが
期待する形式であり、その構造は[pt_regs](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/include/fork.h#L21)構造体で
記述されています。

`pt_regs`構造体の次のフィールドが`move_to_user_mode`関数で初期化されます。

* `pc` ユーザモードで実行する必要のある関数を指します。`kernel_exit`は
`pc`を`elr_el1`レジスタにコピーします。これにより例外復帰の実行後に
`pc`アドレスに戻ることが保証されます。
* `pstate` このフィールドは`kernel_exit`により`spsr_el1`にコピーされ，
例外復帰の完了後のプロセッサの状態になります。`pstate`フィールドにコピー
される[PSR_MODE_EL0t](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/include/fork.h#L9)定数は
例外復帰がEL0レベルになるような形で用意されています。同じトリックはすでに
レッスン2でEL3からEL1へ切り替える際に行っています。
* `stack` `move_to_user_mode`はユーザスタック用に新しいページを割り当て、
このページのトップへのポインタを`sp`フィールドに設定します。

`task_pt_regs`関数は`pt_regs`領域の位置の計算に使用されます。カレントカーネル
スレッドの初期化を行うこの方法により、初期化終了後に`sp`が確実に`pt_regs`
領域の直前を指すようになります。これは[ret_from_fork](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/entry.S#L188)関数の
中程で起きます。

```
.globl ret_from_fork
ret_from_fork:
    bl    schedule_tail
    cbz   x19, ret_to_user            // not a kernel thread
    mov   x0, x20
    blr   x19
ret_to_user:
    bl disable_irq
    kernel_exit 0
```

気づいたかもしれませんが`ret_from_fork`は変更されています。この変更により、
カーネルスレッドが終了すると実行は`ret_to_user`ラベルに飛びます。そこでは、
割り込みを無効にし、先に用意したプロセッサの状態を使って、通常の例外復帰を
行います。

### ユーザプロセスをフォークする

では`kernel.c`ファイルに戻りましょう。前のセクションで見たように，`kernel_process`が終了すると[user_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/kernel.c#L22) 関数が
ユーザモードで実行されます。この関数は[user_process1](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/kernel.c#L10) 関数を
2つの並列スレッドで実行するために`clone`システムコールを2回呼び出します。
`clone`システムコールには新規ユーザスタックのアドレスを渡す必要があります。
そのため、新規メモリページを2ページ割り当てるために`malloc`システムコールを
呼び出す必要があります。それでは`clone`システムコールのラッピング関数が
どのようなものか見てみましょう。ソースは[`sys.S#L22`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.S#L22)に
あります。

```
.globl call_sys_clone
call_sys_clone:
    /* Save args for the child.  */
    mov    x10, x0                    /*fn*/
    mov    x11, x1                    /*arg*/
    mov    x12, x2                    /*stack*/

    /* Do the system call.  */
    mov    x0, x2                     /* stack  */
    mov    x8, #SYS_CLONE_NUMBER
    svc    0x0

    cmp    x0, #0
    beq    thread_start
    ret

thread_start:
    mov    x29, 0

    /* Pick the function arg and execute.  */
    mov    x0, x11
    blr    x10

    /* We are done, pass the return value through x0.  */
    mov    x8, #SYS_EXIT_NUMBER
    svc    0x0
```

`clone` syscallラッパー関数の設計では`glibc`ライブラリの[対応する関数](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/aarch64/clone.S;h=e0653048259dd9a3d3eb3103ec2ae86acb43ef48;hb=HEAD#l35)の
動作をエミュレートしようと試みました。この関数は以下を行います。

1. レジスタ`x0` – `x3`を保存します。これらのレジスタはsyscallのパラメータを
含んでおり，後にsyscallハンドラによって上書きされます。
2. syscallハンドラを呼び出します。
3. syscallハンドラの戻り値をチェックします。`0`であれば、`thread_start`
ラベルに飛びます。
4. 返り値が0以外の場合、それは新規タスクのPIDです。これはsyscallが終了した
直後にここに戻ったのであり、オリジナルスレッド内で実行していることを意味
します。この場合は呼び出し元に戻るだけです。
5. 元々第1引数として渡されていた関数を新しいスレッドで呼び出します。
6. この関数が終了したら、`exit` syscallを実行します。これは復帰しません。

ご覧の通り，cloneラッパー関数とclone syscallのセマンティクスは異なります。
前者は実行する関数へのポインタを引数として受け取り，後者は呼び出し元に
2回返ります。1回目はオリジナルタスクに，2回目はクローンされたタスクに
返ります。

clone syscallハンドラのソースは[`sys.c#L11`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.c#L11)にあります。
これは非常にシンプルであり、すでにおなじみの`copy_process`関数を呼び出す
だけです。ただし、この関数は前回のレッスンから変更されており、現在ではカーネル
スレッドだけでなく、ユーザースレッドのクローン化もサポートしています。
この関数のソースを以下に示します。

```
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg, unsigned long stack)
{
    preempt_disable();
    struct task_struct *p;

    p = (struct task_struct *) get_free_page();
    if (!p) {
        return -1;
    }

    struct pt_regs *childregs = task_pt_regs(p);
    memzero((unsigned long)childregs, sizeof(struct pt_regs));
    memzero((unsigned long)&p->cpu_context, sizeof(struct cpu_context));

    if (clone_flags & PF_KTHREAD) {
        p->cpu_context.x19 = fn;
        p->cpu_context.x20 = arg;
    } else {
        struct pt_regs * cur_regs = task_pt_regs(current);
        *childregs = *cur_regs;
        childregs->regs[0] = 0;
        childregs->sp = stack + PAGE_SIZE;
        p->stack = stack;
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

新しいカーネルスレッドを作成する場合，この関数は前回のレッスンで説明した
ものとまったく同じ動作をします。そうでない場合，ユーザスレッドを複製する際に
コードの次の部分が実行されます。

```
        struct pt_regs * cur_regs = task_pt_regs(current);
        *childregs = *cur_regs;
        childregs->regs[0] = 0;
        childregs->sp = stack + PAGE_SIZE;
        p->stack = stack;
```

ここで行っていることは、まず、`kernel_entry`マクロで保存されたプロセッサの
状態にアクセスすることです。しかし、なぜ同じ`task_pt_regs`関数を使うことが
できるのかは明らかではありません。この関数はカーネルスタックのトップにある
`pt_regs`領域を返すだけだからです。`pt_regs`がスタック以外のどこか他の場所に
格納されるということはないのでしょうか。その答えは、このコードは`cloce`
syscallが呼び出された後にしか実行できないからです。syscallがトリガされた
時点ではカレントカーネルスタックは空です（ユーザモードに移行した後でも
スタックは空のままにしておきました）。そのため、`pt_regs`は常にカーネル
スタックのトップに格納されます。この規則は後続のすべてのsyscallにも適用
されます。なぜなら、各syscallはユーザモードに戻るまではカーネルスタックを
空のままにしておくからです。

2行目では、現在のプロセッサの状態を新規タスクの状態にコピーしています。
新しい状態の`x0`は`0`に設定します。`x0`は呼び出し元によってsyscallの戻り値と
して解釈されるからです。クローンラッパー関数がどのようにこの値を使って
まだオリジナルスレッドとして実行しているのか，新しいスレッドとして実行して
いるのかを判断しているかを見たばかりです。

次に、新規タスク用の`sp`が新規ユーザスタックページのトップを指すように
設定します。また，タスク終了後にクリーンアップできるようにスタックページ
へのポインタも保存します．

### タスクを実行する

各ユーザタスクは終了したら`exit` syscallを呼び出す必要があります（現在の
実装では`exit`は`clone`ラッパー関数によりが暗黙的に呼び出されています）。
`exit` syscallは[exit_process](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sched.c)関数を呼び出します。この関数はタスクの
解除を担当します。この関数を以下に示します。

```
void exit_process(){
    preempt_disable();
    for (int i = 0; i < NR_TASKS; i++){
        if (task[i] == current) {
            task[i]->state = TASK_ZOMBIE;
            break;
        }
    }
    if (current->stack) {
        free_page(current->stack);
    }
    preempt_enable();
    schedule();
}
```

Linuxの慣習に従い、タスクをすぐに削除するのではなく、その状態を`TASK_ZOMBIE`に
設定します。これにより、このタスクがスケジューラに選択されて実行されるのを
防ぎます。Linuxでは親プロセスが子プロセスに関する情報を子プロセスの終了後も
照会できるようにするためにこの方法が使用されています。

`exit_process`は今や不要となったユーザースタックも削除して、`schedule`を呼び
出します。`schedule`が呼び出されると新しいタスクが選択されます。このシステム
コールが決して戻らないのはそのためです。

### 結論

RPi OSがユーザータスクを管理できるようになったことで、完全なプロセス隔離に
大きく近づきました。しかし、まだ1つ重要なステップが残っています。すべての
ユーザタスクが同じ物理メモリを共有しているため、お互いのデータを簡単に
読み取ることができるのです。次のレッスンでは仮想メモリを導入してこの問題を
解決します。

##### 前ページ

4.5 [プロセスくけジューラ: 演習](../../docs/lesson04/exercises.md)

##### Next Page

5.2 [ユーザプロセスとシステムコール: Linux](../../docs/lesson05/linux.md)
