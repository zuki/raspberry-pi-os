## 1.5: 演習

演習は任意ですが、ソースコードを少しでも試してみることを強くお勧めします。演習を終えられた方は
ぜひそのソースコードを他の方と共有してください。詳しくは[貢献のためのガイド](../Contributions.md)を
ご覧ください。

1. 定数`baud_rate`を導入し、この定数を使って必要なMini UARTレジスタの値を計算してください。
   プログラムが115200以外のボーレートで動作することを確認してください。
2. OSのコードを変更し、Mini UARTではなくUARTデバイスを使用するようにしてください。UARTレジスタへの
   アクセス方法やGPIOピンの設定方法については「BCM2837 ARMペリフェラル」マニュアルと[ARM PrimeCell UART (PL011)](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183g/DDI0183G_uart_pl011_r1p5_trm.pdf)マニュアルを
   参照してください。UARTデバイスは48MHzのクロックをベースとして使用しています。
3. 4つのプロセッサコアをすべて使用するようにしてください。OSはすべてのコアが
   "Hello, from processor <processor index>"と表示する必要があります。各コアに個別のスタックを設定し、
   Mini UARTは一度だけ初期化されるようにすることを忘れないでください。グローバル変数と`delay`関数を
   組み合わせることで同期を行うことができます。
4. レッスン01をqemu上で実行できるようにしてください。[このissue](https://github.com/s-matyukevich/raspberry-pi-os/issues/8)を
   参考にしてください。

##### 前ページ

1.4 [カーネル初期化: Linuxの起動シーケンス](../../docs/lesson01/linux/kernel-startup.md)

##### 次ページ

2.1 [プロセッサ初期化: RPi OS](../../docs/lesson02/rpi-os.md)
