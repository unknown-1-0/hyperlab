#ifndef _ARCH_X86_MSR_H_
#define _ARCH_X86_MSR_H_

#include <stdint.h>

static inline uint64_t __attribute((always_inline)) rdmsr(uint32_t msr)
{
    uint32_t high, low;
    __asm__ volatile("rdmsr":"=a"(low),"=d"(high):"c"(msr));
    return (uint64_t)low | ((uint64_t)high << 32);
}

static inline void __attribute__((always_inline)) wrmsr(uint32_t msr, uint64_t value)
{
    __asm__ volatile("wrmsr"::"a"(value & 0xffFFffFF),"d"(value>>32),"c"(msr));
}

#endif
