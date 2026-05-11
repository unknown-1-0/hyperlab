#include <arch/x86/control_registers.h>
#include <arch/x86/debug_registers.h>
#include <efi.h>
#include <intel/ept.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <print.h>
#include <vm_backend.h>

EFI_BOOT_SERVICES* BootServices = NULL;
static uint32_t xsave_state_area_size = 0;
static void* host_xsave_state_area_unaligned = NULL;
static void* host_xsave_state_area = NULL;

int intel_setup_xsave(void)
{
    uint32_t xcr0_lower = 0xd;
    uint32_t xcr0_upper = 0;
    uint32_t xsave_supported_features_size = 0;
    __asm__("cpuid":"+a"(xcr0_lower),"=d"(xcr0_upper),"+c"(xsave_supported_features_size)::"rbx");

    __asm__("xsetbv"::"a"(xcr0_lower),"d"(xcr0_upper),"c"(0));

    uint32_t new_xcr0_lower = 0xd;
    uint32_t new_xcr0_upper = 0;
    uint32_t xsave_enabled_features_size = 0;
    uint32_t new_xsave_supported_features_size = 0;
    __asm__("cpuid":"+a"(new_xcr0_lower),"=d"(new_xcr0_upper),"+c"(new_xsave_supported_features_size),"=b"(xsave_enabled_features_size));

    if (new_xcr0_lower != xcr0_lower || new_xcr0_upper != xcr0_upper)
    {
        uint64_t old_max_xcr0 = xcr0_lower | (uint64_t)xcr0_upper << 32;
        uint64_t new_max_xcr0 = new_xcr0_lower | (uint64_t)new_xcr0_upper << 32;
        print(L"XCR0 allowed values mismatch!\r\n");
        printf(L"old: 0x%x new: 0x%x\r\n", old_max_xcr0, new_max_xcr0);
        goto xsave_unstable;
    }

    if (xsave_supported_features_size != new_xsave_supported_features_size)
    {
        printf(L"XSAVE supported features size mismatch after XSETBV: old = 0x%x, new = 0x%x\r\n",
                xsave_supported_features_size, new_xsave_supported_features_size);
        goto xsave_unstable;
    }

    if (xsave_enabled_features_size != new_xsave_supported_features_size)
    {
        printf(L"XSAVE supported features size (0x%x) != XSAVE enabled features size (0x%x) after XSETBV!\r\n",
                new_xsave_supported_features_size, xsave_enabled_features_size);
        goto xsave_unstable;
    }

    xsave_state_area_size = xsave_enabled_features_size;
    host_xsave_state_area_unaligned = NULL;

    EFI_STATUS status = BootServices->AllocatePool(EfiLoaderData, xsave_state_area_size+0x40, &host_xsave_state_area_unaligned);

    if (status != EFI_SUCCESS)
    {
        printf(L"Could not allocate memory for host XSAVE state area, status = 0x%x\r\n", status);
        return 0;
    }

    BootServices->SetMem(host_xsave_state_area_unaligned, xsave_state_area_size+0x40, 0);
    host_xsave_state_area = (void*)(((size_t)host_xsave_state_area_unaligned + 0x40 - 1) & ~0x3fULL);
    return 1;

xsave_unstable:
    print(L"XSAVE state area info is unstable, cannot reliably continue\r\n");
    return 0;
}


int intel_init_backend(EFI_SYSTEM_TABLE* SystemTable)
{
    if (!intel_check_vmx_support())
    {
        return 0;
    }

    BootServices = SystemTable->BootServices;

    print(L"Intel VMX is supported and turned on!\r\n");

    EFI_PHYSICAL_ADDRESS vmxon_area = 0;

    if (BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &vmxon_area) != EFI_SUCCESS)
    {
        print(L"Failed to allocate a page for VMXON area\r\n");
        return 0;
    }

    if (!intel_enter_vmx_operation(vmxon_area))
    {
        BootServices->FreePages(vmxon_area, 1);
        return 0;
    }
    // intel_enter_vmx_operation sets up neccessary CR4 flags in the process
    // so it is necessary for this function to be called after it
    if (!intel_setup_xsave())
    {
        print(L"Could not setup XSAVE\r\n");
        return 0;
    }

    return 1;
}

static EFI_PHYSICAL_ADDRESS cur_vmcs = 0;
static int vm_created = 0;
int intel_create_vm(void)
{
    if (!BootServices)
    {
        print(L"Intel backend not initialized yet!\r\n");
        return 0;
    }

    if (vm_created)
    {
        print(L"VM is already created\r\n");
        return 0;
    }

    if (BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &cur_vmcs) != EFI_SUCCESS)
    {
        print(L"Failed to allocate a page for VMCS\r\n");
        return 0;
    }

    if (!intel_init_vmcs(cur_vmcs))
    {
        print(L"VMCS initialization failed\r\n");
        BootServices->FreePages(cur_vmcs, 1);
        return 0;
    }

    return 1;
}

int intel_do_enter_vm(uint64_t in_guest_general_purpose_regs[16], uint64_t out_guest_general_purpose_regs[16]);

int intel_enter_vm(struct vm_context* context, struct vm_context* out_context)
{
    if (context->xsave_state_area_size != xsave_state_area_size)
    {
        return 0;
    }

    if (out_context->xsave_state_area_size != xsave_state_area_size)
    {
        return 0;
    }

    // xsave64 and xrstor64 will raise #GP(0) if
    // pointers to xsave state area are not 64-byte aligned
    if ((size_t)context->xsave_state_area & 0x3fULL)
    {
        return 0;
    }

    if ((size_t)out_context->xsave_state_area & 0x3fULL)
    {
        return 0;
    }

    if (!intel_vmcs_load_guest_context(context))
    {
        return 0;
    }

    uint64_t host_cr2 = read_cr2();
    write_cr2(context->cr2);

    uint64_t host_dr[4];
    host_dr[0] = read_dr0();
    host_dr[1] = read_dr1();
    host_dr[2] = read_dr2();
    host_dr[3] = read_dr3();

    write_dr0(context->dr0);
    write_dr1(context->dr1);
    write_dr2(context->dr2);
    write_dr3(context->dr3);

    __asm__("xsave64 [%0]\n"
            "xrstor64 [%1]\n"::"r"(host_xsave_state_area),"r"(context->xsave_state_area):"memory");

    int ret = intel_do_enter_vm(context->registers, out_context->registers);

    __asm__("xsave64 [%0]\n"
            "xrstor64 [%1]\n"::"r"(out_context->xsave_state_area),"r"(host_xsave_state_area):"memory");

    out_context->cr2 = read_cr2();
    write_cr2(host_cr2);

    out_context->dr0 = read_dr0();
    out_context->dr1 = read_dr1();
    out_context->dr2 = read_dr2();
    out_context->dr3 = read_dr3();
    write_dr0(host_dr[0]);
    write_dr1(host_dr[1]);
    write_dr2(host_dr[2]);
    write_dr3(host_dr[3]);

    intel_vmcs_save_guest_context(out_context);
    return ret;
}

struct vm_backend intel_vm_backend = {
    .init = intel_init_backend,
    .enter_vm = intel_enter_vm,
    .create_vm = intel_create_vm,
    .map_page = intel_ept_map_page,
    .compute_segment_attributes = intel_vmcs_compute_segment_attributes
};
