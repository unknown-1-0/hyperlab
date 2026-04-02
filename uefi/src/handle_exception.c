#include <arch/x86/control_registers.h>
#include <arch/x86/idt.h>
#include <efi.h>
#include <halt.h>
#include <handle_exception.h>
#include <print.h>
#include <stdint.h>


static CHAR16* exception_names[] = {
    L"#DE - Divide Error",
    L"#DB - Debug Exception",
    L"NMI - Non-Maskable Interrupt",
    L"#BP - Breakpoint Exception",
    L"#OF - Overflow Exception",
    L"#BR - BOUND Range Exceeded Exception",
    L"#UD - Invalid Opcode Exception",
    L"#NM - Device Not Available Exception",
    L"#DF(0x%x) - Double Fault Exception",
    L"INT9 - Coprocessor Segment Overrun",
    L"#TS(0x%x) - Invalid TSS Exception",
    L"#NP(0x%x) - Segment Not Present",
    L"#SS(0x%x) - Stack Fault Exception",
    L"#GP(0x%x) - General Protection Exception",
    L"#PF(0x%x) - Page Fault Exception",
    L"INT15 - Reserved",
    L"#MF - x87 FPU Floating-Point Error",
    L"#AC(0x%x) - Alignment Check Exception",
    L"#MC - Machine-Check Exception",
    L"#XM - SIMD Floating-Point Exception",
    L"#VE - Virtualization Exception",
    L"#CP(0x%x) - Control Protection Exception"
};

void handle_exception(unsigned int exception_number, struct exception_handler_stack* stack)
{
    uint64_t error_code = 0;
    struct iret_stack* iret_stack = &stack->iret_stack_no_error_code;

    if (((1UL<<exception_number) & 
        ((1UL<<8)|(1UL<<10)|(1UL<<11)|(1UL<<12)|(1UL<<13)|(1UL<<14)|(1UL<<17)|(1UL<<21))) != 0)
    {
        error_code = stack->error_code;
        iret_stack = &stack->iret_stack_with_error_code;
    }

    if (exception_number < sizeof(exception_names)/sizeof(exception_names[0]))
    {
        printf(exception_names[exception_number], error_code);
    }
    else
    {
        printf(L"Unknown exception 0x%x", exception_number);
    }

    print(L"\r\n");

    printf(L"RAX=0x%x RCX=0x%x RDX=0x%x RBX=0x%x\r\n", stack->rax, stack->rcx, stack->rdx, stack->rbx);
    printf(L"RSP=0x%x RBP=0x%x RSI=0x%x RDI=0x%x\r\n", iret_stack->rsp, stack->rbp, stack->rsi, stack->rdi);
    printf(L"R08=0x%x R09=0x%x R10=0x%x R11=0x%x\r\n", stack->r8, stack->r9, stack->r10, stack->r11);
    printf(L"R12=0x%x R13=0x%x R14=0x%x R15=0x%x\r\n", stack->r12, stack->r13, stack->r14, stack->r15);
    printf(L"RIP=0x%x RFLAGS=0x%x\r\n", iret_stack->rip, iret_stack->rflags);

    uint16_t es, ds, fs, gs;

    __asm__("mov %0, es":"=r"(es));
    __asm__("mov %0, ds":"=r"(ds));
    __asm__("mov %0, fs":"=r"(fs));
    __asm__("mov %0, gs":"=r"(gs));

    printf(L"ES=0x%x CS=0x%x SS=0x%x\r\nDS=0x%x FS=0x%x GS=0x%x\r\n",
        es, iret_stack->cs, iret_stack->ss,ds, fs, gs);


    printf(L"CR0=0x%x CR2=0x%x CR3=0x%x\r\nCR4=0x%x CR8=0x%x\r\n", read_cr0(), read_cr2(), read_cr3(), read_cr4(), read_cr8());

    halt();
}

void handle_divide_error(void);
void handle_debug_exception(void);
void handle_nmi(void);
void handle_breakpoint(void);
void handle_overflow_exception(void);
void handle_bound_range_exceeded(void);
void handle_invalid_opcode(void);
void handle_device_not_available(void);
void handle_double_fault(void);
void handle_coprocessor_segment_overrun(void);
void handle_invalid_tss(void);
void handle_segment_not_present(void);
void handle_stack_fault(void);
void handle_general_protection_fault(void);
void handle_page_fault(void);
void handle_reserved15(void);
void handle_x87_fpu_fp_error(void);
void handle_alignment_check_exception(void);
void handle_machine_check_exception(void);
void handle_simd_fp_exception(void);
void handle_virtualization_exception(void);
void handle_control_protection_exception(void);

static void (*exception_handlers[])(void) = {
    handle_divide_error,
    handle_debug_exception,
    handle_nmi,
    handle_breakpoint,
    handle_overflow_exception,
    handle_bound_range_exceeded,
    handle_invalid_opcode,
    handle_device_not_available,
    handle_double_fault,
    handle_coprocessor_segment_overrun,
    handle_invalid_tss,
    handle_segment_not_present,
    handle_stack_fault,
    handle_general_protection_fault,
    handle_page_fault,
    handle_reserved15,
    handle_x87_fpu_fp_error,
    handle_alignment_check_exception,
    handle_machine_check_exception,
    handle_simd_fp_exception,
    handle_virtualization_exception,
    handle_control_protection_exception
};

static struct idt_entry* idt = NULL;

int setup_idt(EFI_SYSTEM_TABLE* SystemTable)
{
    const size_t idt_entries = sizeof(exception_handlers)/sizeof(exception_handlers[0]);
    if (SystemTable->BootServices->AllocatePool(EfiRuntimeServicesData, idt_entries*sizeof(*idt), (VOID**)&idt) != EFI_SUCCESS)
    {
        return 0;
    }

    SystemTable->BootServices->SetMem(idt, idt_entries * sizeof(*idt), 0);

    uint16_t code_segment;
    __asm__("mov %0, cs":"=r"(code_segment));

    for (size_t i = 0; i < idt_entries; i++)
    {
        struct idt_entry* cur_entry = &idt[i];
        size_t cur_handler = (size_t)exception_handlers[i];
        cur_entry->offset_low = (uint16_t)cur_handler;
        cur_entry->segment_selector = code_segment;
        cur_entry->flags = IDT_PRESENT | IDT_TRAP_GATE;
        cur_entry->offset_middle = (uint16_t)(cur_handler >> 16);
        cur_entry->offset_high = cur_handler >> 32;
    }
    
    uint8_t new_idtr[sizeof(uint16_t)+sizeof(size_t)];
    *(volatile uint16_t*)new_idtr = idt_entries * sizeof(*idt) - 1;
    *(volatile size_t*)(new_idtr+sizeof(uint16_t)) = (size_t)idt;

    __asm__ volatile("lidt %0"::"m"(new_idtr));
    return 1;
}
