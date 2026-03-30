#include <arch/x86/msr.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <print.h>

#define MSR_IA32_VMX_PINBASED_CTLS 0x481
#define MSR_IA32_VMX_PROCBASED_CTLS 0x482
#define MSR_IA32_VMX_PROCBASED_CTLS2 0x48b
#define MSR_IA32_VMX_PROCBASED_CTLS3 0x492

#define VMWRITE(field, value) do \
{ \
    if (!vmwrite(field, value)) \
    { \
        /* TODO: Specify write of which field failed */ \
        print(L"VMCS: VMWRITE failed\r\n"); \
        return 0; \
    } \
} while(0)


int intel_vmcs_setup_control_fields(void)
{
    {
        uint64_t vmx_pinbased_ctls = rdmsr(MSR_IA32_VMX_PINBASED_CTLS);

        VMWRITE(VMCS_PINBASED_VM_EXEC_CTLS, vmx_pinbased_ctls & (vmx_pinbased_ctls >> 32));
    }

    {
        uint64_t primary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS);
        uint32_t result_value = (uint32_t)primary_procbased_ctls |
            PRIMARY_PROCBASED_VM_EXEC_CTLS_UNCONDITIONAL_IO_EXITING | PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_SECONDARY_CTLS;

        if ((result_value & (primary_procbased_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set valid value of primary processor-based VM-execution controls\r\n");
            return 0;
        }

        VMWRITE(VMCS_PRIMARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    {
        uint64_t secondary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS2);
        uint32_t result_value = (uint32_t)secondary_procbased_ctls | 
            SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_EPT;

        if ((result_value & (secondary_procbased_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set valid value of secondary processor-based VM-execution controls (EPT not supported?)\r\n");
            return 0;
        }

        VMWRITE(VMCS_SECONDARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    return 1;
}

int intel_vmcs_setup_host_state(void)
{
    return 0;
}

int intel_vmcs_setup_guest_state(void)
{
    return 0;
}

int intel_vmcs_setup_ept(void)
{
    return 0;
}

int intel_init_vmcs(EFI_PHYSICAL_ADDRESS vmcs)
{
    *(uint32_t*)vmcs = intel_vmx_get_vmcs_revision_id();

    if (!vmclear(vmcs))
    {
        print(L"VMCS: VMCLEAR failed\r\n");
        return 0;
    }

    if (!vmptrld(vmcs))
    {
        print(L"VMCS: VMPTRLD failed\r\n");
        return 0;
    }

    if (!intel_vmcs_setup_control_fields())
    {
        print(L"VMCS: Failed to set up control fields\r\n");
        return 0;
    }

    if (!intel_vmcs_setup_host_state())
    {
        print(L"VMCS: Failed to set up host state\r\n");
        return 0;
    }

    if (!intel_vmcs_setup_guest_state())
    {
        print(L"VMCS: Failed to set up guest state\r\n");
        return 0;
    }

    if (!intel_vmcs_setup_ept())
    {
        print(L"VMCS: Failed to set up EPT\r\n");
        return 0;
    }

    return 1;
}