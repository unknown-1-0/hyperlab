#ifndef _VM_CONTEXT_H_
#define _VM_CONTEXT_H_

#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7

#ifndef __ASSEMBLER__

#include <stdint.h>

struct segment_register
{
    size_t base;
    uint32_t limit;
    uint32_t attributes;
    uint16_t selector;
};

struct gdtr_idtr
{
    size_t base;
    uint16_t limit;
};

#define EFER_LME (1ULL<<8)
#define EFER_LMA (1ULL<<10)
#define EFER_NXE (1ULL<<11)

// SUBJECT TO CHANGE
struct vm_context
{
    struct segment_register es;
    struct segment_register cs;
    struct segment_register ss;
    struct segment_register ds;
    struct segment_register fs;
    struct segment_register gs;
    struct segment_register ldtr;
    struct segment_register tr;
    struct gdtr_idtr gdtr;
    struct gdtr_idtr idtr;

    uint64_t registers[16];
    uint64_t rip;
    uint64_t rflags;

    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;
//    uint64_t cr8;
    uint64_t xcr0;
    uint64_t dr0;
    uint64_t dr1;
    uint64_t dr2;
    uint64_t dr3;
    uint64_t dr6;
    uint64_t dr7;

    uint64_t pdpte[4];

    uint64_t efer;
    uint64_t pat;
    uint64_t debugctl;
    uint64_t sysenter_cs;
    uint64_t sysenter_esp;
    uint64_t sysenter_eip;
    uint64_t smbase;
    uint64_t bndcfgs;
    uint64_t perf_global_ctrl;

    uint32_t activity_state;
    uint32_t interruptibility_state;

    uint32_t xsave_state_area_size;
    void* xsave_state_area;
};

#endif

#endif
