# lesson06のソースそのままでは動かない

## 現象

kernel_mainまでいかずqemuがスタックする。

## 回避策

`xdfm1`氏のexercisesは問題なく動くため、ソースを見るとページテーブルをセットする前にidmap_dirという別のテーブルを作成してttbr0_el1に設定していることがわかった。これをlesson06に適用すると問題なく動くことを確認。

```
$ qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
Kernel process started. EL 1
User process
12345123451234512345123451234512345123451234512345123451
```

その理由はstack overflowの[この記事](https://stackoverflow.com/questions/57292720/how-to-enable-mmu-in-qemu-virt-machine-a57-cpu)の回答に該当すると思われる。

回答によれば低位アドレス用の1対1マップを作れということだが、実際、idmapはva=paの1対1マップになっている。ちなみに、idmapはidentity map（恒等マップ）の略だろう。
