## 3.2: Linuxにおける低レベル例外処理

巨大なLinuxカーネルのソースコードがある中で、割り込み処理を担当しているコードを
見つける良い方法は何でしょうか。1つのアイデアを提案します。ベクタテーブルの
ベースアドレスは'vbar_el1'レジスタに格納されるので、'vbar_el1'を検索すれば
ベクタテーブルがどこで初期化されているのかがわかるはずです。実際、検索すると
その使用は数箇所しかなく、その一つはすでにおなじみの[hhead.S](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S)にあります。
このコードは[__primary_switched](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L323)
関数にあります。この関数はMMUがスイッチオンされた後に実行されます。コードは次の
ようになっています。

```
    adr_l  x8, vectors             // 仮想ベクタテーブルアドレスを
    msr    vbar_el1, x8            // VBAR_EL1にロード
```

このコードからベクタテーブルは`vectors`と呼ばれているのが推測でき、[その定義](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L367)
を簡単に見つけることができます。

```
/*
 * 例外ベクタ
 */
    .pushsection ".entry.text", "ax"

    .align    11
ENTRY(vectors)
    kernel_ventry    el1_sync_invalid         // Synchronous EL1t
    kernel_ventry    el1_irq_invalid          // IRQ EL1t
    kernel_ventry    el1_fiq_invalid          // FIQ EL1t
    kernel_ventry    el1_error_invalid        // Error EL1t

    kernel_ventry    el1_sync                 // Synchronous EL1h
    kernel_ventry    el1_irq                  // IRQ EL1h
    kernel_ventry    el1_fiq_invalid          // FIQ EL1h
    kernel_ventry    el1_error_invalid        // Error EL1h

    kernel_ventry    el0_sync                 // Synchronous 64-bit EL0
    kernel_ventry    el0_irq                  // IRQ 64-bit EL0
    kernel_ventry    el0_fiq_invalid          // FIQ 64-bit EL0
    kernel_ventry    el0_error_invalid        // Error 64-bit EL0

#ifdef CONFIG_COMPAT
    kernel_ventry    el0_sync_compat          // Synchronous 32-bit EL0
    kernel_ventry    el0_irq_compat           // IRQ 32-bit EL0
    kernel_ventry    el0_fiq_invalid_compat   // FIQ 32-bit EL0
    kernel_ventry    el0_error_invalid_compat // Error 32-bit EL0
#else
    kernel_ventry    el0_sync_invalid         // Synchronous 32-bit EL0
    kernel_ventry    el0_irq_invalid          // IRQ 32-bit EL0
    kernel_ventry    el0_fiq_invalid          // FIQ 32-bit EL0
    kernel_ventry    el0_error_invalid        // Error 32-bit EL0
#endif
END(vectors)
```

