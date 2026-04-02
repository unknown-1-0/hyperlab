#ifndef _ARCH_X86_GDT_H_
#define _ARCH_X86_GDT_H_
/*
#include <stdint.h>

struct __attribute__((packed)) gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t flags;
    uint8_t limit_high_and_flags;
    uint8_t base_upper;
};

struct __attribute__((packed)) gdt_system_descriptor_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle1;
    uint8_t flags;
    uint8_t limit_high_and_flags;
    uint8_t base_middle2;
    uint32_t base_upper;
    uint32_t reserved;
};

_Static_assert(sizeof(struct gdt_entry) == 8, "GDT entry packing failed");
_Static_assert(sizeof(struct gdt_system_descriptor_entry) == 16, "GDT system descriptor entry packing failed");
*/
void setup_gdt_and_tss(void);
void restore_gdt_and_tss(void);

#endif