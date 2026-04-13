#ifndef _ARCH_X86_DEBUG_REGISTERS_H_
#define _ARCH_X86_DEBUG_REGISTERS_H_

#include <stdint.h>

static inline uint64_t __attribute__((always_inline)) read_dr7(void)
{
    uint64_t value;
    __asm__("mov %0, dr7":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr7(uint64_t value)
{
    __asm__("mov dr7, %0"::"r"(value):"flags");
}

#endif