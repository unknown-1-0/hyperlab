#ifndef _ARCH_X86_IDT_H_
#define _ARCH_X86_IDT_H_

#include <stdint.h>

struct __attribute__((packed)) idt_entry
{
    uint16_t offset_low;
    uint16_t segment_selector;
    uint16_t flags;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
};

_Static_assert(sizeof(struct idt_entry) == 0x10, "IDT entry packing failed");

#define IDT_PRESENT (1UL<<15)
#define IDT_TRAP_GATE (0xfUL << 8)

#endif