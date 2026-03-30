#include <efi.h>
#include <intel/vmx.h>
#include <print.h>
#include <vm_backend.h>

static EFI_BOOT_SERVICES* BootServices = NULL;

int intel_init_backend(EFI_SYSTEM_TABLE* SystemTable)
{
    if (!intel_check_vmx_support())
    {
        return 0;
    }

    BootServices = SystemTable->BootServices;

    print(L"Intel VMX is supported and turned on!\r\n");

    EFI_PHYSICAL_ADDRESS vmxon_area = 0;
    
    if (BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &vmxon_area) != EFI_SUCCESS)
    {
        print(L"Failed to allocate a page for VMXON area\r\n");
        return 0;
    }
    
    if (!intel_enter_vmx_operation(vmxon_area))
    {
        BootServices->FreePages(vmxon_area, 1);
        return 0;
    }



    return 1;
}


struct vm_backend intel_vm_backend = {
    .init = intel_init_backend
};
