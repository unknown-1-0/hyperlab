#ifndef _EXCEPTION_HANDLERS_H_
#define _EXCEPTION_HANDLERS_H_

#include <efi.h>
#include <stdint.h>
int setup_idt(EFI_SYSTEM_TABLE* SystemTable);
void restore_idt(EFI_SYSTEM_TABLE* SystemTable);

struct iret_stack
{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct exception_handler_stack
{
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    //uint64_t rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    union {
        struct iret_stack iret_stack_no_error_code;
        struct {
            uint64_t error_code;
            struct iret_stack iret_stack_with_error_code;
        };
    };
};

#endif