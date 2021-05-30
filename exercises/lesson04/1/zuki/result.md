```bash
$ make run
qemu-system-aarch64 -m 1024 -M raspi3 -serial null -serial mon:stdio -nographic -kernel kernel8.img
text  start: 0xd53800a0
data  start: 0x0
bss   begin: 0x0
bss     end: 0x0
HIGH_MEMORY: 0x3f000000
init        [0]: pointer: 0x83298, stack: 0x400000
new process [1]: pointer: 0x400000, stack: 0x401000
new process [2]: pointer: 0x401000, stack: 0x402000
new process [3]: pointer: 0x402000, stack: 0x403000

switch to task 1
Tasks state:
    0: sp: 0x0
    1: sp: 0x401000
    2: sp: 0x402000
    3: sp: 0x403000
123451234512345123451234512345123451
switch to task 2
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x401000
    2: sp: 0x402000
    3: sp: 0x403000
abcdeabcdeabcdeabcdeabcd
switch to task 3
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x402000
    3: sp: 0x403000
0, 1, 1, 2,
switch to task 0
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x403000

switch to task 1
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30
23451234512345123451234512
switch to task 2
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30
eabcdeabcdeabcdeabc
switch to task 3
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30
3, 5, 8, 13,
switch to task 0
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e00
    3: sp: 0x402e30

switch to task 1
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e00
    3: sp: 0x402e30
34512345123451234512345123451234
switch to task 2
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e00
    3: sp: 0x402e30
deabcdeabcdeabcdea
switch to task 3
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e00
    3: sp: 0x402e30
21, 34,
switch to task 0
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30

switch to task 1
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30
512345123451234512345123451
switch to task 2
Tasks state:
    0: sp: 0x3fff70
    1: sp: 0x400e20
    2: sp: 0x401e20
    3: sp: 0x402e30
bcdeabQEMU: Terminated
```
