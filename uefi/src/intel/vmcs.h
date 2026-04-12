#ifndef _INTEL_VMCS_H_
#define _INTEL_VMCS_H_

#include <efi.h>
#include <stdint.h>

static inline uint8_t __attribute__((always_inline)) vmclear(EFI_PHYSICAL_ADDRESS vmcs)
{
    uint8_t carry;
    uint8_t zero;
    __asm__("vmclear %2\n"
        "setc %0\n"
        "setz %1\n":"=r"(carry),"=r"(zero):"m"(vmcs):"flags");

    return ((zero | carry) == 0);
}

static inline uint8_t __attribute__((always_inline)) vmptrld(EFI_PHYSICAL_ADDRESS vmcs)
{
    uint8_t carry;
    uint8_t zero;
    __asm__(
        "vmptrld %2\n"
        "setc %0\n"
        "setz %1\n":"=r"(carry),"=r"(zero):"m"(vmcs):"flags");

    return ((zero | carry) == 0);
}

static inline uint8_t __attribute__((always_inline)) vmptrst(EFI_PHYSICAL_ADDRESS* out_vmcs)
{
    uint8_t carry;
    uint8_t zero;
    __asm__(
        "vmptrst %2\n"
        "setc %0\n"
        "setz %1\n":"=r"(carry),"=r"(zero):"m"(*out_vmcs):"flags");

    return ((zero | carry) == 0);
}

static inline uint8_t __attribute__((always_inline)) vmread(uint64_t field, uint64_t* out_value)
{
    uint8_t carry;
    uint8_t zero;

    __asm__(
        "vmread %2, %3\n"
        "setc %0\n"
        "setz %1\n":"=r"(carry),"=r"(zero),"=rm"(*out_value):"r"(field):"flags");

    return ((zero | carry) == 0);
}

static inline uint8_t __attribute__((always_inline)) vmwrite(uint64_t field, uint64_t value)
{
    uint8_t carry;
    uint8_t zero;

    __asm__(
        "vmwrite %2, %3\n"
        "setc %0\n"
        "setz %1\n":"=r"(carry),"=r"(zero):"r"(field),"rm"(value):"flags");

    return ((zero | carry) == 0);
}

int intel_init_vmcs(EFI_PHYSICAL_ADDRESS vmcs);

#define VMCS_HOST_ES 0xc00
#define VMCS_HOST_CS 0xc02
#define VMCS_HOST_SS 0xc04
#define VMCS_HOST_DS 0xc06
#define VMCS_HOST_FS 0xc08
#define VMCS_HOST_GS 0xc0a
#define VMCS_HOST_TR 0xc0c

#define VMCS_HOST_IA32_PAT 0x2c00
#define VMCS_HOST_IA32_EFER 0x2c02
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL 0x2c04

#define VMCS_PINBASED_VM_EXEC_CTLS 0x4000

#define VMCS_PRIMARY_PROCBASED_VM_EXEC_CTLS 0x4002
#define PRIMARY_PROCBASED_VM_EXEC_CTLS_UNCONDITIONAL_IO_EXITING (1UL<<24)
#define PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_SECONDARY_CTLS (1UL<<31)

#define VMCS_EXCEPTION_BITMAP 0x4004
#define VMCS_CR3_TARGET_COUNT 0x400a

#define VMCS_PRIMARY_VM_EXIT_CTLS 0x400c
#define PRIMARY_VM_EXIT_CTLS_SAVE_DEBUG_CTLS (1UL<<2)
#define PRIMARY_VM_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE (1UL<<9)
#define PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PERF_GLOBAL_CTL (1UL<<12)
#define PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PAT (1UL<<18)
#define PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PAT (1UL<<19)
#define PRIMARY_VM_EXIT_CTLS_SAVE_IA32_EFER (1UL<<20)
#define PRIMARY_VM_EXIT_CTLS_LOAD_IA32_EFER (1UL<<21)
#define PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PERF_GLOBAL_CTL (1UL<<30)

#define VMCS_VM_EXIT_MSR_STORE_COUNT 0x400e
#define VMCS_VM_EXIT_MSR_LOAD_COUNT 0x4010

#define VMCS_VM_ENTRY_CTLS 0x4012
#define VM_ENTRY_CTLS_LOAD_DEBUG_CTLS (1UL<<2)
#define VM_ENTRY_CTLS_IA32E_MODE_GUEST (1UL<<9)
#define VM_ENTRY_CTLS_LOAD_IA32_PERF_GLOBAL_CTRL (1UL<<13)
#define VM_ENTRY_CTLS_LOAD_IA32_PAT (1UL<<14)
#define VM_ENTRY_CTLS_LOAD_IA32_EFER (1UL<<15)


#define VMCS_VM_ENTRY_MSR_LOAD_COUNT 0x4014
#define VMCS_SECONDARY_PROCBASED_VM_EXEC_CTLS 0x401e
#define SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_EPT (1UL<<1)
#define SECONDARY_PROCBASED_VM_EXEC_CTLS_UNRESTRICTED_GUEST (1UL<<7)

#define VMCS_VM_INSTRUCTION_ERROR 0x4400
#define VMCS_EXIT_REASON 0x4402
#define VMCS_EXITING_EVENT_ID 0x4404
#define VMCS_EXITING_EVENT_ERROR_CODE 0x4406
#define VMCS_ORIGINAL_EVENT_ID 0x4408
#define VMCS_ORIGINAL_EVENT_ERROR_CODE 0x440a
#define VMCS_VM_EXIT_INSTRUCTION_LENGTH 0x440c
#define VMCS_VM_EXIT_INSTRUCTION_INFO 0x440e

#define VMCS_HOST_IA32_SYSENTER_CS 0x4c00

#define VMCS_HOST_CR0 0x6c00
#define VMCS_HOST_CR3 0x6c02
#define VMCS_HOST_CR4 0x6c04
#define VMCS_HOST_FS_BASE 0x6c06
#define VMCS_HOST_GS_BASE 0x6c08
#define VMCS_HOST_TR_BASE 0x6c0a
#define VMCS_HOST_GDTR_BASE 0x6c0c
#define VMCS_HOST_IDTR_BASE 0x6c0e
#define VMCS_HOST_IA32_SYSENTER_ESP 0x6c10
#define VMCS_HOST_IA32_SYSENTER_EIP 0x6c12
#define VMCS_HOST_RSP 0x6c14
#define VMCS_HOST_RIP 0x6c16

#endif