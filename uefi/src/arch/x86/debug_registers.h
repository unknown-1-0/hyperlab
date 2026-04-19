#ifndef _ARCH_X86_DEBUG_REGISTERS_H_
#define _ARCH_X86_DEBUG_REGISTERS_H_

#include <stdint.h>

static inline uint64_t __attribute__((always_inline)) read_dr0(void)
{
    uint64_t value;
    __asm__("mov %0, dr0":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr0(uint64_t value)
{
    __asm__("mov dr0, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_dr1(void)
{
    uint64_t value;
    __asm__("mov %0, dr1":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr1(uint64_t value)
{
    __asm__("mov dr1, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_dr2(void)
{
    uint64_t value;
    __asm__("mov %0, dr2":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr2(uint64_t value)
{
    __asm__("mov dr2, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_dr3(void)
{
    uint64_t value;
    __asm__("mov %0, dr3":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr3(uint64_t value)
{
    __asm__("mov dr3, %0"::"r"(value):"flags");
}

static inline uint64_t __attribute__((always_inline)) read_dr6(void)
{
    uint64_t value;
    __asm__("mov %0, dr6":"=r"(value)::"flags");
    return value;
}

static inline void __attribute__((always_inline)) write_dr6(uint64_t value)
{
    __asm__("mov dr6, %0"::"r"(value):"flags");
}

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
