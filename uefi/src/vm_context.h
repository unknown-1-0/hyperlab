#ifndef _VM_CONTEXT_H_
#define _VM_CONTEXT_H_

#include <stdint.h>

// SUBJECT TO CHANGE
struct vm_context
{
    uint32_t exception_intercept_mask;
    uint64_t registers[16];
    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;
    uint64_t cr8;
    uint64_t xcr0;
    uint64_t efer;
    uint64_t debugctl;
};

#endif
