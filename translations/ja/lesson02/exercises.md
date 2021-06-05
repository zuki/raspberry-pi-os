## 2.3: 演習

1. EL3からEL1に直接ジャンプするのではなく、まずEL2に移行してからEL1に切り替える
ようにしてください。
2. FP/SIMDレジスタを使用した場合、EL3ではすべてうまくいきますが、EL1になるとすぐに
プリント機能が動作しなくなるという問題がありました。そのため、コンパイラオプションに
[-mgeneral-regs-only](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson02/Makefile#L3)パラメータを追加しました。まず、このパラメータを削除して、この動作を再現
してみてください。次に、`objdump`ツールを使い`-mgeneral-regs-only`フラグがないとgccが
FP/SIMDレジスタをどのように利用するかを確認してください。最後に、`cpacr_el1`を使って
FP/SIMDレジスタが使えるようにしてください。
3. レッスン02をqemu上で実行できるようにしてください。[このissue](https://github.com/s-matyukevich/raspberry-pi-os/issues/8)を参考にしてください。

##### 前ページ

2.2 [プロセッサの初期化: Linux](../../ja/lesson02/linux.md)

##### 次ページ

3.1 [割り込み処理: RPi OS](../../ja/lesson03/rpi-os.md)
