#include <arch/x86/control_registers.h>
#include <arch/x86/debug_registers.h>
#include <arch/x86/msr.h>
#include <efi.h>
#include <gdt.h>
#include <intel/ept.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <print.h>
#include <vm_backend.h>
#include <vm_context.h>

#define MSR_IA32_VMX_PINBASED_CTLS 0x481
#define MSR_IA32_VMX_PROCBASED_CTLS 0x482
#define MSR_IA32_VMX_EXIT_CTLS 0x483
#define MSR_IA32_VMX_ENTRY_CTLS 0x484
#define MSR_IA32_VMX_PROCBASED_CTLS2 0x48b
#define MSR_IA32_VMX_PROCBASED_CTLS3 0x492

#define VMREAD(field, value) do \
{ \
    uint64_t vmcs_field = (field); \
    void* vmcs_field_value_ptr = &(value); \
    if (!vmread(vmcs_field, vmcs_field_value_ptr)) \
    { \
        printf(L"VMCS: VMREAD of field 0x%x failed\r\n", vmcs_field); \
        return 0; \
    } \
} while(0)

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

    int tertiary_ctls_available = 0;
    {
        uint64_t primary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS);
        uint32_t result_value = (uint32_t)primary_procbased_ctls |
            PRIMARY_PROCBASED_VM_EXEC_CTLS_UNCONDITIONAL_IO_EXITING |
            PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_SECONDARY_CTLS;

        if ((result_value & (primary_procbased_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set desired value of primary processor-based VM-execution controls\r\n");
            return 0;
        }

        if (((primary_procbased_ctls >> 32) & PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_TERTIARY_CTLS) != 0)
        {
            result_value |= PRIMARY_PROCBASED_VM_EXEC_CTLS_ACTIVATE_TERTIARY_CTLS;
            tertiary_ctls_available = 1;
        }

        VMWRITE(VMCS_PRIMARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    {
        uint64_t secondary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS2);
        uint32_t result_value = (uint32_t)secondary_procbased_ctls |
            SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_EPT | SECONDARY_PROCBASED_VM_EXEC_CTLS_UNRESTRICTED_GUEST;

        if ((result_value & (secondary_procbased_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not enable neccessary features in secondary processor-based VM-execution controls (EPT or unrestricted guest not supported?)\r\n");
            return 0;
        }

        result_value |= SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_RDTSCP | SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_INVPCID |
            SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_XSAVES_XRSTORS | SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_USER_WAIT_AND_PAUSE |
            SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_PCONFIG;

        result_value &= (secondary_procbased_ctls >> 32);

        VMWRITE(VMCS_SECONDARY_PROCBASED_VM_EXEC_CTLS, result_value);
    }

    if (tertiary_ctls_available)
    {
        uint64_t tertiary_procbased_ctls = rdmsr(MSR_IA32_VMX_PROCBASED_CTLS3);

        uint64_t result_value = TERTIARY_PROCBASED_VM_EXEC_CTLS_ENABLE_MSR_LIST_INSTRUCTIONS |
            TERTIARY_PROCBASED_VM_EXEC_CTLS_ENABLE_PBNDKB;

        result_value &= tertiary_procbased_ctls;

        VMWRITE(VMCS_TERTIARY_PROCBASED_VM_EXEC_CTLS, result_value);
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

// Will be set up during intel_vmcs_load_guest_context
/*    VMWRITE(VMCS_GUEST_CR0, read_cr0());
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
    VMWRITE(VMCS_GUEST_PDPTE3, 0);*/
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

int intel_vmcs_load_guest_context(struct vm_context* context)
{
    VMWRITE(VMCS_GUEST_CR0, context->cr0);
    VMWRITE(VMCS_GUEST_CR3, context->cr3);
    VMWRITE(VMCS_GUEST_CR4, context->cr4);
    VMWRITE(VMCS_GUEST_DR7, context->dr7);

    VMWRITE(VMCS_GUEST_ES_SELECTOR, context->es.selector);
    VMWRITE(VMCS_GUEST_CS_SELECTOR, context->cs.selector);
    VMWRITE(VMCS_GUEST_SS_SELECTOR, context->ss.selector);
    VMWRITE(VMCS_GUEST_DS_SELECTOR, context->ds.selector);
    VMWRITE(VMCS_GUEST_FS_SELECTOR, context->fs.selector);
    VMWRITE(VMCS_GUEST_GS_SELECTOR, context->gs.selector);
    VMWRITE(VMCS_GUEST_LDTR_SELECTOR, context->ldtr.selector);
    VMWRITE(VMCS_GUEST_TR_SELECTOR, context->tr.selector);

    VMWRITE(VMCS_GUEST_ES_BASE, context->es.base);
    VMWRITE(VMCS_GUEST_CS_BASE, context->cs.base);
    VMWRITE(VMCS_GUEST_SS_BASE, context->ss.base);
    VMWRITE(VMCS_GUEST_DS_BASE, context->ds.base);
    VMWRITE(VMCS_GUEST_FS_BASE, context->fs.base);
    VMWRITE(VMCS_GUEST_GS_BASE, context->gs.base);
    VMWRITE(VMCS_GUEST_LDTR_BASE, context->ldtr.base);
    VMWRITE(VMCS_GUEST_TR_BASE, context->tr.base);
    VMWRITE(VMCS_GUEST_GDTR_BASE, context->gdtr.base);
    VMWRITE(VMCS_GUEST_IDTR_BASE, context->idtr.base);

    VMWRITE(VMCS_GUEST_ES_LIMIT, context->es.limit);
    VMWRITE(VMCS_GUEST_CS_LIMIT, context->cs.limit);
    VMWRITE(VMCS_GUEST_SS_LIMIT, context->ss.limit);
    VMWRITE(VMCS_GUEST_DS_LIMIT, context->ds.limit);
    VMWRITE(VMCS_GUEST_FS_LIMIT, context->fs.limit);
    VMWRITE(VMCS_GUEST_GS_LIMIT, context->gs.limit);
    VMWRITE(VMCS_GUEST_LDTR_LIMIT, context->ldtr.limit);
    VMWRITE(VMCS_GUEST_TR_LIMIT, context->tr.limit);
    VMWRITE(VMCS_GUEST_GDTR_LIMIT, context->gdtr.limit);
    VMWRITE(VMCS_GUEST_IDTR_LIMIT, context->idtr.limit);

    VMWRITE(VMCS_GUEST_ES_ACCESS_RIGHTS, context->es.attributes);
    VMWRITE(VMCS_GUEST_CS_ACCESS_RIGHTS, context->cs.attributes);
    VMWRITE(VMCS_GUEST_SS_ACCESS_RIGHTS, context->ss.attributes);
    VMWRITE(VMCS_GUEST_DS_ACCESS_RIGHTS, context->ds.attributes);
    VMWRITE(VMCS_GUEST_FS_ACCESS_RIGHTS, context->fs.attributes);
    VMWRITE(VMCS_GUEST_GS_ACCESS_RIGHTS, context->gs.attributes);
    VMWRITE(VMCS_GUEST_LDTR_ACCESS_RIGHTS, context->ldtr.attributes);
    VMWRITE(VMCS_GUEST_TR_ACCESS_RIGHTS, context->tr.attributes);

    uint64_t dr6_mask = (0xfULL | (3ULL << 11) | (1ULL << 14) | (1ULL << 16));
    uint64_t dr6_xor_mask = (1ULL<<11) | (1ULL<<16);

    VMWRITE(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, (context->dr6 ^ dr6_xor_mask) & dr6_mask);
    VMWRITE(VMCS_GUEST_ACTIVITY_STATE, context->activity_state);
    VMWRITE(VMCS_GUEST_INTERRUPTIBILITY_STATE, context->interruptibility_state);

    VMWRITE(VMCS_GUEST_IA32_SYSENTER_CS, context->sysenter_cs);
    VMWRITE(VMCS_GUEST_IA32_SYSENTER_ESP, context->sysenter_esp);
    VMWRITE(VMCS_GUEST_IA32_SYSENTER_EIP, context->sysenter_eip);
    VMWRITE(VMCS_GUEST_IA32_DEBUGCTL, context->debugctl);

    uint64_t efer = context->efer;
    VMWRITE(VMCS_GUEST_IA32_EFER, efer);

    uint64_t vm_entry_ctls = 0;
    VMREAD(VMCS_VM_ENTRY_CTLS, vm_entry_ctls);

    if (efer & EFER_LMA)
    {
        vm_entry_ctls |= VM_ENTRY_CTLS_IA32E_MODE_GUEST;
    }
    else
    {
        vm_entry_ctls &= ~VM_ENTRY_CTLS_IA32E_MODE_GUEST;
    }
    VMWRITE(VMCS_VM_ENTRY_CTLS, vm_entry_ctls);

    VMWRITE(VMCS_GUEST_IA32_PAT, context->pat);
    VMWRITE(VMCS_GUEST_IA32_PERF_GLOBAL_CTRL, context->perf_global_ctrl);

    // Guest RSP will be loaded in intel_do_enter_vm()
    VMWRITE(VMCS_GUEST_RIP, context->rip);
    VMWRITE(VMCS_GUEST_RFLAGS, context->rflags);

    // Fails in KVM
    vmwrite(VMCS_GUEST_SMBASE, context->smbase);
    // Not all CPUs support Intel MPX, may fail
    vmwrite(VMCS_GUEST_IA32_BNDCFGS, context->bndcfgs);

    VMWRITE(VMCS_VMX_PREEMPTION_TIMER_VALUE, 0);

    VMWRITE(VMCS_GUEST_PDPTE0, context->pdpte[0]);
    VMWRITE(VMCS_GUEST_PDPTE1, context->pdpte[1]);
    VMWRITE(VMCS_GUEST_PDPTE2, context->pdpte[2]);
    VMWRITE(VMCS_GUEST_PDPTE3, context->pdpte[3]);
    return 1;
}

int intel_vmcs_save_guest_context(struct vm_context* context)
{
    VMREAD(VMCS_GUEST_CR0, context->cr0);
    VMREAD(VMCS_GUEST_CR3, context->cr3);
    VMREAD(VMCS_GUEST_CR4, context->cr4);
    VMREAD(VMCS_GUEST_DR7, context->dr7);

    VMREAD(VMCS_GUEST_ES_SELECTOR, context->es.selector);
    VMREAD(VMCS_GUEST_CS_SELECTOR, context->cs.selector);
    VMREAD(VMCS_GUEST_SS_SELECTOR, context->ss.selector);
    VMREAD(VMCS_GUEST_DS_SELECTOR, context->ds.selector);
    VMREAD(VMCS_GUEST_FS_SELECTOR, context->fs.selector);
    VMREAD(VMCS_GUEST_GS_SELECTOR, context->gs.selector);
    VMREAD(VMCS_GUEST_LDTR_SELECTOR, context->ldtr.selector);
    VMREAD(VMCS_GUEST_TR_SELECTOR, context->tr.selector);

    VMREAD(VMCS_GUEST_ES_BASE, context->es.base);
    VMREAD(VMCS_GUEST_CS_BASE, context->cs.base);
    VMREAD(VMCS_GUEST_SS_BASE, context->ss.base);
    VMREAD(VMCS_GUEST_DS_BASE, context->ds.base);
    VMREAD(VMCS_GUEST_FS_BASE, context->fs.base);
    VMREAD(VMCS_GUEST_GS_BASE, context->gs.base);
    VMREAD(VMCS_GUEST_LDTR_BASE, context->ldtr.base);
    VMREAD(VMCS_GUEST_TR_BASE, context->tr.base);
    VMREAD(VMCS_GUEST_GDTR_BASE, context->gdtr.base);
    VMREAD(VMCS_GUEST_IDTR_BASE, context->idtr.base);

    VMREAD(VMCS_GUEST_ES_LIMIT, context->es.limit);
    VMREAD(VMCS_GUEST_CS_LIMIT, context->cs.limit);
    VMREAD(VMCS_GUEST_SS_LIMIT, context->ss.limit);
    VMREAD(VMCS_GUEST_DS_LIMIT, context->ds.limit);
    VMREAD(VMCS_GUEST_FS_LIMIT, context->fs.limit);
    VMREAD(VMCS_GUEST_GS_LIMIT, context->gs.limit);
    VMREAD(VMCS_GUEST_LDTR_LIMIT, context->ldtr.limit);
    VMREAD(VMCS_GUEST_TR_LIMIT, context->tr.limit);
    VMREAD(VMCS_GUEST_GDTR_LIMIT, context->gdtr.limit);
    VMREAD(VMCS_GUEST_IDTR_LIMIT, context->idtr.limit);

    VMREAD(VMCS_GUEST_ES_ACCESS_RIGHTS, context->es.attributes);
    VMREAD(VMCS_GUEST_CS_ACCESS_RIGHTS, context->cs.attributes);
    VMREAD(VMCS_GUEST_SS_ACCESS_RIGHTS, context->ss.attributes);
    VMREAD(VMCS_GUEST_DS_ACCESS_RIGHTS, context->ds.attributes);
    VMREAD(VMCS_GUEST_FS_ACCESS_RIGHTS, context->fs.attributes);
    VMREAD(VMCS_GUEST_GS_ACCESS_RIGHTS, context->gs.attributes);
    VMREAD(VMCS_GUEST_LDTR_ACCESS_RIGHTS, context->ldtr.attributes);
    VMREAD(VMCS_GUEST_TR_ACCESS_RIGHTS, context->tr.attributes);

    uint64_t dr6_xor_mask = (1ULL<<11) | (1ULL<<16);

    VMREAD(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, context->dr6);
    context->dr6 ^= dr6_xor_mask;

    VMREAD(VMCS_GUEST_ACTIVITY_STATE, context->activity_state);
    VMREAD(VMCS_GUEST_INTERRUPTIBILITY_STATE, context->interruptibility_state);

    VMREAD(VMCS_GUEST_IA32_SYSENTER_CS, context->sysenter_cs);
    VMREAD(VMCS_GUEST_IA32_SYSENTER_ESP, context->sysenter_esp);
    VMREAD(VMCS_GUEST_IA32_SYSENTER_EIP, context->sysenter_eip);
    VMREAD(VMCS_GUEST_IA32_DEBUGCTL, context->debugctl);
    VMREAD(VMCS_GUEST_IA32_EFER, context->efer);
    VMREAD(VMCS_GUEST_IA32_PAT, context->pat);
    VMREAD(VMCS_GUEST_IA32_PERF_GLOBAL_CTRL, context->perf_global_ctrl);

    // Guest RSP will be saved in intel_do_enter_vm()
    VMREAD(VMCS_GUEST_RIP, context->rip);
    VMREAD(VMCS_GUEST_RFLAGS, context->rflags);

    // Fails in KVM
    vmread(VMCS_GUEST_SMBASE, &context->smbase);
    // Not all CPUs support Intel MPX, may fail
    vmread(VMCS_GUEST_IA32_BNDCFGS, &context->bndcfgs);

    VMREAD(VMCS_GUEST_PDPTE0, context->pdpte[0]);
    VMREAD(VMCS_GUEST_PDPTE1, context->pdpte[1]);
    VMREAD(VMCS_GUEST_PDPTE2, context->pdpte[2]);
    VMREAD(VMCS_GUEST_PDPTE3, context->pdpte[3]);
    return 1;
}

uint32_t intel_vmcs_compute_segment_attributes(uint8_t dpl, uint8_t type, uint8_t flags)
{
    dpl &= 3;
    type &= (1U<<4)-1;

    uint32_t attributes = (dpl << 5) | type;

    if (flags & ATTR_CODE_OR_DATA_SEGMENT)
    {
        attributes |= ACCESS_RIGHTS_CODE_OR_DATA_SEGMENT;
    }

    if (flags & ATTR_PRESENT)
    {
        attributes |= ACCESS_RIGHTS_PRESENT;
    }

    if (flags & ATTR_64BIT_MODE_ACTIVE)
    {
        attributes |= ACCESS_RIGHTS_64BIT_MODE_ACTIVE;
    }

    if (flags & ATTR_32BIT_SEGMENT)
    {
        attributes |= ACCESS_RIGHTS_32BIT_SEGMENT;
    }

    if (flags & ATTR_GRANULARITY)
    {
        attributes |= ACCESS_RIGHTS_GRANULARITY;
    }

    if (flags & ATTR_UNUSABLE)
    {
        attributes |= ACCESS_RIGHTS_UNUSABLE;
    }

    return attributes;
}
