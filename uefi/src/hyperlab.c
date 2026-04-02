#include <arch/x86/control_registers.h>
#include <arch/x86/idt.h>
#include <efi.h>
#include <halt.h>
#include <handle_exception.h>
#include <print.h>
#include <stdint.h>
#include <vm_backend.h>
#include <vm_backends.h>

#define CPUID_01H_ECX_VMX (1<<5)

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

    write_cr8(15);

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

    halt();
    __builtin_unreachable();
}
