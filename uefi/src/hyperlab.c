#include <arch/x86/control_registers.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <efi.h>
#include <halt.h>
#include <handle_exception.h>
#include <print.h>
#include <stdint.h>
#include <vm_backend.h>
#include <vm_backends.h>

#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <arch/x86/msr.h>
EFI_SYSTEM_TABLE* SystemTable = NULL;

struct vm_backend* backend = NULL;
EFI_STATUS hyperlab_entry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* EFISystemTable)
{
    (void)ImageHandle;

    SystemTable = EFISystemTable;

    SystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    print_init(SystemTable);
    print(L"HyperLab UEFI initializing...\r\n");

    // Mask all interrupts to prevent them from causing #GPs
    write_cr8(15);

    setup_gdt_and_tss();

    if (!setup_idt(SystemTable))
    {
        print(L"Failed to initalize IDT\r\n");
        halt();
    }

    uint32_t ebx, ecx, edx;
    __asm__ volatile("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));

    for (size_t i = 0; i < sizeof(vendor_specific_vm_backends)/sizeof(vendor_specific_vm_backends[0]); i++)
    {
        struct vm_backend_entry* cur_entry = &vendor_specific_vm_backends[i];
        if (cur_entry->cpuid_0h_ebx == ebx && cur_entry->cpuid_0h_ecx == ecx && cur_entry->cpuid_0h_edx == edx)
        {
            backend = cur_entry->backend;
            break;
        }
    }

    if (!backend)
    {
        print(L"Could not find VM backend for current CPU\r\n");
        halt();
    }

    if (!backend->init || !backend->init(EFISystemTable))
    {
        print(L"Backend initialization failed\r\n");
        halt();
    }

    if (!backend->create_vm || !backend->create_vm())
    {
        print(L"Failed to create a new VM\r\n");
        halt();
    }

    EFI_PHYSICAL_ADDRESS guest_memory = 0;
    // 4 page tables (PML4, PDPT, PD, PT) + 1 code and 1 data page
    #define GUEST_PAGES_COUNT (4+1+1)
    EFI_STATUS status = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, GUEST_PAGES_COUNT, &guest_memory);
    SystemTable->BootServices->SetMem((void*)guest_memory, 0x1000 * GUEST_PAGES_COUNT, 0);

    if (status != EFI_SUCCESS)
    {
        print(L"Failed to allocate pages for guest VM\r\n");
        halt();
    }

    #define DATA_PAGE_NUM 4
    #define CODE_PAGE_NUM 5
    for (unsigned int i = 0; i < GUEST_PAGES_COUNT; i++)
    {
        unsigned int prot = PROT_READ | (i == CODE_PAGE_NUM ? PROT_EXEC : PROT_WRITE);
        if (!backend->map_page || !backend->map_page(0x1000 * i, prot, (void*)(guest_memory+0x1000*i)))
        {
            print(L"Could not map guest pages to the VM\r\n");
            halt();
        }
    }

    #define PML4_PHYS_ADDR 0x0
    #define PDPT_PHYS_ADDR 0x1000
    #define PD_PHYS_ADDR 0x2000
    #define PT_PHYS_ADDR 0x3000
    #define DATA_PHYS_ADDR (DATA_PAGE_NUM << 12)
    #define CODE_PHYS_ADDR (CODE_PAGE_NUM << 12)

    uint64_t* pml4 = (uint64_t*)(guest_memory+PML4_PHYS_ADDR);
    uint64_t* pdpt = (uint64_t*)(guest_memory+PDPT_PHYS_ADDR);
    uint64_t* pd = (uint64_t*)(guest_memory+PD_PHYS_ADDR);
    uint64_t* pt = (uint64_t*)(guest_memory+PT_PHYS_ADDR);

    #define PAGE_PRESENT (1ULL<<0)
    #define PAGE_WRITABLE (1ULL<<1)
    #define PAGE_USER (1ULL<<2)
    #define PAGE_NOEXECUTE (1ULL<<63)

    pml4[0] = PDPT_PHYS_ADDR | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    pdpt[0] = PD_PHYS_ADDR | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    pd[2] = PT_PHYS_ADDR | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    pt[0] = DATA_PHYS_ADDR | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NOEXECUTE;
    pt[1] = CODE_PHYS_ADDR | PAGE_PRESENT | PAGE_USER;

    uint8_t* guest_code = (uint8_t*)(guest_memory+0x1000*CODE_PAGE_NUM);

    int i = 0;
    guest_code[i++] = 0x0f;
    guest_code[i++] = 0x01;
    guest_code[i++] = 0xc1;

    struct vm_context context;
    SystemTable->BootServices->SetMem(&context, sizeof(context), 0);

    context.cr0 = read_cr0();
    context.cr3 = 0;
    context.cr4 = read_cr4();
    context.dr6 = (1ULL<<11) | (1ULL<<16);
    context.dr7 = (1ULL<<10);

    context.es.base = 0;
    context.cs.base = 0;
    context.ss.base = 0;
    context.ds.base = 0;
    context.fs.base = 0;
    context.gs.base = 0;
    context.ldtr.base = 0;
    context.tr.base = 0;

    context.es.limit = 0xffffffffULL;
    context.cs.limit = 0xffffffffULL;
    context.ss.limit = 0xffffffffULL;
    context.ds.limit = 0xffffffffULL;
    context.fs.limit = 0xffffffffULL;
    context.gs.limit = 0xffffffffULL;
    context.ldtr.limit = 0;
    context.tr.limit = 0;

    uint32_t data_segment_attributes =
        backend->compute_segment_attributes(0, 3, ATTR_CODE_OR_DATA_SEGMENT | ATTR_PRESENT | ATTR_32BIT_SEGMENT | ATTR_GRANULARITY);

    context.es.attributes = data_segment_attributes;
    context.cs.attributes =
        backend->compute_segment_attributes(0, 11, ATTR_CODE_OR_DATA_SEGMENT | ATTR_PRESENT | ATTR_64BIT_MODE_ACTIVE | ATTR_GRANULARITY);
    context.ss.attributes = data_segment_attributes;
    context.ds.attributes = data_segment_attributes;
    context.fs.attributes = data_segment_attributes;
    context.gs.attributes = data_segment_attributes;

    context.ldtr.attributes = backend->compute_segment_attributes(0, 2, ATTR_UNUSABLE);
    context.tr.attributes = backend->compute_segment_attributes(0, 11, ATTR_PRESENT);

    context.registers[REG_RSP] = 0x401000;
    context.rip = 0x401000;
    context.rflags = 2;

    context.efer = EFER_NXE | EFER_LMA | EFER_LME;

    context.sysenter_cs = 0;
    context.sysenter_esp = 0;
    context.sysenter_eip = 0;

    context.smbase = 0;
    context.bndcfgs = 0;
    context.perf_global_ctrl = 0;

#define MSR_IA32_PAT 0x277
    context.pat = rdmsr(MSR_IA32_PAT);

    struct vm_context out_context;

    if (backend->enter_vm(&context, &out_context))
    {
        print(L"VM-entry OK!\r\n");
    }
    else
    {
        print(L"VM-entry fail\r\n");
    }

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

    print(L"Exiting-event ID = ");
    uint64_t exiting_event_id = 0xcc;
    if (vmread(VMCS_EXITING_EVENT_ID, &exiting_event_id))
    {
        printf(L"0x%x\r\n", exiting_event_id);
    }
    else
    {
        print(L"???\r\n");
    }

    print(L"Exiting-event error code = ");
    uint64_t exiting_event_error_code = 0xdd;
    if (vmread(VMCS_EXITING_EVENT_ERROR_CODE, &exiting_event_error_code))
    {
        printf(L"0x%x\r\n", exiting_event_error_code);
    }
    else
    {
        print(L"???\r\n");
    }


    print(L"Original-event ID = ");
    uint64_t original_event_id = 0xcc;
    if (vmread(VMCS_ORIGINAL_EVENT_ID, &original_event_id))
    {
        printf(L"0x%x\r\n", original_event_id);
    }
    else
    {
        print(L"???\r\n");
    }

    print(L"Original-event error code = ");
    uint64_t original_event_error_code = 0xdd;
    if (vmread(VMCS_ORIGINAL_EVENT_ERROR_CODE, &original_event_error_code))
    {
        printf(L"0x%x\r\n", original_event_error_code);
    }
    else
    {
        print(L"???\r\n");
    }

    halt();
    __builtin_unreachable();
}
