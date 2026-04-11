#include <arch/x86/control_registers.h>
#include <arch/x86/gdt.h>
#include <gdt.h>
#include <stdint.h>

#define GDT_PRESENT (1UL<<15)
#define GDT_CODE_OR_DATA_SEGMENT (1UL<<12)
#define GDT_CODE_SEGMENT ((1UL<<11)|GDT_CODE_OR_DATA_SEGMENT)
#define GDT_LONG_CODE_SEGMENT ((1UL<<21)|GDT_CODE_SEGMENT)
#define GDT_32BIT_SEGMENT (1UL<<22)
#define GDT_GRANULARITY (1UL<<23)
#define GDT_32BIT_WRITABLE_DATA_SEGMENT ((1ULL<<9)|GDT_CODE_OR_DATA_SEGMENT|GDT_32BIT_SEGMENT)
#define GDT_TSS64_AVAILABLE ((1UL<<11)|(1UL<<8))

_Static_assert(sizeof(struct tss) == 104, "Task state struct packing failed");

const struct tss new_tss = {.io_map_base_address = sizeof(new_tss)-1};

// Make it look less ugly
#define TSS_BASE_ADDR ((size_t)(&new_tss))
#define GDT_MID_BASE_ADDR(addr) ((((addr) >> 16) & 0xffUL) | ((addr) & 0xff000000UL))

uint32_t gdt[10] = {
    // First entry must be all 0
    0, 0,
    // 0x8 - Kernel 64-bit code segment
    0xffff, (0xf << 16) | GDT_PRESENT | GDT_LONG_CODE_SEGMENT | GDT_GRANULARITY,
    // 0x10 - Kernel 32-bit data segment
    0xffff, (0xf << 16) | GDT_PRESENT | GDT_32BIT_WRITABLE_DATA_SEGMENT | GDT_GRANULARITY,
    // 0x18 - 64-bit TSS (to be filled)
    0,
    0,
    0,
    0
};

void x86_load_segments(uint16_t code_segment, uint16_t data_segment, uint16_t task_segment);

void setup_gdt_and_tss(void)
{
    uint32_t* gdt_tss_entry = &gdt[6];

    gdt_tss_entry[0] = (sizeof(new_tss) - 1) | (uint32_t)(TSS_BASE_ADDR << 16);
    gdt_tss_entry[1] = GDT_MID_BASE_ADDR(TSS_BASE_ADDR) | GDT_PRESENT | GDT_TSS64_AVAILABLE;
    gdt_tss_entry[2] = TSS_BASE_ADDR >> 32;
    gdt_tss_entry[3] = 0;

    uint8_t new_gdtr[sizeof(uint16_t)+sizeof(size_t)];
    *(uint16_t*)new_gdtr = sizeof(gdt) - 1;
    *(size_t*)(new_gdtr+sizeof(uint16_t)) = (size_t)gdt;

    __asm__("lgdt %0"::"m"(new_gdtr));

    x86_load_segments(0x8, 0x10, 0x18);
}