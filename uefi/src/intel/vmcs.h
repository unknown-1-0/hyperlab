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
        "setz %1\n":"=r"(carry),"=r"(zero):"rm"(*out_value),"r"(field):"flags");

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

#define VMCS_PINBASED_VM_EXEC_CTLS 0x4000

#define VMCS_PRIMARY_PROCBASED_VM_EXEC_CTLS 0x4002
#define PRIMARY_PROCBASED_VM_EXEC_CTLS_UNCONDITIONAL_IO_EXITING (1UL<<24)
#define PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_SECONDARY_CTLS (1UL<<31)

#define VMCS_EXCEPTION_BITMAP 0x4004
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

#endif