#include <arch/x86/control_registers.h>
#include <arch/x86/debug_registers.h>
#include <efi.h>
#include <intel/ept.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <print.h>
#include <vm_backend.h>

EFI_BOOT_SERVICES* BootServices = NULL;

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

    if (BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &cur_vmcs) != EFI_SUCCESS)
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

    int ret = intel_do_enter_vm(context->registers, out_context->registers);

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
