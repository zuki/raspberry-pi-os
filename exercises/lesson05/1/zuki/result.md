# タスクがユーザモードで実行されているときに、システムレジスタにアクセス

`get_sysreg()`を作成して以下で実行。結果は同じで`SYNC_ERROR`（同期例外）が発生するが
ほとんどのシステムレジスタのアクセスでは例外クラスが0（`ESR=2000000`, 'EC=0`）となり、
判定ができない。DAIFレジスタへのアクセスでEC=0x18のエラーが発生したので、これを例と
して使用した。

- `kernel.c#user_process()`内で実行
- `user_process1(char *array)`内で実行

```
$ make run
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
Kernel process started. EL 1
User process started
SYNC_ERROR, ESR: 2000000, address: 83164        // spsr_el1, esr_el1など EC = 0b000000
SYNC_ERROR, ESR: 6232d005, address: 83164       // daif: EC = 0b011000
SYNC_ERROR, ESR: 1fe00000, address: 83164       // fpcf: EC = 0b000111 （FP/SIMD無効時のFP/SIMDレジスタへのアクセス）
```

# 同期例外を処理

```asm
el0_sync:                           // 動機例外ハンドラ
	kernel_entry 0
	mrs	x25, esr_el1				// syndromeレジスタを読み込む
	lsr	x24, x25, #ESR_ELx_EC_SHIFT	// 例外クラス（EC）を取り出す
	cmp	x24, #ESR_ELx_EC_SVC64		// 64ビットSVC
	b.eq	el0_svc
    cmp x24, #ESR_ELx_EC_SYS_INSTR  // EC=0b000111: MSR, MRSのトラップ例外
    b.eq    el0_sys_instr
	handle_invalid_entry 0, SYNC_ERROR

el0_sys_instr:
    bl show_trapped_sys_instr       // メッセージ表示
    ldr x22, [sp, #16 * 16]         // 例外発生時のPCが入っている
    add x22, x22, #4                // 例外を発生させた命令をスキップして
    str x22, [sp, #16 * 16]         //   次の命令から実行を再開するように設定
    b ret_to_user                   // ユーザモードへ戻る
```

```
# make run
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
Kernel process started. EL 1
User process started
User process accessed an system register.
12345123451234512345123451234512345123451234512345123451234512345123
```
