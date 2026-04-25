.intel_syntax noprefix
.global intel_do_enter_vm

#include <intel/vmcs.h>
#include <vm_context.h>

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
mov rbx, [rcx+REG_RSP*8]
vmwrite rax, rbx
jz .fail
jc .fail

mov rax, [rcx+REG_RAX*8]
mov rdx, [rcx+REG_RDX*8]
mov rbx, [rcx+REG_RBX*8]
// guest RSP is already loaded
mov rbp, [rcx+REG_RBP*8]
mov rsi, [rcx+REG_RSI*8]
mov rdi, [rcx+REG_RDI*8]
mov r8, [rcx+8*8]
mov r9, [rcx+9*8]
mov r10, [rcx+10*8]
mov r11, [rcx+11*8]
mov r12, [rcx+12*8]
mov r13, [rcx+13*8]
mov r14, [rcx+14*8]
mov r15, [rcx+15*8]

mov rcx, [rcx+REG_RCX*8]

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
mov [rdx+REG_RAX*8], rax
mov [rdx+REG_RCX*8], rcx

pop rax
mov [rdx+REG_RDX*8], rax

mov [rdx+REG_RBX*8], rbx

mov eax, VMCS_GUEST_RSP
vmread rax, rax
mov [rdx+REG_RSP*8], rax
mov [rdx+REG_RBP*8], rbp
mov [rdx+REG_RSI*8], rsi
mov [rdx+REG_RDI*8], rdi
mov [rdx+8*8], r8
mov [rdx+9*8], r9
mov [rdx+10*8], r10
mov [rdx+11*8], r11
mov [rdx+12*8], r12
mov [rdx+13*8], r13
mov [rdx+14*8], r14
mov [rdx+15*8], r15

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


