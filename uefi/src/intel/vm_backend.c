#include <efi.h>
#include <intel/vmx.h>
#include <print.h>
#include <vm_backend.h>

int intel_init_backend(EFI_SYSTEM_TABLE* SystemTable)
{
    if (!intel_check_vmx_support())
    {
        return 1;
    }

    print(L"Intel VMX is supported and turned on!\r\n");
    (void)SystemTable;

    return 0;
}


struct vm_backend intel_vm_backend = {
    .init = intel_init_backend
};
