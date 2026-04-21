#include <arch/x86/control_registers.h>
#include <arch/x86/debug_registers.h>
#include <arch/x86/msr.h>
#include <efi.h>
#include <gdt.h>
#include <intel/ept.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <print.h>

#define MSR_IA32_VMX_PINBASED_CTLS 0x481
#define MSR_IA32_VMX_PROCBASED_CTLS 0x482
#define MSR_IA32_VMX_EXIT_CTLS 0x483
#define MSR_IA32_VMX_ENTRY_CTLS 0x484
#define MSR_IA32_VMX_PROCBASED_CTLS2 0x48b
#define MSR_IA32_VMX_PROCBASED_CTLS3 0x492

#define VMWRITE(field, value) do \
{ \
    uint64_t vmcs_field = (field); \
    uint64_t vmcs_field_value = (value); \
    if (!vmwrite(vmcs_field, vmcs_field_value)) \
    { \
        printf(L"VMCS: VMWRITE of value 0x%x to field 0x%x failed\r\n", vmcs_field_value, vmcs_field); \
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
            print(L"VMCS: Could not set desired value of primary processor-based VM-execution controls\r\n");
            return 0;
        }

        VMWRITE(VMCS_PRIMARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    {
        uint64_t secondary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS2);
        uint32_t result_value = (uint32_t)secondary_procbased_ctls |
            SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_EPT | SECONDARY_PROCBASED_VM_EXEC_CTLS_UNRESTRICTED_GUEST;

        if ((result_value & (secondary_procbased_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set desired value of secondary processor-based VM-execution controls (EPT not supported?)\r\n");
            return 0;
        }

        VMWRITE(VMCS_SECONDARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    {
        uint64_t vm_exit_ctls = rdmsr(MSR_IA32_VMX_EXIT_CTLS);

        uint32_t result_value = (uint32_t)vm_exit_ctls |
            PRIMARY_VM_EXIT_CTLS_SAVE_DEBUG_CTLS | PRIMARY_VM_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE |
            PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PAT | PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PAT |
            PRIMARY_VM_EXIT_CTLS_SAVE_IA32_EFER | PRIMARY_VM_EXIT_CTLS_LOAD_IA32_EFER |
            PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PERF_GLOBAL_CTL; // | PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PERF_GLOBAL_CTL;

        if ((result_value & (vm_exit_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set desired value of primary VM-exit controls\r\n");
            return 0;
        }

        VMWRITE(VMCS_PRIMARY_VM_EXIT_CTLS, result_value);
    }

    {
        uint64_t vm_entry_ctls = rdmsr(MSR_IA32_VMX_ENTRY_CTLS);

        uint32_t result_value = (uint32_t)vm_entry_ctls | VM_ENTRY_CTLS_LOAD_DEBUG_CTLS |
            VM_ENTRY_CTLS_IA32E_MODE_GUEST | VM_ENTRY_CTLS_LOAD_IA32_PERF_GLOBAL_CTRL |
            VM_ENTRY_CTLS_LOAD_IA32_PAT | VM_ENTRY_CTLS_LOAD_IA32_EFER;

        if ((result_value & (vm_entry_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set desired value of VM-entry controls\r\n");
            return 0;
        }

        VMWRITE(VMCS_VM_ENTRY_CTLS, result_value);
    }

    {
        VMWRITE(VMCS_EXCEPTION_BITMAP, 0xffffffffULL);
        VMWRITE(VMCS_PAGE_FAULT_ERROR_CODE_MASK, 0);
        VMWRITE(VMCS_PAGE_FAULT_ERROR_CODE_MATCH, 0);

        VMWRITE(VMCS_CR3_TARGET_COUNT, 0);
        // May fail
        vmwrite(VMCS_CR3_TARGET_VALUE_0, 0);
        vmwrite(VMCS_CR3_TARGET_VALUE_1, 0);
        vmwrite(VMCS_CR3_TARGET_VALUE_2, 0);
        vmwrite(VMCS_CR3_TARGET_VALUE_3, 0);

        VMWRITE(VMCS_VM_EXIT_MSR_STORE_COUNT, 0);
        VMWRITE(VMCS_VM_EXIT_MSR_LOAD_COUNT, 0);
        VMWRITE(VMCS_VM_ENTRY_MSR_LOAD_COUNT, 0);
        VMWRITE(VMCS_INJECTED_EVENT_ID, 0);
        VMWRITE(VMCS_INJECTED_EVENT_ERROR_CODE, 0);
        VMWRITE(VMCS_VM_ENTRY_INSTRUCTION_LENGTH, 0);

        VMWRITE(VMCS_CR0_READ_SHADOW, read_cr0());
        VMWRITE(VMCS_CR0_GUEST_HOST_MASK, 0);
        VMWRITE(VMCS_CR4_READ_SHADOW, read_cr4());
        VMWRITE(VMCS_CR4_GUEST_HOST_MASK, 0);
    }
    return 1;
}

#define MSR_IA32_FS_BASE 0xc0000100
#define MSR_IA32_GS_BASE 0xc0000101

#define MSR_IA32_SYSENTER_CS 0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176
#define MSR_IA32_PERF_GLOBAL_CTRL 0x38f
#define MSR_IA32_PAT 0x277
#define MSR_IA32_EFER 0xc0000080

#define EFER_LME (1ULL<<8)
#define EFER_LMA (1ULL<<10)
#define EFER_NXE (1ULL<<11)

int intel_vmcs_setup_host_state(void)
{
    VMWRITE(VMCS_HOST_CR0, read_cr0());
    VMWRITE(VMCS_HOST_CR3, read_cr3());
    VMWRITE(VMCS_HOST_CR4, read_cr4());

    VMWRITE(VMCS_HOST_FS_BASE, rdmsr(MSR_IA32_FS_BASE));
    VMWRITE(VMCS_HOST_GS_BASE, rdmsr(MSR_IA32_GS_BASE));

    {
        uint8_t gdtr[sizeof(uint16_t)+sizeof(size_t)];
        __asm__("sgdt %0":"=m"(gdtr)::"memory");

        size_t gdt_base = *(size_t*)(gdtr+sizeof(uint16_t));

        VMWRITE(VMCS_HOST_GDTR_BASE, gdt_base);
    }

    {
        uint8_t idtr[sizeof(uint16_t)+sizeof(size_t)];
        __asm__("sidt %0":"=m"(idtr)::"memory");

        size_t idt_base = *(size_t*)(idtr+sizeof(uint16_t));

        VMWRITE(VMCS_HOST_IDTR_BASE, idt_base);
    }

    VMWRITE(VMCS_HOST_TR_BASE, (size_t)(&new_tss));

    VMWRITE(VMCS_HOST_IA32_SYSENTER_CS, rdmsr(MSR_IA32_SYSENTER_CS));
    VMWRITE(VMCS_HOST_IA32_SYSENTER_ESP, rdmsr(MSR_IA32_SYSENTER_ESP));
    VMWRITE(VMCS_HOST_IA32_SYSENTER_EIP, rdmsr(MSR_IA32_SYSENTER_EIP));
    VMWRITE(VMCS_HOST_IA32_PERF_GLOBAL_CTRL, rdmsr(MSR_IA32_PERF_GLOBAL_CTRL));
    VMWRITE(VMCS_HOST_IA32_PAT, rdmsr(MSR_IA32_PAT));
    VMWRITE(VMCS_HOST_IA32_EFER, rdmsr(MSR_IA32_EFER));

    // Host RIP and RSP are to be determined on VM-entry

    uint16_t es, cs, ss, ds, fs, gs, tr;
    __asm__ volatile("mov %0, es":"=rm"(es));
    __asm__ volatile("mov %0, cs":"=rm"(cs));
    __asm__ volatile("mov %0, ss":"=rm"(ss));
    __asm__ volatile("mov %0, ds":"=rm"(ds));
    __asm__ volatile("mov %0, fs":"=rm"(fs));
    __asm__ volatile("mov %0, gs":"=rm"(gs));
    __asm__ volatile("str %0":"=rm"(tr));

    VMWRITE(VMCS_HOST_ES, es);
    VMWRITE(VMCS_HOST_CS, cs);
    VMWRITE(VMCS_HOST_SS, ss);
    VMWRITE(VMCS_HOST_DS, ds);
    VMWRITE(VMCS_HOST_FS, fs);
    VMWRITE(VMCS_HOST_GS, gs);
    VMWRITE(VMCS_HOST_TR, tr);

    return 1;
}

/* ! Do not forget to set valid values for the following fields !
    * GDTR base and limit
    * IDTR base and limit
    * CR3
    * RSP, RIP, RFLAGS
*/
int intel_vmcs_setup_guest_state(void)
{
    VMWRITE(VMCS_LINK_PTR, ~0ULL);

    VMWRITE(VMCS_GUEST_CR0, read_cr0());
    VMWRITE(VMCS_GUEST_CR3, 0);
    VMWRITE(VMCS_GUEST_CR4, read_cr4());
    VMWRITE(VMCS_GUEST_DR7, (1ULL<<10));

    VMWRITE(VMCS_GUEST_ES_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_CS_SELECTOR, 0x8);
    VMWRITE(VMCS_GUEST_SS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_DS_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_FS_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_GS_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_LDTR_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_TR_SELECTOR, 0);

    VMWRITE(VMCS_GUEST_ES_BASE, 0);
    VMWRITE(VMCS_GUEST_CS_BASE, 0);
    VMWRITE(VMCS_GUEST_SS_BASE, 0);
    VMWRITE(VMCS_GUEST_DS_BASE, 0);
    VMWRITE(VMCS_GUEST_FS_BASE, 0);
    VMWRITE(VMCS_GUEST_GS_BASE, 0);
    VMWRITE(VMCS_GUEST_LDTR_BASE, 0);
    VMWRITE(VMCS_GUEST_TR_BASE, 0);
    VMWRITE(VMCS_GUEST_GDTR_BASE, 0);
    VMWRITE(VMCS_GUEST_IDTR_BASE, 0);

    VMWRITE(VMCS_GUEST_ES_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_CS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_SS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_DS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_FS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_GS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_LDTR_LIMIT, 0);
    VMWRITE(VMCS_GUEST_TR_LIMIT, 0);
    VMWRITE(VMCS_GUEST_GDTR_LIMIT, 0);
    VMWRITE(VMCS_GUEST_IDTR_LIMIT, 0);

    VMWRITE(VMCS_GUEST_ES_ACCESS_RIGHTS,
        ACCESS_RIGHTS_WRITABLE_DATA_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY | ACCESS_RIGHTS_32BIT_SEGMENT);

    VMWRITE(VMCS_GUEST_CS_ACCESS_RIGHTS,
        ACCESS_RIGHTS_64BIT_READABLE_CODE_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY);

    VMWRITE(VMCS_GUEST_SS_ACCESS_RIGHTS,
        ACCESS_RIGHTS_WRITABLE_DATA_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY | ACCESS_RIGHTS_32BIT_SEGMENT);

    VMWRITE(VMCS_GUEST_DS_ACCESS_RIGHTS,
        ACCESS_RIGHTS_WRITABLE_DATA_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY | ACCESS_RIGHTS_32BIT_SEGMENT);

    VMWRITE(VMCS_GUEST_FS_ACCESS_RIGHTS,
        ACCESS_RIGHTS_WRITABLE_DATA_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY | ACCESS_RIGHTS_32BIT_SEGMENT);

    VMWRITE(VMCS_GUEST_GS_ACCESS_RIGHTS,
        ACCESS_RIGHTS_WRITABLE_DATA_SEGMENT | ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_GRANULARITY | ACCESS_RIGHTS_32BIT_SEGMENT);

    VMWRITE(VMCS_GUEST_LDTR_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE);
    
    VMWRITE(VMCS_GUEST_TR_ACCESS_RIGHTS,
        ACCESS_RIGHTS_DPL(0) | ACCESS_RIGHTS_PRESENT | ACCESS_RIGHTS_64BIT_BUSY_TASK_STATE_SEGMENT);

    VMWRITE(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
    VMWRITE(VMCS_GUEST_ACTIVITY_STATE, GUEST_ACTIVITY_STATE_ACTIVE);
    VMWRITE(VMCS_GUEST_INTERRUPTIBILITY_STATE, 0);


    VMWRITE(VMCS_GUEST_IA32_SYSENTER_CS, 0);
    VMWRITE(VMCS_GUEST_IA32_SYSENTER_ESP, 0);
    VMWRITE(VMCS_GUEST_IA32_SYSENTER_EIP, 0);
    VMWRITE(VMCS_GUEST_IA32_DEBUGCTL, 0);
    VMWRITE(VMCS_GUEST_IA32_EFER, EFER_LME | EFER_LMA | EFER_NXE);
    // TODO: Hard-code the default PAT value instead of using the host one
    VMWRITE(VMCS_GUEST_IA32_PAT, rdmsr(MSR_IA32_PAT));
    VMWRITE(VMCS_GUEST_IA32_PERF_GLOBAL_CTRL, 0);

    // ! Do not forget to change these to valid values !
    VMWRITE(VMCS_GUEST_RSP, 0);
    VMWRITE(VMCS_GUEST_RIP, 0);
    VMWRITE(VMCS_GUEST_RFLAGS, 2);

    // Fails in KVM
    vmwrite(VMCS_GUEST_SMBASE, 0);
    // Not all CPUs support Intel MPX, may fail
    vmwrite(VMCS_GUEST_IA32_BNDCFGS, 0);

    VMWRITE(VMCS_VMX_PREEMPTION_TIMER_VALUE, 0);

    VMWRITE(VMCS_GUEST_PDPTE0, 0);
    VMWRITE(VMCS_GUEST_PDPTE1, 0);
    VMWRITE(VMCS_GUEST_PDPTE2, 0);
    VMWRITE(VMCS_GUEST_PDPTE3, 0);
    return 1;
}

int intel_vmcs_setup_ept(void)
{
    return intel_ept_initialize();
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

