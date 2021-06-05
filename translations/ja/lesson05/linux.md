## 5.2: ユーザプロセスとシステムコール

この章は短いものにします。その理由は、syscallの実装はLinuxからRPi OSに
ほぼ正確にコピーしたので多くの説明を必要としないからです。しかし、それでも
Linuxのソースコードを見ることで、特定のsyscall機能がどこでどのように実装されて
いるかを確認できるようにしたいと思います。

### 最初のユーザプロセスを作成する

これから取り組む最初の質問は、最初のユーザープロセスがどのように作成されて
いるかです。その答えを探すのに良い場所は[start_kernel](https://github.com/torvalds/linux/blob/v4.14/init/main.c#L509)
関数です。前に見たように、これは、カーネルブートプロセスの初期に呼び出される
アーキテクチャに依存しない最初の関数です。ここはカーネルの初期化が始まる
場所でり、カーネルの初期化の中で最初のユーザプロセスを実行するのは理に
かなっていると考えられます。

実際、`start_kernel`のロジックをたどっていくとすぐに、次のようなコードを
持つk[kernel_init](https://github.com/torvalds/linux/blob/v4.14/init/main.c#L989) 関数が見つかります。

```
    if (!try_to_run_init_process("/sbin/init") ||
        !try_to_run_init_process("/etc/init") ||
        !try_to_run_init_process("/bin/init") ||
        !try_to_run_init_process("/bin/sh"))
        return 0;
```

これはまさに私たちが探しているもののようです。このコードから、Linuxカーネルが
`init`ユーザプログラムを正解にどこから、どの順番で探すのかを推測できます。`try_to_run_init_process`は[do_execve](https://github.com/torvalds/linux/blob/v4.14/fs/exec.c#L1837)
関数を実行します。この関数は[execve](http://man7.org/linux/man-pages/man2/execve.2.html)
システムコールの処理も担当しています。このシステムコールはバイナリ実行
ファイルを読み込んで、カレントプロセス内で実行します。

`execve`システムコールのロジックについてはレッスン9で詳しく説明しますが、
今のところは、このシステムコールが行う最も重要な作業は、実行ファイルの解析と
その内容のメモリへのロードであると述べておくだけで十分でしょう。これは
[load_elf_binary](https://github.com/torvalds/linux/blob/v4.14/fs/binfmt_elf.c#L679)関数で行われます（ここで実行ファイルは[ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)フォーマットであると想定しています。
このフォーマットは最も一般的なものですがゆういつの選択肢ではありません）。
`load_elf_binary`メソッドの最後（[`binfmt_elf.c#L1149`](https://github.com/torvalds/linux/blob/v4.14/fs/binfmt_elf.c#L1149)にあります）には、
アーキテクチャ固有の[start_thread](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/processor.h#L119)関数の呼び出しもあります。私は
これをRPi OSの`move_to_user_mode`ルーチンのプロトタイプとして使用しました。
これは私たちが最も注目するコードです。これがそのコードです。

```
static inline void start_thread_common(struct pt_regs *regs, unsigned long pc)
{
    memset(regs, 0, sizeof(*regs));
    forget_syscall(regs);
    regs->pc = pc;
}

static inline void start_thread(struct pt_regs *regs, unsigned long pc,
                unsigned long sp)
{
    start_thread_common(regs, pc);
    regs->pstate = PSR_MODE_EL0t;
    regs->sp = sp;
}
```

`start_thread`が実行されるまで、カレントプロセスはカーネルモードで動作して
います。`start_thread`には現在のカレントプロセスの`pt_regs`構造体にアクセス
する権利が与えられており、保存する`pstate`、`sp`、`pc`の各フィールドを設定
するために使用されます。このロジックはRPi OSの`move_to_user_mode`関数と
まったく同じなのでもう一度繰り返したくはありません。重要なことは
`start_thread`は、`kernel_exit`マクロが最終的にプロセスをユーザモードに
移行させるような方法でプロセッサの保存状態を準備することです。

###  Linuxのシステムコール

システムコールの主な仕組みがLinuxとRPi OSで全く同じであることは驚くことでは
ありません。ここでは既におなじみの[clone](http://man7.org/linux/man-pages/man2/clone.2.html)
システムコールを使って，このメカニズムの詳細を理解しましょう。[glibcのclone ラッパー関数](https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/aarch64/clone.S;h=e0653048259dd9a3d3eb3103ec2ae86acb43ef48;hb=HEAD#l35)
から調査を始めるのがよいでしょう。この関数はRPi OSの[call_sys_clone](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson05/src/sys.S#L22)
関数と全く同じように動作しますが，前者の関数は引数のサニティチェックを行う
ことと例外を適切に処理することが異なります。理解し覚えておくことが重要な
ことは、どちらの場合も`svc`命令を使って同期例外を発生させていること、
システムコール番号は`x8`レジスタを使って渡されること、すべての引数は
`x0 - x7`レジスタで渡されることです。

次に、`clone`システムコールがどのように定義されているかを見てみましょう。
定義は[`fork.c#L2153`](https://github.com/torvalds/linux/blob/v4.14/kernel/fork.c#L2153)にあり、以下のようになっています。

```
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
         int __user *, parent_tidptr,
         int __user *, child_tidptr,
         unsigned long, tls)
{
    return _do_fork(clone_flags, newsp, 0, parent_tidptr, child_tidptr, tls);
}
```

[SYSCALL_DEFINE5](https://github.com/torvalds/linux/blob/v4.14/include/trace/syscall.h#L25)
マクロの名前に数字の5が入っているのは、5つのパラメータを持つシステムコールを
定義することを示しています。このマクロは新規[syscall_metadata](https://github.com/torvalds/linux/blob/v4.14/include/trace/syscall.h#L25)
構造体を割り当て、データを設定して`sys_<syscall name>`関数を作成します。
たとえば，`clone`システムコールの場合は`sys_clone`関数が定義されます。
この関数は低レベルアーキテクチャコードから呼び出される実際のシステムコール
ハンドラです。

システムコールを実行する際、カーネルがシステムコール番号からシステムコール
ハンドラを見つける方法が必要です。これを実現する最も簡単な方法は、システム
コールハンドラへのポインタの配列を作成し、システムコール番号をこの配列の
インデックスとして使用することです。これはRPi OSで採用した方法であり、
Linuxカーネルでも全く同じ方法が採用されています。システムコールハンドラへの
ポインタの配列は[sys_call_table](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/sys.c#L62)と呼ばれ、次のように定義されています。

```
void * const sys_call_table[__NR_syscalls] __aligned(4096) = {
    [0 ... __NR_syscalls - 1] = sys_ni_syscall,
#include <asm/unistd.h>
};
```

すべてのシスコールは初期状態では`sys_ni_syscall`関数を指すように割り当て
られています（ここで"ni"は"non-existent"を意味します）。番号0のシスコールと
カレントアーキテクチャで定義されていないすべてのシスコールにはこのハンドラが
使用されます。`sys_call_table`配列に含まれるその他のすべてのシスコール
ハンドラは[asm/unistd.h](https://github.com/torvalds/linux/blob/v4.14/include/uapi/asm-generic/unistd.h)
ヘッダファイルで再定義されています。ご覧の通り、このファイルはシステムコール
番号とシステムコールハンドラ関数のマッピングを提供しているだけです。

### 低レベルシステムコール処理コード

`sys_call_table`がどのように作成され、データが登録されているかを見てきました。
今度は、低レベルのシステムコール処理コードがこれをどのように使用するかを
調べてみましょう。繰り返しになりますが、基本的なメカニズムはRPi OSと
ほとんど変わりません。

すべてのシステムコールは同期例外であり、例外ハンドラはすべて例外ベクタ
テーブル（お気に入りの[vectors](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L367)
配列です）で定義されていることは知っています。そこで、私たちが注目すべき
ハンドラはEL0で発生する同期例外を処理するものです。これらのことから、
正しいハンドラの発見は簡単です。それは[el0_sync](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L598)と呼ばれるもので、以下の
ような形をしています。

```
el0_sync:
    kernel_entry 0
    mrs    x25, esr_el1            // read the syndrome register
    lsr    x24, x25, #ESR_ELx_EC_SHIFT    // exception class
    cmp    x24, #ESR_ELx_EC_SVC64        // SVC in 64-bit state
    b.eq    el0_svc
    cmp    x24, #ESR_ELx_EC_DABT_LOW    // data abort in EL0
    b.eq    el0_da
    cmp    x24, #ESR_ELx_EC_IABT_LOW    // instruction abort in EL0
    b.eq    el0_ia
    cmp    x24, #ESR_ELx_EC_FP_ASIMD    // FP/ASIMD access
    b.eq    el0_fpsimd_acc
    cmp    x24, #ESR_ELx_EC_FP_EXC64    // FP/ASIMD exception
    b.eq    el0_fpsimd_exc
    cmp    x24, #ESR_ELx_EC_SYS64        // configurable trap
    b.eq    el0_sys
    cmp    x24, #ESR_ELx_EC_SP_ALIGN    // stack alignment exception
    b.eq    el0_sp_pc
    cmp    x24, #ESR_ELx_EC_PC_ALIGN    // pc alignment exception
    b.eq    el0_sp_pc
    cmp    x24, #ESR_ELx_EC_UNKNOWN    // unknown exception in EL0
    b.eq    el0_undef
    cmp    x24, #ESR_ELx_EC_BREAKPT_LOW    // debug exception in EL0
    b.ge    el0_dbg
    b    el0_inv
```

ここでは`esr_el1`（exception syndromeレジスタ）を使って、現在の例外が
システムコールであるか否かを判定します。もしそうであれば、[el0_svc](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L837)
関数を呼び出します。この関数を以下に示します。

```
el0_svc:
    adrp    stbl, sys_call_table        // load syscall table pointer
    mov    wscno, w8            // syscall number in w8
    mov    wsc_nr, #__NR_syscalls
el0_svc_naked:                    // compat entry point
    stp    x0, xscno, [sp, #S_ORIG_X0]    // save the original x0 and syscall number
    enable_dbg_and_irq
    ct_user_exit 1

    ldr    x16, [tsk, #TSK_TI_FLAGS]    // check for syscall hooks
    tst    x16, #_TIF_SYSCALL_WORK
    b.ne    __sys_trace
    cmp     wscno, wsc_nr            // check upper syscall limit
    b.hs    ni_sys
    ldr    x16, [stbl, xscno, lsl #3]    // address in the syscall table
    blr    x16                // call sys_* routine
    b    ret_fast_syscall
ni_sys:
    mov    x0, sp
    bl    do_ni_syscall
    b    ret_fast_syscall
ENDPROC(el0_svc)
```

では、一行ずつ見ていきましょう。

```
el0_svc:
    adrp    stbl, sys_call_table        // load syscall table pointer
    mov    wscno, w8            // syscall number in w8
    mov    wsc_nr, #__NR_syscalls
```

最初の3行は`stbl`、`wscno`、`wsc_nr`という変数を初期化していますが、
これらは単なるレジスタのエイリアスです。`stbl`はシステムコールテーブルの
アドレスを、`wsc_nr`はシステムコールの総数を、`wscno`は`w8`レジスタにある
現在のシスコール番号を表しています。

```
    stp    x0, xscno, [sp, #S_ORIG_X0]    // save the original x0 and syscall number
```

RPi OSのシステムコールに関する説明を覚えているかもしれませんが、`pt_regs`
領域にある`x0`はシステムコールが終了すると上書きされます。`x0`レジスタの
元の値が必要な場合は、`pt_regs`構造体の別のフィールドに保存します。同様に、
システムコール番号も`pt_regs`に保存します。

```
    enable_dbg_and_irq
```

割り込みとデバッグ例外を有効にします。

```
    ct_user_exit 1
```

ユーザモードからカーネルモードへ切り替えるというイベントを記録します。

```
    ldr    x16, [tsk, #TSK_TI_FLAGS]    // check for syscall hooks
    tst    x16, #_TIF_SYSCALL_WORK
    b.ne    __sys_trace

```

カレントタスクがシステムコールトレーサの下で実行されている場合は
`_TIF_SYSCALL_WORK`フラグが設定されているはずです。その場合は`_sys_trace`
関数を呼び出します。ここでは、一般的なケースだけに焦点を当てているので、
この関数は省略します。

```
    cmp     wscno, wsc_nr            // check upper syscall limit
    b.hs    ni_sys
```

現在のシステムコール番号がシステムコールの総数よりも大きい場合は、
エラーをユーザに返します。

```
    ldr    x16, [stbl, xscno, lsl #3]    // address in the syscall table
    blr    x16                // call sys_* routine
    b    ret_fast_syscall
```

システムコール番号をシステムコールテーブル配列のインデックスとして使用して
ハンドラを見つけます。見つけたらハンドラのアドレスを`x16`レジスタにロードして
実行します。最後に`ret_fast_syscall`関数を呼び出します。

```
ret_fast_syscall:
    disable_irq                // disable interrupts
    str    x0, [sp, #S_X0]            // returned x0
    ldr    x1, [tsk, #TSK_TI_FLAGS]    // re-check for syscall tracing
    and    x2, x1, #_TIF_SYSCALL_WORK
    cbnz    x2, ret_fast_syscall_trace
    and    x2, x1, #_TIF_WORK_MASK
    cbnz    x2, work_pending
    enable_step_tsk x1, x2
    kernel_exit 0
```

ここで重要なのは、割り込みを禁止している最初の行と、`kernel_exit`を呼び
出している最後の行です。それ以外はすべて特殊なケースの処理に関連するものです。
つまり、お察しの通り、ここがシステムコールが実際に終了し、実行がユーザ
プロセスに移される場所です。

### 結論

システムコールの生成と処理の家庭を説明してきました。このプロセスは比較的
単純ですが、OSにとっては非常に重要です。なぜなら、カーネルがAPIを設定する
ことを可能とし、このAPIがユーザプログラムとカーネル間の唯一の通信手段で
あることを保証するからです。

##### 前ページ

5.1 [ユーザプロセスとシステムコール: RPi OS](../../ja/lesson05/rpi-os.md)

##### 次ページ

5.3 [ユーザプロセスとシステムコール: 演習](../../ja/lesson05/exercises.md)
