#ifndef _INTEL_EPT_H_
#define _INTEL_EPT_H_

#include <efi.h>

int intel_ept_alloc_page(EFI_PHYSICAL_ADDRESS* out_addr);
int intel_ept_initialize(void);

#endif
