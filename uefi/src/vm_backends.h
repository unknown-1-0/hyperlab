#ifndef _VM_BACKENDS_H_
#define _VM_BACKENDS_H_

#include <efi.h>
#include <vm_backend.h>

struct vm_backend_entry
{
    CHAR16 vendor_name[8];
    uint32_t cpuid_0h_ebx;
    uint32_t cpuid_0h_ecx;
    uint32_t cpuid_0h_edx;
    struct vm_backend* backend;
};

extern struct vm_backend_entry vendor_specific_vm_backends[1];

#endif
