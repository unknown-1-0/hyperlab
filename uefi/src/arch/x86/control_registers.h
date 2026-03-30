#ifndef _ARCH_X86_CONTROL_REGISTERS_H_
#define _ARCH_X86_CONTROL_REGISTERS_H_

#include <stdint.h>

static inline uint64_t __attribute__((always_inline)) read_cr0(void)
{
    uint64_t value;
    __asm__("mov %0, cr0":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_cr0(uint64_t value)
{
    __asm__("mov cr0, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_cr2(void)
{
    uint64_t value;
    __asm__("mov %0, cr2":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_cr2(uint64_t value)
{
    __asm__("mov cr2, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_cr3(void)
{
    uint64_t value;
    __asm__("mov %0, cr3":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_cr3(uint64_t value)
{
    __asm__("mov cr3, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_cr4(void)
{
    uint64_t value;
    __asm__("mov %0, cr4":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_cr4(uint64_t value)
{
    __asm__("mov cr4, %0"::"r"(value):"flags");
}

#endif