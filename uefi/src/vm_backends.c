#include "intel/vm_backend.h"
#include "vm_backends.h"

struct vm_backend_entry vendor_specific_vm_backends[1] = {
    {
        .vendor_name = L"Intel", 
        .cpuid_0h_ebx = 0x756e6547,
        .cpuid_0h_ecx = 0x6c65746e,
        .cpuid_0h_edx = 0x49656e69,
        .backend = &intel_vm_backend
    }
};

