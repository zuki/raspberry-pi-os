## 2.2: プロセッサの初期化 (Linux)

私たちのLinuxカーネルの探究は、`arm64`アーキテクチャのエントリーポイントである
[stext](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L116)関数で
止まっていました。今回は、もう少し深く掘り下げて、今回と前回のレッスンで
すでに実装したコードとの類似点を探してみましょう。

この章は少し退屈に思うかもしれません。さまざまなARMシステムレジスタと
それらがLinuxカーネルでどのように使用されているかに関する説明が中心だから
です。しかし、私は以下の理由からこの章が非常に重要だと考えます。

1. ハードウェアがソフトウェアに提供するインターフェースを理解する必要が
あります。このインターフェースを知っているだけで、多くの場合、特定のカーネル
機能がどのように実装されているか、この機能を実装するためにソフトウェアと
ハードウェアがどのように協力してしているかを分析することができます。
2. システムレジスタのさまざまなオプションは、通常、さまざまなハードウェア
機能の有効化/無効化に関連しています。ARMプロセッサがどのようなシステム
レジスタを持っているかを知れば、そのプロセッサがどのような機能をサポート
しているかがわかります。

それでは、`stext`機能の調査を再開しましょう。

```
ENTRY(stext)
    bl    preserve_boot_args
    bl    el2_setup            // EL1に移行する, w0=cpu_boot_mode
    adrp    x23, __PHYS_OFFSET
    and    x23, x23, MIN_KIMG_ALIGN - 1    // KASLRオフセット、デフォルトは0
    bl    set_cpu_boot_mode_flag
    bl    __create_page_tables
    /*
     * 以下はCPUの設定コードを呼び出す。詳細はarch/arm64/mm/proc.Sを
     * 参照のこと。
     * 復帰時には、CPUはMMUを有効にする準備ができており、
     * TCRが設定されている。
     */
    bl    __cpu_setup            // プロセッサを初期化する
    b    __primary_switch
ENDPROC(stext)
```

### preserve_boot_args

[preserve_boot_args](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L136)関数は
ブートローダからカーネルに渡されたパラメータを保存します。

```
preserve_boot_args:
    mov    x21, x0                // x21=FDT

    adr_l    x0, boot_args        // カーネルエントリ時の
    stp    x21, x1, [x0]          // x0 .. x3の内容を記録する
    stp    x2, x3, [x0, #16]

    dmb    sy                     // MMUオフのデータキャッシュを
                                  // 破棄する前に必要

    mov    x1, #0x20              // 4 x 8 bytes
    b    __inval_dcache_area      // tail call
ENDPROC(preserve_boot_args)
```

