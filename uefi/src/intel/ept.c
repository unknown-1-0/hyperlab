#include <arch/x86/msr.h>
#include <efi.h>
#include <intel/ept.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <halt.h>
#include <print.h>

int intel_ept_alloc_page(EFI_PHYSICAL_ADDRESS* out_addr)
{
    EFI_PHYSICAL_ADDRESS addr = 0;
    EFI_STATUS status = BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &addr);

    if (status != EFI_SUCCESS)
    {
        printf(L"EPT: Could not allocate a new page, status = 0x%x\r\n", status);
        *out_addr = 0;
        return 0;
    }

    BootServices->SetMem((void*)addr, 0x1000, 0);
    *out_addr = addr;
    return 1;
}

void intel_ept_free_page(EFI_PHYSICAL_ADDRESS addr)
{
    EFI_STATUS status = BootServices->FreePages(addr, 1);

    if (status == EFI_SUCCESS)
    {
        return;
    }

    printf(L"BUG: Invalid page free detected! FreePages(0x%x, 1) returned 0x%x!\r\n", addr, status);
    halt();
}

#define MSR_IA32_VMX_EPT_VPID_CAP 0x48c
#define VMX_EPT_VPID_CAP_EPT_PAGE_WALK_LENGTH_4_SUPPORTED (1ULL<<6)
#define VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_UC_SUPPORTED (1ULL<<8)
#define VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_WB_SUPPORTED (1ULL<<14)
#define VMX_EPT_VPID_CAP_EPT_ACCESSED_DIRTY_FLAGS_SUPPORTED (1ULL<<21)

static EFI_PHYSICAL_ADDRESS ept_root = 0;

int intel_ept_initialize(void)
{
    uint64_t vmx_ept_vpid_cap = rdmsr(MSR_IA32_VMX_EPT_VPID_CAP);

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_PAGE_WALK_LENGTH_4_SUPPORTED) == 0)
    {
        print(L"EPT: Page walk length of 4 is not supported!\r\n");
        return 0;
    }

    uint64_t ept_ptr = EPT_PTR_PAGE_WALK_LENGTH(4);

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_WB_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_MEMORY_TYPE_WB;
        print(L"EPT: Using WB as EPT memory type\r\n");
    }
    else if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_UC_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_MEMORY_TYPE_UC;
        print(L"EPT: Using UC as EPT memory type\r\n");
    }
    else
    {
        print(L"EPT: Cannot determine the supported EPT memory type\r\n");
        return 0;
    }

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_ACCESSED_DIRTY_FLAGS_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_ENABLE_ACCESSED_DIRTY_FLAGS;
        print(L"EPT: Accessed and dirty flags enabled\r\n");
    }

    if (!intel_ept_alloc_page(&ept_root))
    {
        print(L"EPT: Could not allocate a new page for root EPT table\r\n");
        ept_root = 0;
        return 0;
    }

    ept_ptr |= ept_root;

    if (!vmwrite(VMCS_EPT_PTR, ept_ptr))
    {
        print(L"EPT: Could not write EPT pointer to VMCS\r\n");
        intel_ept_free_page(ept_root);
        ept_root = 0;
        return 0;
    }
    return 1;
}