見覚えがありませんか。実際、渡しはこのコードをコピーして少し簡単に下だけです。
[kernel_ventry](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L72)
マクロは、RPi OSで定義されている[ventry](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson03/src/entry.S#L12)と
ほぼ同じです。ただし一つだけ違います。`kernel_ventry`はカーネルのスタック
オーバーフローの発生をチェックする役割も担っていることです。この機能は
`CONFIG_VMAP_STACK`が設定されている場合に有効で、「仮想マップドカーネルスタック」
と呼ばれるカーネル機能の一部です。それについてはここでは詳細を説明しませんが
興味のある方は[この](https://lwn.net/Articles/692208/)記事を読むことを勧めます。

### kernel_entry

[kernel_entry](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L120)
マクロもお馴染みのはずです。このマクロはRPI OSの[対応するマクロ](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson03/src/entry.S#L17)と
まったく同じように使用されます。ただし、オリジナル（Linux）バージョンはより複雑
です。そのコードを以下に示します。

```
	.macro	kernel_entry, el, regsize = 64
	.if	\regsize == 32
	mov	w0, w0				// x0の上位32ビットをゼロクリア
	.endif
	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]

	.if	\el == 0
	mrs	x21, sp_el0
	ldr_this_cpu	tsk, __entry_task, x20	// MDSCR_EL1.SSを確実にクリア
	ldr	x19, [tsk, #TSK_TI_FLAGS]	// スケジューリングの際にデバッグ
	disable_step_tsk x19, x20		// 例外をアンマスクできるように

	mov	x29, xzr			// fpがユーザ空間を指すように
	.else
	add	x21, sp, #S_FRAME_SIZE
	get_thread_info tsk
	/* タスクのもともとのaddr_limitを保存して、USER_DS (TASK_SIZE_64)をセット */
	ldr	x20, [tsk, #TSK_TI_ADDR_LIMIT]
	str	x20, [sp, #S_ORIG_ADDR_LIMIT]
	mov	x20, #TASK_SIZE_64
	str	x20, [tsk, #TSK_TI_ADDR_LIMIT]
	/* ハードウェがすでに0にセットしているので、PSTATE.UAOのリセットは不要 */
	.endif /* \el == 0 */
	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	stp	lr, x21, [sp, #S_LR]

	/*
	 * （後でコールスタックを辿る場合に）例外が発生した
	 * 時点のpt_regs構造体の内容をダンプできるように、
	 * スタックフレームも入れておく
	 */
	.if \el == 0
	stp	xzr, xzr, [sp, #S_STACKFRAME]
	.else
	stp	x29, x22, [sp, #S_STACKFRAME]
	.endif
	add	x29, sp, #S_STACKFRAME

#ifdef CONFIG_ARM64_SW_TTBR0_PAN
	/*
	 * SPSRのTTBR0 PANビットをセットする。例外がEL0で発生した場合は.
	 * アクセスは既に有効だから、TTBR0_EL1の状態をチェックする必要はない。
	 * このビットの意味はARMv8.1のPAN機能とは異なり、ユーザマッピングへの
     * アクセスだけでなく、すべてのTTBR0_EL1アクセスが無効になることに
     * 注意してください。
	 */
alternative_if ARM64_HAS_PAN
	b	1f				// TTBR0 PANをスキップ
alternative_else_nop_endif

	.if	\el != 0
	mrs	x21, ttbr0_el1
	tst	x21, #0xffff << 48		    // 予約されているASIDをチェック
	orr	x23, x23, #PSR_PAN_BIT		// 保存したSPSRのエミュレートPANをセット
	b.eq	1f				        // TTBR0のアクセスは既に無効になっている
	and	x23, x23, #~PSR_PAN_BIT		// 保存したSPSRのエミュレートPANをクリア
	.endif

	__uaccess_ttbr0_disable x21
1:
#endif

	stp	x22, x23, [sp, #S_PC]

	/* デフォルトではシステムコールではない (el0_svcが実際のシステムコールで上書きする) */
	.if	\el == 0
	mov	w21, #NO_SYSCALL
	str	w21, [sp, #S_SYSCALLNO]
	.endif

	/*
	 * sp_el0にカレントスレッド情報をセット
	 */
	.if	\el == 0
	msr	sp_el0, tsk
	.endif

	/*
	 * このマクロの実行後に便利なレジスタ:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	*/
	.endm
```

では、`kernel_entry`マクロを詳細に見ていきましょう。

```
    .macro    kernel_entry, el, regsize = 64
```

このマクロは`el`と`regsize`の2つのパラメータを受け付けます。`el`は例外が
EL0とEL1のいずれで発生したかにより、`0`または`1`になります。`regsize`は
32ビットEL0から来た場合は32、そ例外は64になります。

```
    .if    \regsize == 32
    mov    w0, w0                // zero upper 32 bits of x0
    .endif
```

32ビットモードでは、32ビットの汎用レジスタ（`x0`ではなく`w0`）を使用します。
`w0`はアーキテクチャ上、`x0`の下位にマッピングされています。上のコード
スニペットでは`w0`を自分自身に書き込むことで`x0`レジスタの上位32ビットを
ゼロにしています。

```
    stp    x0, x1, [sp, #16 * 0]
    stp    x2, x3, [sp, #16 * 1]
    stp    x4, x5, [sp, #16 * 2]
    stp    x6, x7, [sp, #16 * 3]
    stp    x8, x9, [sp, #16 * 4]
    stp    x10, x11, [sp, #16 * 5]
    stp    x12, x13, [sp, #16 * 6]
    stp    x14, x15, [sp, #16 * 7]
    stp    x16, x17, [sp, #16 * 8]
    stp    x18, x19, [sp, #16 * 9]
    stp    x20, x21, [sp, #16 * 10]
    stp    x22, x23, [sp, #16 * 11]
    stp    x24, x25, [sp, #16 * 12]
    stp    x26, x27, [sp, #16 * 13]
    stp    x28, x29, [sp, #16 * 14]
```

この部分はすべての汎用レジスタをスタックに保存しています。なお、スタックポインタは
すでに保存すべきすべてに合うように[kernel_ventry](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L74)で
調整されています。レジスタを保存する順序は重要です。なぜなら、Linuxには特別な構造体[pt_regs](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/ptrace.h#L119)が
あり、後で例外ハンドラが保存されたレジスタにアクセスする際に使用されるからです。
見ればわかるように、この構造体には汎用レジスタだけでなく他の情報も含まれており、
これらの情報は主に`kernel_entry`マクロの後の方で入力されます。`pt_regs`構造体は
覚えて置いてください。なぜなら、次の数回のレッスンで同様の構造体を実装して使用
する予定だからです。

```
    .if    \el == 0
    mrs    x21, sp_el0
```

`x21`には現在、アボートしたスタックポインタが含まれています。Linuxのタスクは
ユーザモード用とカーネルモード用の2つの異なるスタックを使用していることに注意して
ください。ユーザモードの場合は、`sp_el0`レジスタを使って、例外が発生した瞬間の
スタックポインタの値を知ることができます。コンテキストスイッチの際にスタック
ポインタを交換する必要があるため、この行は非常に重要です。詳しくは次のレッスンで
説明します。

```
    ldr_this_cpu    tsk, __entry_task, x20    // Ensure MDSCR_EL1.SS is clear,
    ldr    x19, [tsk, #TSK_TI_FLAGS]    // since we can unmask debug
    disable_step_tsk x19, x20        // exceptions when scheduling.
```

`MDSCR_EL1.SS`ビットは，"ソフトウェアステップ例外 "を有効にするためのビットです。
このビットがセットされ、デバッグ例外がマスクされていない場合、任意の命令を実行
後に例外が生成されます。これはデバッガでよく使われます。ユーザモードからの例外
を処理する場合、まずカレントタスクに[TIF_SINGLESTEP](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/thread_info.h#L93)
フラグが設定されているか確認する必要があります。設定されている場合、これはタスクが
デバッガ下で実行されていることを示しており、`MDSCR_EL1.SS`ビットをクリアする
必要があります。このコードで理解すべき重要な点は、カレントタスクに関する情報を
どのように取得するかです。Linuxでは、プロセスやスレッド（今後、どちらも単に「タスク」と呼びます）には[task_struct](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L519)が関連付けられています。この構造体にはタスクに関するすべての
メタデータ情報が含まれています。`arm64`アーキテクチャでは`task_struct`は
[thread_info](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/thread_info.h#L39)と
呼ばれる別の構造体を内蔵しており、`task_struct`へのポインタは常に`thread_info`への
ポインタとして使用することができます。`thread_info`にはフラグと`entry.S`が直接
アクセスする必要のある低レベルの値が格納されます。

```
    mov    x29, xzr            // fp pointed to user-space
```

`x29`は汎用レジスタですが、通常は特別な意味を持っています。「フレームポインタ」と
して使用されるからです。ここでは、その目的について少し説明したいと思います。

関数がコンパイルされると、最初の数命令は通常、古いフレームポインタとリンクレジスタの
値をスタックに格納する役割を果たします（簡単に説明しておくと、`x30`はリンクレジスタ
と呼ばれ、`ret`命令が使用する「リターンアドレス」を格納します）。次に、その関数の
すべてのローカル変数を格納できるように新しいスタックフレームが割り当てられ、
フレームポインタレジスタはこのフレームの底を指すように設定されます。関数が
ローカル変数にアクセスする必要がある場合は、ハードコードされたオフセットをフレーム
ポインタに加えるだけです。ここで、エラーが発生し、スタックトレースを作成する必要が
あるとします。現在のフレームポインタを使用してスタックにあるすべてのローカル変数を
見つけることができ、リンクレジスタを使用して呼び出し元の正確な位置を知ることが
できます。さらに、古いフレームポインタとリンクレジスタの値が常にスタックフレームの
先頭に保存されているので、これらの値はそこから読み込むだけです。呼び出し元の
フレームポインタを取得すれば、呼び出し元のすべてのローカル変数にもアクセスできます。
このプロセスはスタックの先頭に到達するまで再帰的に繰り返され、「スタック巻き戻し」と
呼ばれます。同様のアルゴリズムは[ptrace](http://man7.org/linux/man-pages/man2/ptrace.2.html) システムコールでも使用されています。

では、`kernel_entry`マクロの話に戻りますが、EL0からの例外を捕捉後に、なぜ`x29`
レジスタをクリアする必要があるのかは明らかでしょう。それは、Linuxではタスクは
ユーザモードとカーネルモードで異なるスタックを使用するため、共通のスタック
トレースを持つことに意味がないからです。

```
    .else
    add    x21, sp, #S_FRAME_SIZE
```

ここではelse節に入っていますが、これはEL1からの例外を処理する場合にのみこのコードが
有効であることを意味します。この場合、古いスタックを再利用しており、上のコード
スニペットでは、後で使用するために元の`sp`値を`x21`レジスタに保存しているだけです。

```
    /* Save the task's original addr_limit and set USER_DS (TASK_SIZE_64) */
    ldr    x20, [tsk, #TSK_TI_ADDR_LIMIT]
    str    x20, [sp, #S_ORIG_ADDR_LIMIT]
    mov    x20, #TASK_SIZE_64
    str    x20, [tsk, #TSK_TI_ADDR_LIMIT]
```

タスクアドレス制限には使用可能な仮想アドレスの最大値を指定します。ユーザ
プロセスが32ビットモードで動作する場合、この制限値は2^32です。64ビットカーネルの
場合は、これよりも大きくなり、通常は2^48です。例外が32ビットのEL1で発生した場合は、
タスクアドレス制限を[TASK_SIZE_64](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/memory.h#L80)に
変更する必要があります。また、元のアドレス制限を保存する必要があります。実行を
ユーザーモードに戻す前に元のアドレス制限を復元する必要があるためです。

```
    mrs    x22, elr_el1
    mrs    x23, spsr_el1
```

`elr_el1`と`spsr_el1`は、例外処理を開始する前にスタックに保存しておく必要が
あります。RPI OSではまだそれを行っていません。今のところ、常に例外が発生した
時と同じ場所に戻るからです。しかし、例外処理中にコンテキストスイッチを行う
必要がある場合はどうでしょうか。このシナリオについては、次のレッスンで詳しく
説明します。

```
    stp    lr, x21, [sp, #S_LR]
```

リンクレジスタとフレームポインタレジスタをスタックに保存します。既に見たように、
フレームポインタは例外がEL0とEL1のどちらで発生したかによって計算方法が異なり、
その計算結果はすでに`x21`レジスタに保存されています。

```
    /*
     * In order to be able to dump the contents of struct pt_regs at the
     * time the exception was taken (in case we attempt to walk the call
     * stack later), chain it together with the stack frames.
     */
    .if \el == 0
    stp    xzr, xzr, [sp, #S_STACKFRAME]
    .else
    stp    x29, x22, [sp, #S_STACKFRAME]
    .endif
    add    x29, sp, #S_STACKFRAME
```

ここでは`pt_regs`構造体の[stackframe](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/ptrace.h#L140)
プロパティにデータを設定しています。このプロパティにはリンクレジスタとフレーム
ポインタも含まれていますが、今回は`lr`の代わりに`elr_el1`値（現在は`x22`に入って
います）が使われています。`stackframe`はスタックの巻き戻しのためだけに使われます。

```
#ifdef CONFIG_ARM64_SW_TTBR0_PAN
alternative_if ARM64_HAS_PAN
    b    1f                // skip TTBR0 PAN
alternative_else_nop_endif

    .if    \el != 0
    mrs    x21, ttbr0_el1
    tst    x21, #0xffff << 48        // Check for the reserved ASID
    orr    x23, x23, #PSR_PAN_BIT        // Set the emulated PAN in the saved SPSR
    b.eq    1f                // TTBR0 access already disabled
    and    x23, x23, #~PSR_PAN_BIT        // Clear the emulated PAN in the saved SPSR
    .endif

    __uaccess_ttbr0_disable x21
1:
#endif
```

`CONFIG_ARM64_SW_TTBR0_PAN`パラメタは、カーネルがユーザー空間のメモリに直接
アクセスすることを防ぎます。これがどのような場合に役立つのか気になる方は
[この](https://kernsec.org/wiki/index.php/Exploit_Methods/Userspace_data_usage)
記事をご覧ください。このようなセキュリティ機能は今回の議論にはまったくの
範囲外であるため、ここではこの仕組みについての詳細な説明も省略します。

```
    stp    x22, x23, [sp, #S_PC]
```

ここでは`elr_el1`と`spsr_el1`をスタックに保存しています。

```
    /* Not in a syscall by default (el0_svc overwrites for real syscall) */
    .if    \el == 0
    mov    w21, #NO_SYSCALL
    str    w21, [sp, #S_SYSCALLNO]
    .endif
```

`pt_regs`構造体には、現在の例外がシステムコールであるかを示すフィールドが
あります。デフォルトでは、システムコールではないと仮定します。システムコールの
仕組みに関する詳しい説明はレッスン 5までお待ちください。

```
    /*
     * Set sp_el0 to current thread_info.
     */
    .if    \el == 0
    msr    sp_el0, tsk
    .endif
```

タスクがカーネルモードで実行され場合は`sp_el0`は必要ありません。`sp_el0`の値は
あらかじめスタックに保存されているので、`kernel_exit`マクロで簡単に復元する
ことができます。この時点から`sp_el0`は現在の[task_struct](https://github.com/torvalds/linux/blob/v4.14/include/linux/sched.h#L519)に素早くアクセスするためのポインタを
保持することになります。

### el1_irq

次に調査するのはEL1からのIRQを処理するハンドラです。[ベクタテーブル](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L374)
から、そのハンドラが`el1_irq`と呼ばれ、[`entry.S#L562`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L562)で
定義されていることを簡単に見つけることができます。それでは、コードを一行ずつ
見ていきましょう。

```
el1_irq:
    kernel_entry 1
    enable_dbg
#ifdef CONFIG_TRACE_IRQFLAGS
    bl    trace_hardirqs_off
#endif

    irq_handler

#ifdef CONFIG_PREEMPT
    ldr    w24, [tsk, #TSK_TI_PREEMPT]    // get preempt count
    cbnz    w24, 1f                // preempt count != 0
    ldr    x0, [tsk, #TSK_TI_FLAGS]    // get flags
    tbz    x0, #TIF_NEED_RESCHED, 1f    // needs rescheduling?
    bl    el1_preempt
1:
#endif
#ifdef CONFIG_TRACE_IRQFLAGS
    bl    trace_hardirqs_on
#endif
    kernel_exit 1
ENDPROC(el1_irq)
```

この関数では次のことが行われます。

* `kernel_entry`マクロと`kernel_exit`マクロが呼び出され、プロセッサの状態を
保存・復元します。最初のパラメータは例外がEL1で発生したものであることを示します。
* `enable_dbg`マクロを呼び出してデバッグ割り込みのマスクを外します。この時点で
それを行うことは安全です。プロセッサの状態はすでに保存されており、仮に割り込み
ハンドラの途中でデバッグ例外が発生しても、正しく処理されるためです。そもそも、
なぜ割り込み処理中にデバッグ例外のマスクを外す必要があるのか疑問に思う方は
[この](https://github.com/torvalds/linux/commit/2a2830703a2371b47f7b50b1d35cb15dc0e2b717)
コミットメッセージをお読みください。
* `#ifdef CONFIG_TRACE_IRQFLAGS`ブロック内のコードは割込みのトレースを行っています。
割り込みの開始と終了の2つのイベントを記録します。
* `#ifdef CONFIG_PREEMPT`ブロック内のコードはカレントタスクのフラグにアクセスして
スケジューラを呼び出す必要があるかチェックします。このコードについては次の
レッスンで詳しく調べます。
* `irq_handler` - これが実際に割り込み処理が行われる場所です。

[irq_handler](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/entry.S#L351)はマクロで、次のように定義されています。

```
    .macro    irq_handler
    ldr_l    x1, handle_arch_irq
    mov    x0, sp
    irq_stack_entry
    blr    x1
    irq_stack_exit
    .endm
```

コードからわかるように、`irq_handler`は[handle_arch_irq](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/irq.c#L44)
関数を実行します。この関数は「irqスタック」と呼ばれる特別なスタックで実行されます。
なぜ、別のスタックに切り替える必要があるのでしょうか。たとえば、RPI OSではこのような
ことはしていません。まあ、おそらく必要ないのですが、これがないと割り込みがタスク
スタックを使って処理されることになりますが、割り込みハンドラのためにスタックが
どれだけ残っているかはわからないからです。

次に、[handle_arch_irq](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/irq.c#L44)を
調べる必要があります。これは関数ではなく、変数のようです。これは[set_handle_irq](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/irq.c#L46)
関数で設定されています。しかし、誰がこれを設定しているのでしょうか。また、この
地点に到達後、この割り込みはどうなるのでしょうか。その答えは、このレッスンの
次の章で解明します。

### 結論

結論として、これまでに低レベル割り込み処理コードを調査し、ベクタテーブルから `handle_arch_irq`までの割り込みのすべての経路を追跡してきました。ここは
割り込みがアーキテクチャ固有のコードを離れ、ドライバコードによる処理が開始される
地点です。次の章ではドライバのソースコードを通じてタイマ割込みの経路を追跡する
ことを目標とします。

##### 前ページ

3.1 [割り込みハンドラ: RPi OS](../../../ja/lesson03/rpi-os.md)

##### 次ページ

3.3 [割り込みハンドラ: 割り込みコントローラ](../../../ja/lesson03/linux/interrupt_controllers.md)
