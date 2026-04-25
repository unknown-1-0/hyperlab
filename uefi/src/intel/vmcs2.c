#include <arch/x86/control_registers.h>
#include <arch/x86/debug_registers.h>
#include <arch/x86/msr.h>
#include <gdt.h>
#include <intel/vm_backend.h>
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
        uint32_t result_value = (uint32_t)secondary_procbased_ctls;// |
            //SECONDARY_PROCBASED_VM_EXEC_CTLS_ENABLE_EPT;

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
            /*PRIMARY_VM_EXIT_CTLS_SAVE_DEBUG_CTLS |*/ PRIMARY_VM_EXIT_CTLS_HOST_ADDRESS_SPACE_SIZE; /*|
            PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PAT | PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PAT |
            PRIMARY_VM_EXIT_CTLS_SAVE_IA32_EFER | PRIMARY_VM_EXIT_CTLS_LOAD_IA32_EFER |
            PRIMARY_VM_EXIT_CTLS_LOAD_IA32_PERF_GLOBAL_CTL; // | PRIMARY_VM_EXIT_CTLS_SAVE_IA32_PERF_GLOBAL_CTL;*/

        if ((result_value & (vm_exit_ctls >> 32)) != result_value)
        {
            print(L"VMCS: Could not set desired value of primary VM-exit controls\r\n");
            return 0;
        }

        VMWRITE(VMCS_PRIMARY_VM_EXIT_CTLS, result_value);
    }

    {
        uint64_t vm_entry_ctls = rdmsr(MSR_IA32_VMX_ENTRY_CTLS);

        uint32_t result_value = (uint32_t)vm_entry_ctls /* | VM_ENTRY_CTLS_LOAD_DEBUG_CTLS*/ |
            VM_ENTRY_CTLS_IA32E_MODE_GUEST/* | VM_ENTRY_CTLS_LOAD_IA32_PERF_GLOBAL_CTRL |
            VM_ENTRY_CTLS_LOAD_IA32_PAT | VM_ENTRY_CTLS_LOAD_IA32_EFER*/;

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
    VMWRITE(VMCS_GUEST_CR3, read_cr3());
    VMWRITE(VMCS_GUEST_CR4, read_cr4());
    VMWRITE(VMCS_GUEST_DR7, (1ULL<<10));

    VMWRITE(VMCS_GUEST_ES_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_CS_SELECTOR, 0x8);
    VMWRITE(VMCS_GUEST_SS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_DS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_FS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_GS_SELECTOR, 0x10);
    VMWRITE(VMCS_GUEST_LDTR_SELECTOR, 0);
    VMWRITE(VMCS_GUEST_TR_SELECTOR, 0x18);

    VMWRITE(VMCS_GUEST_ES_BASE, 0);
    VMWRITE(VMCS_GUEST_CS_BASE, 0);
    VMWRITE(VMCS_GUEST_SS_BASE, 0);
    VMWRITE(VMCS_GUEST_DS_BASE, 0);
    VMWRITE(VMCS_GUEST_FS_BASE, 0);
    VMWRITE(VMCS_GUEST_GS_BASE, 0);
    VMWRITE(VMCS_GUEST_LDTR_BASE, 0);
    VMWRITE(VMCS_GUEST_TR_BASE, (size_t)(&new_tss));
    //VMWRITE(VMCS_GUEST_TR_BASE, 0);
    //VMWRITE(VMCS_GUEST_GDTR_BASE, 0);
    //VMWRITE(VMCS_GUEST_IDTR_BASE, 0);

    {
        uint8_t gdtr[sizeof(uint16_t)+sizeof(size_t)];
        __asm__("sgdt %0":"=m"(gdtr)::"memory");

        size_t gdt_base = *(size_t*)(gdtr+sizeof(uint16_t));
        uint16_t gdt_limit = *(uint16_t*)gdtr;

        VMWRITE(VMCS_GUEST_GDTR_BASE, gdt_base);
        VMWRITE(VMCS_GUEST_GDTR_LIMIT, gdt_limit);
    }

    {
        uint8_t idtr[sizeof(uint16_t)+sizeof(size_t)];
        __asm__("sidt %0":"=m"(idtr)::"memory");

        size_t idt_base = *(size_t*)(idtr+sizeof(uint16_t));
        uint16_t idt_limit = *(uint16_t*)idtr;

        VMWRITE(VMCS_GUEST_IDTR_BASE, idt_base);
        VMWRITE(VMCS_GUEST_IDTR_LIMIT, idt_limit);
    }

    VMWRITE(VMCS_GUEST_ES_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_CS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_SS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_DS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_FS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_GS_LIMIT, 0xffffffffULL);
    VMWRITE(VMCS_GUEST_LDTR_LIMIT, 0);
    VMWRITE(VMCS_GUEST_TR_LIMIT, sizeof(new_tss)-1);
    //VMWRITE(VMCS_GUEST_GDTR_LIMIT, 8*3-1);
    //VMWRITE(VMCS_GUEST_IDTR_LIMIT, 0);

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

    VMWRITE(VMCS_GUEST_LDTR_ACCESS_RIGHTS, ACCESS_RIGHTS_PRESENT | (2));    VMWRITE(VMCS_GUEST_TR_ACCESS_RIGHTS,
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
    VMWRITE(VMCS_GUEST_IA32_PERF_GLOBAL_CTRL, rdmsr(MSR_IA32_PERF_GLOBAL_CTRL));

    // ! Do not forget to change these to appropriate values !
    VMWRITE(VMCS_GUEST_RSP, 0);
    VMWRITE(VMCS_GUEST_RIP, 0);
    VMWRITE(VMCS_GUEST_RFLAGS, 2);

    VMWRITE(VMCS_GUEST_SMBASE, 0x30000);
    VMWRITE(VMCS_GUEST_IA32_BNDCFGS, 0);

    VMWRITE(VMCS_VMX_PREEMPTION_TIMER_VALUE, 0);
    VMWRITE(VMCS_PML_INDEX, 0);

    VMWRITE(VMCS_GUEST_PDPTE0, 0);
    VMWRITE(VMCS_GUEST_PDPTE1, 0);
    VMWRITE(VMCS_GUEST_PDPTE2, 0);
    VMWRITE(VMCS_GUEST_PDPTE3, 0);
    return 1;
}

int intel_vmcs_setup_ept(void)
{
    return 0;
}

#define MSR_IA32_VMX_BASIC 0x480
void intel_save_vmcs(EFI_PHYSICAL_ADDRESS vmcs, CHAR16* file_path)
{
    UINTN vmcs_size = (rdmsr(MSR_IA32_VMX_BASIC) >> 32) & 0x1fffULL;
    printf(L"Saving VMCS @ 0x%x (size: 0x%x bytes)\r\n", vmcs, vmcs_size);

    EFI_GUID LoadedImageProtocolGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImageProtocol = NULL;
    EFI_STATUS status = BootServices->OpenProtocol(ImageHandle, &LoadedImageProtocolGUID, (VOID**)&LoadedImageProtocol,
        ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(status))
    {
        printf(L"Failed to open EFI loaded image protocol, status = 0x%x\r\n", status);
        return;
    }

    print(L"Opened EFI loaded image protocol\r\n");

    EFI_HANDLE DeviceHandle = LoadedImageProtocol->DeviceHandle;


    EFI_GUID SimpleFileSystemProtocolGUID = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystemProtocol = NULL;
    status = BootServices->OpenProtocol(DeviceHandle, &SimpleFileSystemProtocolGUID, (VOID**)&SimpleFileSystemProtocol,
        ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    BootServices->CloseProtocol(ImageHandle, &LoadedImageProtocolGUID, ImageHandle, 0);
    LoadedImageProtocol = NULL;    if (EFI_ERROR(status))
    {
        printf(L"Failed to open EFI Simple File System Protocol, status = 0x%x\r\n", status);
        return;
    }

    print(L"Opened EFI Simple File System Protocol\r\n");
    EFI_FILE_PROTOCOL* FSRoot = NULL;
    status = SimpleFileSystemProtocol->OpenVolume(SimpleFileSystemProtocol, &FSRoot);

    BootServices->CloseProtocol(DeviceHandle, &SimpleFileSystemProtocolGUID, ImageHandle, 0);
    SimpleFileSystemProtocol = NULL;

    if (EFI_ERROR(status))
    {
        printf(L"Failed to open the root directory on the volume, status = 0x%x\r\n", status);
        return;
    }    print(L"Opened the root directory on the volume\r\n");

    EFI_FILE_PROTOCOL* OutFile = NULL;
    status = FSRoot->Open(FSRoot, &OutFile, file_path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

    FSRoot->Close(FSRoot);
    FSRoot = NULL;

    if (EFI_ERROR(status))
    {
        printf(L"Failed to open output file, status = 0x%x\r\n", status);
        return;
    }

    printf(L"Writing VMCS to \"%s\"... ", file_path);

    UINTN bytes_to_write = vmcs_size;

    status = OutFile->Write(OutFile, &bytes_to_write, (void*)vmcs);
    if (EFI_ERROR(status) || bytes_to_write != vmcs_size)
    {
        printf(L"FAIL (Status: 0x%x, bytes written: 0x%x)\r\n", status, bytes_to_write);
    }
    else
    {
        print(L"OK!\r\n");
    }    print(L"Closing the file... ");
    OutFile->Close(OutFile);
    OutFile = NULL;
    print(L"Done!\r\n");
}

void guest_code(void)
{
    __asm__("vmcall");
}

int intel_init_vmcs(EFI_PHYSICAL_ADDRESS vmcs)
{
    uint32_t vmcs_revision_id = intel_vmx_get_vmcs_revision_id();
    printf(L"VMCS revision identifier: 0x%x\r\n", vmcs_revision_id);
    *(uint32_t*)vmcs = vmcs_revision_id;

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

    VMWRITE(VMCS_GUEST_RIP, (size_t)(&guest_code));

    __asm__ volatile(
        "mov eax, 0x681c\n"
        "vmwrite rax, rsp\n"
        "mov eax, 0x6c14\n"
        "vmwrite rax, rsp\n"
        "mov eax, 0x6c16\n"
        "lea rbx, [rip+.L1]\n"
        "vmwrite rax, rbx\n"
        "vmlaunch\n"
        ".L1:\n":::"rax","rbx","flags"
    );

    print(L"VM-instruction error = ");
    uint64_t vm_inst_error = 0xaa;
    if (vmread(VMCS_VM_INSTRUCTION_ERROR, &vm_inst_error))
    {
        printf(L"0x%x\r\n", vm_inst_error);
    }
    else
    {
        print(L"???\r\n");
    }

    print(L"VM-exit reason = ");
    uint64_t vm_exit_reason = 0xbb;
    if (vmread(VMCS_EXIT_REASON, &vm_exit_reason))
    {
        printf(L"0x%x\r\n", vm_exit_reason);
    }
    else
    {
        print(L"???\r\n");
    }

    //BootServices->Stall(10000000);

    if (!intel_vmcs_setup_ept())
    {
        print(L"VMCS: Failed to set up EPT\r\n");

        EFI_PHYSICAL_ADDRESS cur_vmcs = 0xbadf00d;
        if (!vmptrst(&cur_vmcs))
        {
            print(L"Failed to read current VMCS address\r\n");
            return 0;
        }

        printf(L"Expected VMCS address: 0x%x, actual: 0x%x\r\n", vmcs, cur_vmcs);

        print(L"Dumping VMCS before VMCLEAR\r\n");
        intel_save_vmcs(vmcs, L"vmcs_data_before_vmclear.bin");

        if (!vmclear(vmcs))
        {
            print(L"VMCLEAR failed!\r\n");
            return 0;
        }

        print(L"Dumping VMCS after VMCLEAR\r\n");
        intel_save_vmcs(vmcs, L"vmcs_data_after_vmclear.bin");
/*
        if (!vmptrld(cur_vmcs))
        {
            print(L"VMPTRLD failed\r\n");
            return 0;
        }

        print(L"VMPTRLD OK, marking VMCS and dumping it\r\n");

        for (unsigned int width = 0; width < 4; width++)
        {
            for (unsigned int type = 0; type < 4; type++)
            {
                for (unsigned int index = 0; index < (1ULL<<9); index++)
                {
                    uint64_t vmcs_field = (index << 1) | (type << 10) | (width << 13);

                    printf(L"VMWRITE(0x%x): ", vmcs_field);

                    if (vmwrite(vmcs_field, vmcs_field))
                    {
                        print(L"OK!\r\n");
                    }
                    else
                    {
                        print(L"FAIL!\r\n");
                    }
                }
            }
        }
        print(L"Dumping marked VMCS before VMCLEAR\r\n");
        intel_save_vmcs(vmcs, L"marked_vmcs_data_before_vmclear.bin");

        if (!vmclear(cur_vmcs))
        {
            print(L"VMCLEAR failed...\r\n");
        }

        print(L"Dumping marked VMCS after VMCLEAR\r\n");
        intel_save_vmcs(vmcs, L"marked_vmcs_data_after_vmclear.bin");
*/
        return 0;
    }

    return 1;
}