[カーネルブートプロトコル](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt#L150)によるとパラメータはレジスタ`x0 - x3`
によりカーネルに渡されます。`x0`にはデバイスツリーブロブ（`.dtb`）のシステム
RAMにおける物理アドレスが格納されます。`x1 - x3`は将来のために予約されて
います。この関数が行っていることはレジスタ`x0 - x3`の内容を[boot_args](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/setup.c#L93)
配列にコピーし、データキャッシュから対応するキャッシュラインを[破棄する](https://developer.arm.com/docs/den0024/latest/caches/cache-maintenance)
ことです。マルチプロセッサシステムにおけるキャッシュメンテナンスについては、
それ自体が大きなテーマであるため、今回は省略します。このテーマに興味のある方
には「ARMプログラマガイド」の[Caches](https://developer.arm.com/docs/den0024/latest/caches)と [Multi-core processors](https://developer.arm.com/docs/den0024/latest/multi-core-processors)の章を読むことを勧めます。

### el2_setup

[arm64boot protocol](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt#L159)によると
カーネルはEL1とEL2のいずれかで起動することができます。後者の場合、カーネルは
仮想化拡張機能にアクセスでき、ホストOSとして動作することができます。幸運にも
EL2で起動できた場合は、[el2_setup](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L386)
関数が呼び出されます。この関数はEL2しかアクセスできないさまざまなパラメータを
設定し、EL1に移行する役割を担っています。以下、この関数をいくつかの部分に
分けて、1つ1つ説明していきます。

```
    msr    SPsel, #1            // SP_EL{1,2}を使用する
```

EL1とEL2で専用のスタックポインタを使用します。EL0のスタックポインタを再利用
する別の選択肢もあります。

```
    mrs    x0, CurrentEL
    cmp    x0, #CurrentEL_EL2
    b.eq    1f
```

現在のELがEL2の場合のみラベル`1`に分岐します。そうでない場合はEL2の設定が
できないので、この関数にできることはそれほどありません。

```
    mrs    x0, sctlr_el1
CPU_BE(    orr    x0, x0, #(3 << 24)    )    // EL1のEEビットとE0Eビットをセット
CPU_LE(    bic    x0, x0, #(3 << 24)    )    // EL1のEEビットとE0Eビットをクリア
    msr    sctlr_el1, x0
    mov    w0, #BOOT_CPU_MODE_EL1            // このCPUはEL1でブート
    isb
    ret
```

EL1で実行することになった場合は、CPUが「ビッグエンディアン」と
「リトルエンディアン」のいずれかで動作するように[CPU_BIG_ENDIAN](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/Kconfig#L612)
構成設定値を設定して`sctlr_el1`レジスタを更新します。そして、`el2_setup`関数を
終了して、[BOOT_CPU_MODE_EL1](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/virt.h#L55)定数を返します。
[ARM64の関数呼び出し規約](http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf)に従い、返り値は`x0`レジスタ(ここでは`w0`。`w0`レジスタは、`x0`の最初の32ビットと
考えることができます)に入れる必要があります。

```
1:    mrs    x0, sctlr_el2
CPU_BE(    orr    x0, x0, #(1 << 25)    )    // EL2のEEビットをセット
CPU_LE(    bic    x0, x0, #(1 << 25)    )    // EL2のEEビットをクリア
    msr    sctlr_el2, x0
```

EL2で起動した場合は、EL2用に同じような設定をします（今回は`sctlr_el1`ではなく、`sctlr_el2`レジスタを使用していることに注意してください）。

```
#ifdef CONFIG_ARM64_VHE
    /*
     * VHEがあるかチェック。EL2設定の残りの部分において
     * x2が非ゼロはVHEがあり、カーネルはEL2で実行しようと
     * していることを示す。
     */
    mrs    x2, id_aa64mmfr1_el1     // ID_AA64MMFR1_EL1のVH: [11:8]ビットを
    ubfx    x2, x2, #8, #4          // 抽出。VHEをサポートしていれば0x1
#else
    mov    x2, xzr
#endif
```

[ARM64_VHE](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/Kconfig#L926)
構成変数で[VHE （仮想化ホスト拡張）](https://developer.arm.com/products/architecture/a-profile/docs/100942/latest/aarch64-virtualization)が
有効になっており、ホストマシンがそれをサポートしている場合、`x2`を0以外の
値で更新します。`x2`は同じ関数で後ほど`VHE`が有効になっているか確認する
時に使用されます。

```
    mov    x0, #HCR_RW            // 64-bit EL1
    cbz    x2, set_hcr
    orr    x0, x0, #HCR_TGE       // ホスト拡張を有効化
    orr    x0, x0, #HCR_E2H
set_hcr:
    msr    hcr_el2, x0
    isb
```

ここでは`hcr_el2`レジスタを設定しています。RPi OSでは同じレジスタを使って
EL1に64ビット実行モードを設定しました。それは上のコードサンプルの1行目で
行っていることと全く同じです。また、`x2 != 0`、つまりVHEが使用可能かつ
カーネルがそれを使用するように設定されている場合、`hcr_el2`を使って
VHEも有効にしています。

```
    /*
     * セキュアでないEL1とEL0に物理的なタイマとカウンタへのアクセスを許可する。
     * VHEの場合はこれは不要である。ホストカーネルはEL2で動作し、EL0のアクセスは
     * ブートプロセスの後段で設定されるからである。
     * HCR_EL2.E2H == 1 の場合、CNTHCTL_EL2はCNTKCTL_EL1と同じビット
     * レイアウトとなり、CNTKCTL_EL1へのアクセス命令がCNTHCTL_EL2への
     * アクセスに再定義さることに注意されたい。これにより、EL1で動作する
     * ように設計されたカーネルは、EL2でCNTKCTL_EL1にアクセスすることで
     * EL0のビットを透過的に操作することができる。
     */
    cbnz    x2, 1f
    mrs    x0, cnthctl_el2
    orr    x0, x0, #3            // EL1物理タイマを有効化
    msr    cnthctl_el2, x0
1:
    msr    cntvoff_el2, xzr      //仮想オフセットをクリア

```

このコードはコメントで十分説明されています。私が付け加えることは何もありません。

```
#ifdef CONFIG_ARM_GIC_V3
    /* GICv3システムレジスタのアクセス */
    mrs    x0, id_aa64pfr0_el1
    ubfx    x0, x0, #24, #4         // GIC: [27:24]ビット
    cmp    x0, #1                   // GIC v3があるか
    b.ne    3f                      // なければ終わり

    mrs_s    x0, SYS_ICC_SRE_EL2
    orr    x0, x0, #ICC_SRE_EL2_SRE    // Set ICC_SRE_EL2.SRE==1
    orr    x0, x0, #ICC_SRE_EL2_ENABLE // Set ICC_SRE_EL2.Enable==1
    msr_s    SYS_ICC_SRE_EL2, x0
    isb                                // SREがセットされていることを保証
    mrs_s    x0, SYS_ICC_SRE_EL2       // SREを再度読み込み
    tbz    x0, #0, 3f                  // セットされているかチェック
    msr_s    SYS_ICH_HCR_EL2, xzr      // ICC_HCR_EL2をデフォルトにリセット

3:
#endif
```

このコードスニペットは、GICv3が利用可能で有効な場合にのみ実行されます。GICは
Generic Interrupt Controller（汎用割り込みコントローラ）の略です。GIC仕様書の
v3バージョンでは仮想化のコンテキストで特に有用な機能がいくつか追加されています。
たとえば、GICv3では、LPI（Locality-specific Peripheral Interrupt: ローカル固有
ペリフェラル割り込み）が可能になりました。このような割り込みは、メッセージ
バスを介してルーティングされ、その設定はメモリ内の特別なテーブルに保持されます。

上のコードはSRE（システムレジスタインタフェース）を有効にしています。
`ICC_*_ELn`レジスタを使用してGICv3の機能を利用する前にこの手順を実行する
必要があります。

```
    /* ID registersを設定 */
    mrs    x0, midr_el1
    mrs    x1, mpidr_el1
    msr    vpidr_el2, x0
    msr    vmpidr_el2, x1
```

`midr_el1`と`mpidr_el1`は、Identificationレジスタグループに属する読み込み
専用のレジスタです。これらのレジスタには、プロセッサの製造元、プロセッサの
アーキテクチャ名、コア数などの情報が含まれています。EL1からアクセスしようと
するすべての読者に対して、この情報を変更することができます。ここでは、`vpidr_el2`と`vmpidr_el2`に`midr_el1`と`mpidr_el1`から取得した値を入力して
いますので、EL1からアクセスしても、それ以上の例外レベルからアクセスしても、
この情報は同じになります。

```
#ifdef CONFIG_COMPAT
    msr    hstr_el2, xzr            // EL2でのCP15トラップを無効化
```

プロセッサが32ビット実行モードで動作している場合、「コプロセッサ」という
概念があります。コプロセッサは、64ビット実行モードでは通常システムレジスタを
介してアクセスされるような情報にアクセスするために使用されます。コプロセッサを
介してアクセス可能な正確な情報は[公式ドキュメント](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0311d/I1014521.html)で読むことができます。
`msr hstr_el2, xzr`命令により、低い例外レベルからコプロセッサを使用することが
できるようになります。これは互換モードが有効な場合にのみ意味があります（この
モードでは、カーネルは64ビットカーネル上で32ビットのユーザーアプリケーションを
実行することができます）。

```
    /* EL2 デバッグ */
    mrs    x1, id_aa64dfr0_el1        // ID_AA64DFR0_EL1のPMUVerビットをチェック
    sbfx   x0, x1, #8, #4
    cmp    x0, #1
    b.lt   4f                         // PMUがなければスキップ present
    mrs    x0, pmcr_el0               // EL2でのデバッグアクセストラップを無効化
    ubfx   x0, x0, #11, #5            // EL1からのすべてのPMUカウンタへの
4:
    csel   x3, xzr, x0, lt            // アクセスを許可

    /* 統計的プロファイリング */
    ubfx   x0, x1, #32, #4            // ID_AA64DFR0_EL1のPMSVerビットをチェック
    cbz    x0, 6f                     // SPEがなければスキップ
    cbnz   x2, 5f                     // VHE?
    mov    x1, #(MDCR_EL2_E2PB_MASK << MDCR_EL2_E2PB_SHIFT)
    orr    x3, x3, x1                 // VHEがなければ
    b      6f                         //   EL1&0変換を使う
5:                                    // VHEがあれば、EL2変換を使い、
    orr    x3, x3, #MDCR_EL2_TPMS     // EL1からのアクセスを無効に
6:
    msr    mdcr_el2, x3               // デバッグトラップを構成
```

このコードは`mdcr_el2`(モニタデバッグ構成レジスタ (EL2))の設定を行います。
このレジスタは、仮想化拡張に関連するさまざまなデバッグトラップを設定する
役割を果たします。このコードブロックの詳細は説明しません。なぜなら、デバッグと
トレースは今回の議論の範囲外だからです。詳細を知りたい方は[AArch64リファレンスマニュアル](https://developer.arm.com/docs/ddi0487/ca/arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile)の
`2810`ページにある`mdcr_el2`レジスタの説明を読んでください。

```
    /* ステージ-2 変換 */
    msr    vttbr_el2, xzr
```

OSがハイパーバイザとして使用される場合、ゲストOSに対して完全なメモリ隔離を
行う必要があります。ステージ 2の仮想メモリ変換はまさにこの目的に使用されます。
各ゲストOSはすべてのシステムメモリを所有していると考えますが、実際には各メモリ
アクセスはステージ 2の変換により物理メモリにマッピングされています。この時点
では、ステージ2変換は無効にするので、`vttbr_el2`は`0`に設定します。

```
    cbz    x2, install_el2_stub

    mov    w0, #BOOT_CPU_MODE_EL2        // このCPUはEL2でブート
    isb
    ret
```

まず、`x2`を`0`と比較してVHEが有効になっているかチェックします。なっている
場合は`install_el2_stub`ラベルにジャンプし、そうでない場合はCPUがEL2モードで
起動したことを記録して`el2_setup`関数を終了します。後者の場合、プロセッサは
EL2モードで動作し続け、EL1は全く使用されません。

```
install_el2_stub:
    /* sctlr_el1 */
    mov    x0, #0x0800            // RES{1,0} ビットをセット/クリア
CPU_BE(    movk    x0, #0x33d0, lsl #16    )    // BEシステムではEEとE0Eをセット
CPU_LE(    movk    x0, #0x30d0, lsl #16    )    // LEシステムではEEとE0Eをクリア
    msr    sctlr_el1, x0
```

ここに来たということはVHEは必要なく、すぐにEL1に切り替えるということなので、
ここでEL1の早期初期化を行う必要があります。上のコードスニペットは`sctlr_el1`
（システム制御レジスタ）の初期化を担当しています。RPi OSの[`boot.S#L18`](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson02/src/boot.S#L18)で
同じ作業を行っています。

```
    /* コプロセッサトラップ */
    mov    x0, #0x33ff
    msr    cptr_el2, x0            // EL2でのコプロセッサトラップを無効に
```

このコードは、EL1が`cpacr_el1`レジスタにアクセスできるようにし、その結果、
トレース、浮動小数点、Advanced SIMDの各機能へのアクセスを制御できるように
します。

```
    /* ハイパーバイザスタブ */
    adr_l    x0, __hyp_stub_vectors
    msr    vbar_el2, x0
```

今のところ、EL2を使用する予定はありませんが、いくつかの機能にはEL2が必要です。
たとえば、現在実行中のカーネルから別のカーネルをロードして起動することを可能に
する[kexec](https://linux.die.net/man/8/kexec)システムコールの実装に必要です。

[_hyp_stub_vectors](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/hyp-stub.S#L33)にはEL2のすべての例外ハンドラのアドレスが格納されて
います。次回のレッスンでは、割り込みと例外処理について詳しく説明し、EL1に
おける例外処理機能を実装する予定です。

```
    /* spsr */
    mov    x0, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT |\
              PSR_MODE_EL1h)
    msr    spsr_el2, x0
    msr    elr_el2, lr
    mov    w0, #BOOT_CPU_MODE_EL2        // このCPUはEL2でブート
    eret
```

最後に、EL1でのプロセッサの状態を初期化し、例外レベルを切り替える必要が
あります。これはすでに[RPi OS](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson02/src/boot.S#L27-L33)で行っていますので、このコードの
詳細は説明しません。

`elr_el2`の初期化方法だけは新しい点です。`lr`（リンクレジスタ）は`x30`の
エイリアスです。`bl`(分岐リンク)命令を実行する度に`x30`には自動的に現在の
命令のアドレスが入力されます。通常、この事実は`ret`命令によって使用される
ので、`ret`命令はどこに戻るかを正確に知っています。今回の例では、`lr`は
[`head.S#L119`](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L119)を
指しており、`elr_el2`をこのように初期化したことで、EL1に切り替わった後には
ここから実行が再開されることになります。

### EL1におけるプロセッサの初期化

`stext`関数に戻ってきました。ここから先の数行はあまり重要ではありませんが
念のために説明しておきます。
```
    adrp   x23, __PHYS_OFFSET
    and    x23, x23, MIN_KIMG_ALIGN - 1    // KASLRオフセット、デフォルトは0
```

[KASLR](https://lwn.net/Articles/569635/)（カーネルアドレス空間ランダム化）
とは、カーネルをメモリのランダムなアドレスに配置する技術です。これは、
セキュリティ上の理由からのみ必要となります。詳細については、上記のリンクを
読んでください。

```
    bl    set_cpu_boot_mode_flag
```

ここでCPUのブートモードを[__boot_cpu_mode](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/virt.h#L74)
変数に保存します。これを行うコードは、前に説明した`preserve_boot_args`関数と
非常によく似ています。

```
    bl    __create_page_tables
    bl    __cpu_setup            // プロセッサを初期化
    b     __primary_switch
```

この最後の3つの関数は非常に重要ですが、いずれも仮想メモリ管理に関連している
ため、詳細な検討はレッスン6まで延期します。今のところは、その意味を簡単に
説明したいと思います

* `create_page_tables` その名の通り、ページテーブルの作成を担当します。
* `__cpu_setup` 様々なプロセッサの設定、ほとんどは仮想メモリ管理に固有の設定を初期化します。
* `__primary_switch` MMUを有効にして、アーキテクチャに依存しない開始点である[start_kernel](https://github.com/torvalds/linux/blob/v4.14/init/main.c#L509)
関数にジャンプします。

### 結論

本章では、Linuxカーネルの起動時にプロセッサがどのように初期化されるかについて
簡単に説明しました。次のレッスンでは、引き続きARMプロセッサに密着し、すべての
OSにとって重要なトピックである割り込み処理について調べていきます。

##### 前ページ

2.1 [プロセッサの初期化: RPi OS](../../ja/lesson02/rpi-os.md)

##### 次ページ

2.3 [プロセッサの初期化: 演習](../../ja/lesson02/exercises.md)
