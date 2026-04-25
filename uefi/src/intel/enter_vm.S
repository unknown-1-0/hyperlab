.intel_syntax noprefix
.global intel_do_enter_vm

#include <intel/vmcs.h>

// Perform VM-entry with provided general-purpose registers
// The new guest registers are not saved if this function returns 0
// args: RCX - ptr to guest general purpose registers
//       RDX - ptr to memory area where to save general-purpose registers
// return: 0 - VM-entry failed (one of VM instructions failed unexpectedly)
//         1 - VM-entry succeeded (VM-exit reason is not checked! Check that in case VM-entry actually failed)
// Guest RIP and RFLAGS must be loaded BEFORE calling this function
// Registers RAX-R15 are laid out in memory in the same order as they are encoded in instructions
intel_do_enter_vm:
push rbx
push rbp
push rdi
push rsi
push r12
push r13
push r14
push r15

push rdx

mov eax, VMCS_HOST_RSP
vmwrite rax, rsp
jz .fail
jc .fail

mov eax, VMCS_HOST_RIP
lea rbx, [rip+.handle_vmexit]
vmwrite rax, rbx
jz .fail
jc .fail

mov eax, VMCS_GUEST_RSP
mov rbx, [rcx+0x20]
vmwrite rax, rbx
jz .fail
jc .fail

mov rax, [rcx]
mov rdx, [rcx+0x10]
mov rbx, [rcx+0x18]
// guest RSP is already loaded
mov rbp, [rcx+0x28]
mov rsi, [rcx+0x30]
mov rdi, [rcx+0x38]
mov r8, [rcx+0x40]
mov r9, [rcx+0x48]
mov r10, [rcx+0x50]
mov r11, [rcx+0x58]
mov r12, [rcx+0x60]
mov r13, [rcx+0x68]
mov r14, [rcx+0x70]
mov r15, [rcx+0x78]

mov rcx, [rcx+8]

vmresume

push rax
mov eax, VMCS_VM_INSTRUCTION_ERROR
vmread rax, rax

cmp rax, VM_INSTRUCTION_ERROR_VMRESUME_WITH_NON_LAUNCHED_VMCS
pop rax

jne .fail

vmlaunch

.fail:
xor eax, eax
pop rdx
jmp .restore_nonvolatile_regs

.handle_vmexit:
xchg rdx, [rsp]
mov [rdx], rax
mov [rdx+0x8], rcx

pop rax
mov [rdx+0x10], rax

mov [rdx+0x18], rbx

mov eax, VMCS_GUEST_RSP
vmread rax, rax
mov [rdx+0x20], rax
mov [rdx+0x28], rbp
mov [rdx+0x30], rsi
mov [rdx+0x38], rdi
mov [rdx+0x40], r8
mov [rdx+0x48], r9
mov [rdx+0x50], r10
mov [rdx+0x58], r11
mov [rdx+0x60], r12
mov [rdx+0x68], r13
mov [rdx+0x70], r14
mov [rdx+0x78], r15

mov eax, 1
.restore_nonvolatile_regs:
xor ecx, ecx
xor edx, edx
xor esi, esi
xor edi, edi
xor r8d, r8d
xor r9d, r9d
xor r10d, r10d
xor r11d, r11d
pop r15
pop r14
pop r13
pop r12
pop rsi
pop rdi
pop rbp
pop rbx
ret


