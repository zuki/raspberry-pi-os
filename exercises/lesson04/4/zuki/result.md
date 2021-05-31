# 方針

現在配列で持っているタスクリストをリストで持つようにすれば良いはず。しかも、追加だけなので線形リストでOK.

## 実装

```
struct task_struct {
    struct cpu_context cpu_context;
    long state;
    long counter;
    long priority;
    long preempt_count;
    struct task_struct *next;       // 次へのリンクを追加
}

struct task_struct *task_list = &(init_task);   // task配列に替わるtaskリスト

struct task_struct *newtask;                    // 新規タスクの追加
newtask->next = 0;                              // 新規タスクのnextはヌル

struct task_struct *prev = task_list;           // リストの最後のタスクを見つけて
while(prev->next) {
    prev = prev->next;
}
prev->next = newtask;                           // 最後のタスクのnextを新規タスクに
```
