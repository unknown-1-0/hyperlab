#ifndef _INTEL_VMX_H_
#define _INTEL_VMX_H_

#include <efi.h>
#include <stdint.h>

int intel_check_vmx_support(void);
int intel_enter_vmx_operation(EFI_PHYSICAL_ADDRESS vmxon_area);
uint32_t intel_vmx_get_vmcs_revision_id(void);

static inline uint8_t __attribute__((always_inline)) vmxon(EFI_PHYSICAL_ADDRESS vmxon_area)
{
    uint8_t carry;
    __asm__(
        "vmxon %1\n"
        "setc %0\n"
    :"=r"(carry):"m"(vmxon_area):"flags");

    return carry == 0;
}

extern EFI_BOOT_SERVICES* BootServices;

#endif
