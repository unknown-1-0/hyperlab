#ifndef _INTEL_VMX_H_
#define _INTEL_VMX_H_

#include <efi.h>
#include <stdint.h>

int intel_check_vmx_support(void);
int intel_enter_vmx_operation(EFI_PHYSICAL_ADDRESS vmxon_area);
uint32_t intel_vmx_get_vmcs_revision_id(void);

#endif
