
# FP/SIMDレジスタの保存

"Procedure Call Standard for the Arm® 64-bit Architecture (AArch64)"の
"6.1.2 SIMD and Floating-Point Registers"によると関数呼び出しでは

- v8-v15の下64ビットがcallee-saved(d8-d15として保存すれば良い？)
- FPSRとFPCRの保存も必要

そこで、以下のようにした

1. cpu_switch_to (sched.S)ではd8-d15, fpsr, fpcrを保存・復元
2. kernel_entry/exit (entry.S)ではv0-v31を保存・復元

## エラー発生

```bash
$ make run
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345123
SYNC_INVALID_EL0_64, ESR: 2000000, address: 80000
QEMU: Terminated
```

エラータイプがSYNC_INVALID_EL0_64、エラー発生アドレスが0x80000（OSイメージのロードアドレス）。
エラーコードは、"ARM® Architecture Reference Manual"の"D10.2.39 ESR_ELx" (p. 2436)によると

- ESR=0x2000000
  - EC = 000000: Unknown reason
  - IL = 1: 32-bit instruction trapped。または、その他の原因（たとえば、An SP alignment fault exception）


## 他の人の回答を調査

1. cpu_contextとfpsimd_contextを分けて定義
2. cpu_switch_toではv0-v31の全128ビットを保存・復元
3. kernel_entry/exitでは保存・復元をしない

## 対応

ABIによればfp_simdレジスタはすべて保存しなくても良さそうなので、
2項をv0-v31ではなくv8-v15とした以外は、そのまま採用した。うまく行ったようだ。

**考察**

```
#define THREAD_CPU_CONTEXT			0 		// offset of cpu_context in task_struct
#define THREAD_FPSIMD_CONTEXT		0x70	// offset of fpsimd_context in task_struct
```

cpu_contextの実際の長さは 8 x 13 = 104 (0x68)だが、THREAD_FPSIMD_CONTEXTは0x70としている。
16バイトのft_simdレジスタを保存するために16バイト境界に合わせて詰め物がされるようだが、
最初の私の実装ではこれを考慮していなかったので、そのあたりが問題だったのではないか。

```
$ make run
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451abcdeabcdeabcdeabcdeabcdeabcdeabcdeabcd
eabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcd
eabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcd
eabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcd
eabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcd
e2345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512345123451234512345123451234512345123451234512345123451234512345
12345123451234512345123451234512abcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabc
deabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabc
deabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeabcdeQEMU: Terminated
```
