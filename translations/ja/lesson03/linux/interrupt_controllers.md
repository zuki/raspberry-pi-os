## 3.3: 割り込みコントローラ

この章では、Linuxのドライバとそれがどのように割り込みを処理するかについて詳しく
説明します。まず、ドライバの初期化コードから始めて、[handle_arch_irq](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/irq.c#L44)
関数後に割り込みがどのように処理されるかを見ていきます。

### デバイスツリーを使って必要なデバイスとドライバを探す

RPi OSにおける割り込みを実装する際に、システムタイマと割り込みコントローラという
2つのデバイスを扱いました。ここでは、同じデバイスがLinuxでどのように動作するかを
理解することを目的とします。まず最初にしなければならないことはこのデバイスを扱う
ドライバを見つけることです。必要なドライバを見つけるには[bcm2837-rpi-3-b.dts](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837-rpi-3-b.dts)
デバイスツリーファイルが使用できます。これはRaspberry Pi 3 Model B専用のトップ
レベルのデバイスツリーファイルですが、Raspberry Piの様々なバージョンで
共有される、より一般的なデバイスツリーファイルをインクルードしています。
インクルードの連鎖をたどり、`timer`と`interrupt-controller`を検索すると
次の4つのデバイスが見つかります。

1. [ローカル割り込みコントローラ](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L11)
2. [ローカルタイマ](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L20)
3. グローバル割り込みコントローラ。これは、[`bcm283x.dtsi#L109`](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L109)
で定義され、[`bcm2837.dtsi#L72`](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L72)で
変更されています。
4. [システムタイマ](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm283x.dtsi#L57)

待ってください。なぜ2つではなく、4つのデバイスがあるのでしょうか。これには説明が
必要ですので、次の節でこの疑問に取り組みます。

### ローカル割り込みコントローラ vs グローバル割り込みコントローラ

マルチプロセッサシステムの割り込み処理を考えると、疑問に思うことがあります。
ある割り込みの処理をどのコアが担当するのかという疑問です。また、割り込みが
発生したとき、4つのコアのすべてが割り込まれるのか、それとも1つのコアだけが
割り込まれるのか。さらに、特定の割り込みを特定のコアに割り当てることは可能か。
あるプロセッサが別のプロセッサに情報を渡す必要がある場合、どのように他の
プロセッサに通知するのかという疑問もあるでしょう。

ローカルインタラプトコントローラはこれらの疑問のすべてを解決してくれる
デバイスです。ローカルインタラプトコントローラは次の役割を担っています。

* 特定の割り込みをどのコアが受け取るべきかを設定する。
* コア間で割込みを送信する。このような割り込みは「メールボックス」と呼ばれ、
コア同士の通信を可能にします。
* ローカルタイマ割り込みやパフォーマンスモニター割り込み（PMU）を処理する。

ローカル割り込みコントローラとローカルタイマの動作については、[BCM2836 ARM-local peripherals](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)
マニュアルに記載されています。

ローカルタイマについては既に何度か触れています。では、なぜシステムには2つの
独立したタイマが必要なのか疑問に思うでしょう。ローカルタイマの主なユースケースは、
4つのコアが同時にタイマ割り込みを受け取るように設定したい場合だと思います。
システムタイマを使用すると割り込みは1つのコアにしかルーティングできないからです。

RPi OSではローカル割り込みコントローラもローカルタイマも使用しませんでした。
デフォルトですべての外部割り込みが最初のコアに送られるようにローカル割り込み
コントローラが設定されており、この設定はまさに私たちが必要としているものだから
です。また、ローカルタイマを使用しなかったのは、代わりにシステムタイマを使用した
からです。

### ローカル割り込みコントローラ

[bcm2837.dtsi](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L75)に
よると、グローバル割り込みコントローラはローカル割り込みコントローラの子です。
したがって、ローカルコントローラから調査を始めることは理にかなっています。

特定のデバイスで動作するドライバを見つける必要がある場合は[compatible](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L12)
プロパティを使用します。このプロパティの値を検索すると、RPiのローカル割り込み
コントローラと互換性のあるドライバが1つだけあることが簡単にわかります。 以下に
対応する[定義](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L315)を示します。

```
IRQCHIP_DECLARE(bcm2836_arm_irqchip_l1_intc, "brcm,bcm2836-l1-intc",
        bcm2836_arm_irqchip_l1_intc_of_init);
```

ドライバの初期化手順がどのようなものであるかはおそらく推測できるでしょう。
カーネルはデバイスツリーにあるすべてのデバイス定義を走査し、各定義に対して
"compatible"プロパティを使って一致するドライバを探します。ドライバが見つかったら、
その初期化関数を呼び出します。初期化関数はデバイスの登録時に提供されており、
ここでは[bcm2836_arm_irqchip_l1_intc_of_init](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L280)です。

```
static int __init bcm2836_arm_irqchip_l1_intc_of_init(struct device_node *node,
                              struct device_node *parent)
{
    intc.base = of_iomap(node, 0);
    if (!intc.base) {
        panic("%pOF: unable to map local interrupt registers\n", node);
    }

    bcm2835_init_local_timer_frequency();

    intc.domain = irq_domain_add_linear(node, LAST_IRQ + 1,
                        &bcm2836_arm_irqchip_intc_ops,
                        NULL);
    if (!intc.domain)
        panic("%pOF: unable to create IRQ domain\n", node);

    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPSIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPNSIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTHPIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTVIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_GPU_FAST,
                     &bcm2836_arm_irqchip_gpu);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_PMU_FAST,
                     &bcm2836_arm_irqchip_pmu);

    bcm2836_arm_irqchip_smp_init();

    set_handle_irq(bcm2836_arm_irqchip_handle_irq);
    return 0;
}
```

初期化関数は2つのパラメータ`node`と`parent`を受け取ります。どちらの型も[struct device_node](https://github.com/torvalds/linux/blob/v4.14/include/linux/of.h#L49)
です。`node`はデバイスツリーのカレントノードを表し、ここでは[ローカル割り込みコントローラ](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L11)
を指しています。`parent`はデバイスツリー階層における親ノードであり、ローカル
割り込みコントローラの場合は`soc`要素を指しています（`soc`は"system on chip"の略で、
すべてのデバイスレジスタをメインメモリに直接マッピングする最もシンプルなバスです）。

`node`を使ってカレントデバイスツリーノードから様々なプロパティを読み取ることが
できます。たとえば、`bcm2836_arm_irqchip_l1_intc_of_init`関数の1行目では[reg](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L13)
プロパティからデバイスのベースアドレスを読み込んでいます。しかし、その過程はより
複雑です。この関数が実行される際、MMUはすでに有効になっているので、物理メモリの
領域にアクセスする前にその領域を何らかの仮想アドレスにマッピングする必要がある
からです。[of_iomap](https://github.com/torvalds/linux/blob/v4.14/drivers/of/address.c#L759)
関数が行っているのはまさにこれです。この関数は提供されたノードの`reg`プロパティを
読み込み、`reg`プロパティに記述されているメモリ領域全体をある仮想メモリ領域に
マッピングします。

次に、ローカルタイマの周波数を[bcm2835_init_local_timer_frequency](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L264)
関数で初期化します。この関数については特に言うことはありません。[BCM2836 ARM-local peripherals](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)
マニュアルに記載されているレジスタを使用してローカルタイマを初期化しているだけです。

次の行は少し説明が必要です。

```
    intc.domain = irq_domain_add_linear(node, LAST_IRQ + 1,
                        &bcm2836_arm_irqchip_intc_ops,
                        NULL);
```

Linuxでは各割込みにユニークな整数番号を割り当てます。この番号はユニークな割込みIDと
考えることができます。このIDは（たとえば、ハンドラの割り当て、割り込みを処理する
CPUの割り当てなど）割り込みに対して何かを行う場合常に使用されます。割込みには
ハードウェア割込み番号も割り当てられます。これは通常、どの割り込みラインがトリガ
されたかを示す番号です。`BCM2837 ARM Peripherals`マニュアルの113ページには
ペリフェラル割り込みテーブルがありますが、このテーブルのインデックスがハードウェア
割り込み番号であると考えることができます。したがって、Linuxの割り込み番号を
ハードウェア割り込み番号にマッピングしたり、その逆を行うメカニズムが必要です。
割り込みコントローラが1つしかない場合は、1対1のマッピングが可能ですが、
一般的なケースでは、より洗練されたメカニズムを使用する必要があります。
Linuxでは[struct irq_domain](https://github.com/torvalds/linux/blob/v4.14/include/linux/irqdomain.h#L152) が
このマッピングを実装しています。各割込みコントローラドライバは独自のirqドメインを
作成し、処理可能なすべての割込みをこのドメインに登録する必要があります。
登録関数は、その後その割り込みの処理に使われるLinux割り込み番号を返します。

次の6行はサポートしている割込みをirqドメインに登録するためのものです。

```
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPSIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPNSIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTHPIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTVIRQ,
                     &bcm2836_arm_irqchip_timer);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_GPU_FAST,
                     &bcm2836_arm_irqchip_gpu);
    bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_PMU_FAST,
                     &bcm2836_arm_irqchip_pmu);
```

[BCM2836 ARM-local peripherals](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)
マニュアルによると、ローカル割り込みコントローラは10種類の割り込みを処理します。
0～3はローカルタイマからの割り込み、4～7はプロセス間通信に使用されるメールボックス
割り込み、8はグローバル割り込みコントローラが生成するすべての割り込み、9は
パフォーマンスモニタ割り込みです。ここでは、ドライバが定義している各割込みの
ハードウェアIRQ番号を保持している一連の定数は[irq-bcm2836.c#L67](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L67)
にあります。上記の登録コードでは、別のところで登録されているメールボックス
割り込みを除くすべての割り込みを登録しています。この登録コードをより深く理解する
ために[bcm2836_arm_irqchip_register_irq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L154)
関数を調べましょう。

```
static void bcm2836_arm_irqchip_register_irq(int hwirq, struct irq_chip *chip)
{
    int irq = irq_create_mapping(intc.domain, hwirq);

    irq_set_percpu_devid(irq);
    irq_set_chip_and_handler(irq, chip, handle_percpu_devid_irq);
    irq_set_status_flags(irq, IRQ_NOAUTOEN);
}
```

最初の行は実際の割り込み登録を行っています。[irq_create_mapping](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/irqdomain.c#L632)は
ハードウェア割り込み番号を引数に取り、Linux割り込み嵌合を返します。

[irq_set_percpu_devid](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/irqdesc.c#L849)は
割り込みを「CPUごと」に設定します。そのため、カレントCPUでのみ処理されます。
現在検討しているすべての割り込みはローカルであり、それらはすべてカレントCPUで
しか処理できないので、これは完全に理にかなっています。

[irq_set_chip_and_handler](https://github.com/torvalds/linux/blob/v4.14/include/linux/irq.h#L608)は
その名前が示すように、irqチップとirqハンドラを設定します。Irqチップはドライバが作成
する必要のある特殊な構造体であり、特定の割り込みをマスクしたりアンマスクしたりする
メソッドを持っています。現在調べているドライバは、
[タイマ](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L118)チップと
[PMU](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L134)チップ、
[GPU](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L148)チップの3つのirqチップを定義しており、外部ペリフェラルから発生するすべての割り込みを
制御します。ハンドラは割り込みの処理を担当する関数です。この例では汎用の
[handle_percpu_devid_irq](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/chip.c#L859)関数を
ハンドラとして設定しています。このハンドラは後でグローバル割り込みコントローラ
ドライバによって書き換えられます。

ここでの[irq_set_status_flags](https://github.com/torvalds/linux/blob/v4.14/include/linux/irq.h#L652)は
カレント割り込みは手動で有効にすべきであり、デフォルトでは有効にすべきでないことを
示すフラグを設定しています。

`bcm2836_arm_irqchip_l1_intc_of_init`関数に戻ると、後は2つの関数呼び出ししか
残っていません。最初の一つは[bcm2836_arm_irqchip_smp_init](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L243)です。
ここではメールボックス割り込みが有効になり、プロセッサのコア同士の通信が
可能になります。

最後の関数呼び出しは極めて重要です。これは低レベル例外処理コードがドライバに
接続される場所だからです。

```
    set_handle_irq(bcm2836_arm_irqchip_handle_irq);
```

[set_handle_irq](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/irq.c#L46)は
アーキテクチャ固有のコードで定義されており、この関数にはすでに見ています。
このコードから[bcm2836_arm_irqchip_handle_irq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2836.c#L164)が低レベル例外コードから呼び出される
ことがわかります。この関数自体を以下に示します。

```
static void
__exception_irq_entry bcm2836_arm_irqchip_handle_irq(struct pt_regs *regs)
{
    int cpu = smp_processor_id();
    u32 stat;

    stat = readl_relaxed(intc.base + LOCAL_IRQ_PENDING0 + 4 * cpu);
    if (stat & BIT(LOCAL_IRQ_MAILBOX0)) {
#ifdef CONFIG_SMP
        void __iomem *mailbox0 = (intc.base +
                      LOCAL_MAILBOX0_CLR0 + 16 * cpu);
        u32 mbox_val = readl(mailbox0);
        u32 ipi = ffs(mbox_val) - 1;

        writel(1 << ipi, mailbox0);
        handle_IPI(ipi, regs);
#endif
    } else if (stat) {
        u32 hwirq = ffs(stat) - 1;

        handle_domain_irq(intc.domain, hwirq, regs);
    }
}
```

この関数は`LOCAL_IRQ_PENDING`を読んで現在保留されている割り込みを把握します。
`LOCAL_IRQ_PENDING`レジスタは4つあり、各々が各自のプロセッサコアに対応している
ので、現在のプロセッサインデックスを使って正しいレジスタを選択しています。
メールボックス割り込みとその他の割り込みが2つのif文で処理されています。
マルチプロセッサシステムにおけるコア間の相互作用については、今回の議論の
対象外ですのでメールボックス割り込み処理の部分は省略します。次の2行だけが
説明されずに残りました。

```
        u32 hwirq = ffs(stat) - 1;

        handle_domain_irq(intc.domain, hwirq, regs);
```

ここで割り込みを次のハンドラに渡しています。まず、ハードウェアIRQ番号を計算
します。計算には[ffs](https://github.com/torvalds/linux/blob/v4.14/include/asm-generic/bitops/ffs.h#L13)
(Find first bit)関数を使います。ハードウェアirq番号が計算しらた[handle_domain_irq](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/irqdesc.c#L622)
関数を呼び出します。この関数はirq domainを使ってハードウェアirq番号をLinux irq
番号に変換し、([irq_desc](https://github.com/torvalds/linux/blob/v4.14/include/linux/irqdesc.h#L55)構造体に格納されている)irqの
構成をチェックしてから割り込みハンドラを呼び出します。ハンドラが[handle_percpu_devid_irq](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/chip.c#L859)に
設定されていることは既に見てきました。ただし、このハンドラは後で子の割り込み
コントローラによって上書きされます。では、その仕組みを見てみましょう。

### 汎用割り込みコントローラGeneric interrupt controller

デバイスツリーと`compatible`プロパティを使い、何らかのデバイスに対応する
ドライバを見つける方法はすでに見てきましたので、この部分は省略して、汎用
割り込みコントローラドライバのソースコードに直接飛びます。ソースコードは
[irq-bcm2835.c](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c)
ファイルにあります。いつものように、初期化関数から探究を始めます。それは a[armctrl_of_init](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L141)と呼ばれています。

```
static int __init armctrl_of_init(struct device_node *node,
				  struct device_node *parent,
				  bool is_2836)
{
	void __iomem *base;
	int irq, b, i;

	base = of_iomap(node, 0);
	if (!base)
		panic("%pOF: unable to map IC registers\n", node);

	intc.domain = irq_domain_add_linear(node, MAKE_HWIRQ(NR_BANKS, 0),
			&armctrl_ops, NULL);
	if (!intc.domain)
		panic("%pOF: unable to create IRQ domain\n", node);

	for (b = 0; b < NR_BANKS; b++) {
		intc.pending[b] = base + reg_pending[b];
		intc.enable[b] = base + reg_enable[b];
		intc.disable[b] = base + reg_disable[b];

		for (i = 0; i < bank_irqs[b]; i++) {
			irq = irq_create_mapping(intc.domain, MAKE_HWIRQ(b, i));
			BUG_ON(irq <= 0);
			irq_set_chip_and_handler(irq, &armctrl_chip,
				handle_level_irq);
			irq_set_probe(irq);
		}
	}

	if (is_2836) {
		int parent_irq = irq_of_parse_and_map(node, 0);

		if (!parent_irq) {
			panic("%pOF: unable to get parent interrupt.\n",
			      node);
		}
		irq_set_chained_handler(parent_irq, bcm2836_chained_handle_irq);
	} else {
		set_handle_irq(bcm2835_handle_irq);
	}

	return 0;
}
```

では、この関数を詳細に見ていきましょう。

```
    void __iomem *base;
    int irq, b, i;

    base = of_iomap(node, 0);
    if (!base)
        panic("%pOF: unable to map IC registers\n", node);

    intc.domain = irq_domain_add_linear(node, MAKE_HWIRQ(NR_BANKS, 0),
            &armctrl_ops, NULL);
    if (!intc.domain)
        panic("%pOF: unable to create IRQ domain\n", node);

```

この関数は、デバイスツリーからデバイスのベースアドレスを読み取り、irqドメインを
初期化するコードから始まります。この部分はローカルIRQコントローラドライバで
同じようなコードを見たことがあるのでもうお馴染みでしょう。

```
    for (b = 0; b < NR_BANKS; b++) {
        intc.pending[b] = base + reg_pending[b];
        intc.enable[b] = base + reg_enable[b];
        intc.disable[b] = base + reg_disable[b];
```

次に、すべてのirqバンクを繰り返し処理するループがあります。irqバンクについてはすでに
このレッスンの最初の章で簡単に触れています。割り込みコントローラには3つのirqバンクが
あり、各々、`ENABLE_IRQS_1`、`ENABLE_IRQS_2`、`ENABLE_BASIC_IRQS`レジスタで制御
されています。バンクには各自3つのレジスタ: イネーブル，ディセーブル，ペンディングが
あります。イネーブルとディスエーブルレジスタは、そのバンクに属する個々の割り込みを
有効または無効にするために使用できます。ペンディングレジスタは、どの割り込みが
処理待ちであるかを判別するために使用します。

```
        for (i = 0; i < bank_irqs[b]; i++) {
            irq = irq_create_mapping(intc.domain, MAKE_HWIRQ(b, i));
            BUG_ON(irq <= 0);
            irq_set_chip_and_handler(irq, &armctrl_chip,
                handle_level_irq);
            irq_set_probe(irq);
        }
```

ついで、サポートしている割り込みの登録、irqチップとハンドラの設定を行う入れ子の
ループがあります。

同じ関数がローカル割り込みコントローラドライバでどのように使われているかは
すでに見ました。しかし、いくつかの重要な点を強調しておきたいと思います。

* [MAKE_HWIRQ](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L57)マクロは、ハードウェアIRQ番号の算出に使用します。バンク
インデックスとバンク内のIRQインデックスに基づいて計算されます。
* [handle_level_irq](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/chip.c#L603)は、
レベルタイプの割り込みに使用される共通ハンドラです。このタイプの割り込みは
割り込みが確認されるまで、割り込みラインを"ハイ"に保ちます。また、動作の異なる
エッジタイプの割り込みもあります。
* [irq_set_probe](https://github.com/torvalds/linux/blob/v4.14/include/linux/irq.h#L667)関数は、
[IRQ_NOPROBE](https://github.com/torvalds/linux/blob/v4.14/include/linux/irq.h#L64)
割り込みフラグを解除するだけで、割り込みのオートプロービングを効果的に無効にします。
割り込みオートプロービングとは、ドライバが各自のデバイスがどの割り込みラインに
接続されているかを発見できるようにするためのプロセスです。Raspberry Piでは
この情報がデバイスツリーに書かれているので必要ありませんが、これが役に立つ
デバイスもあるかもしれませんん。Linuxカーネルにおけるオートプロービングの仕組みに
ついては[このコメント](https://github.com/torvalds/linux/blob/v4.14/include/linux/interrupt.h#L662)を参照してください。

次のコードは、BCM2836とBCM2835の割り込みコントローラで異なります（前者はRPiモデル2と
3に、後者はRPiモデル1に対応します）。BCM2836を扱っている場合は、以下のコードが
実行されます。

```
        int parent_irq = irq_of_parse_and_map(node, 0);

        if (!parent_irq) {
            panic("%pOF: unable to get parent interrupt.\n",
                  node);
        }
        irq_set_chained_handler(parent_irq, bcm2836_chained_handle_irq);
```

デバイスツリーはローカル割込みコントローラがグローバル割込みコントローラの親で
あると[示しています](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L75)。
別のデバイスツリーの[プロパティ](https://github.com/torvalds/linux/blob/v4.14/arch/arm/boot/dts/bcm2837.dtsi#L76)は
グローバル割り込みコントローラはローカルコントローラの割り込み線番号8に接続されて
いることを示しています。これは、親のirqはハードウェアirq番号が8のirqであることを
意味します。この2つのプロパティによりLinuxカーネルは親の割り込み番号を知ることが
できます（これはLinux割り込み番号であり、ハードウェア番号ではありません）。最後に。[irq_set_chained_handler](https://github.com/torvalds/linux/blob/v4.14/include/linux/irq.h#L636)
関数が親irqのハンドラを[bcm2836_chained_handle_irq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L246)
関数に置き換えます。

[bcm2836_chained_handle_irq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L246)は非常にシンプルです。そのコードを以下に示します。

```
static void bcm2836_chained_handle_irq(struct irq_desc *desc)
{
    u32 hwirq;

    while ((hwirq = get_next_armctrl_hwirq()) != ~0)
        generic_handle_irq(irq_linear_revmap(intc.domain, hwirq));
}
```

このコードはRPi OSのために[irq.c#L39](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson03/src/irq.c#L39)で
行ったものの上位版と考えることができます。[get_next_armctrl_hwirq](https://github.com/torvalds/linux/blob/v4.14/drivers/irqchip/irq-bcm2835.c#L217)は
3つの保留レジスタのすべてを使ってどの割り込みが発生したかを調べます。[irq_linear_revmap](https://github.com/torvalds/linux/blob/v4.14/include/linux/irqdomain.h#L377)は
irqドメインを使ってハードウェアirq番号をLinux irq番号に変換し、[generic_handle_irq](https://github.com/torvalds/linux/blob/v4.14/include/linux/irqdesc.h#L156)は
irqハンドラを実行します。irqハンドラは初期化関数で設定されており、最終的に
その割り込みに関連するすべてのirqアクションを実行する[handle_level_irq](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/chip.c#L603)を
指しています(これは実際に[`handle.c#L135`](https://github.com/torvalds/linux/blob/v4.14/kernel/irq/handle.c#L135)で
行われています)。今のところ、サポートされているすべての割り込みに対して、
irqアクションのリストは空になっています。何らかの割り込みを処理したいと考える
ドライバは適切なリストにアクションを追加する必要があります。次の章では、
システムタイマを例として、これがどのように行われるかを見ていきます。

##### 前ページ

3.2 [割り込み処理: Linuxにおける低レベル例外処理](../../../ja/lesson03/linux/low_level-exception_handling.md)

##### 次ページ

3.4 [割り込み処理: タイマ](../../../ja/lesson03/linux/timer.md)
