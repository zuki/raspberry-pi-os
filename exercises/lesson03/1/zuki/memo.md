# 演習3.5.1でエラー

```
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
Assertion failed: (LOCALTIMER_VALUE(s->local_timer_control) > 0), function bcm2836_control_local_timer_set_next, file ../hw/intc/bcm2836_control.c, line 201.
make: *** [run] Abort trap: 6
```

`LTIMER_CTRL`のbit[0:27]にreload値が必要なのに設定していなかったため。
