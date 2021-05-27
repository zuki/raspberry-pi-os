## 3.5: 演習

1. プロセッサ割り込みの生成にシステムタイマではなくローカルタイマを使用してください。
詳細は[このissue](https://github.com/s-matyukevich/raspberry-pi-os/issues/70)を参照してください。
2. MiniUART割り込みを処理してください。`kernel_main`関数の最後のループを何もしないループに
置き換えてください。ユーザが新しい文字を入力したらすぐに割り込みを発生するようにMiniUART
デバイスを設定してください。新たに入力された各文字を画面に表示する役割を担う割り込み
ハンドラを実装してください。
3. レッスン03をqemu上で実行できるようにしてください。[このissue](https://github.com/s-matyukevich/raspberry-pi-os/issues/8)を参考にしてください。

##### 前ページ

3.4 [割り込み処理: タイマ](../../docs/lesson03/linux/timer.md)

##### 次ページ

4.1 [プロセススケジューラ: RPi OSスケジューラ](../../docs/lesson04/rpi-os.md)
