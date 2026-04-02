.intel_syntax noprefix
.global x86_load_segments
// Load code, data, and task segments
// arg 1 (rcx): code segment
// arg 2 (rdx): data segment
// arg 3 (r8): task segment
x86_load_segments:
lea rax, [rip+.load_data_and_task_segments]
push rcx
push rax
retfq
.load_data_and_task_segments:
mov es, rdx
mov ds, rdx
mov ss, rdx
mov fs, rdx
mov gs, rdx

ltr r8w
ret

