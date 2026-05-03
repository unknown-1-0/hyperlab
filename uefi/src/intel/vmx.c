#include <arch/x86/msr.h>
#include <arch/x86/control_registers.h>
#include <intel/vmx.h>
#include <print.h>
#include <stdint.h>

#define MSR_IA32_FEATURE_CONTROL 0x3a
#define FEATURE_CONTROL_LOCKED 1
#define FEATURE_CONTROL_VMX_OUTSIDE_SMX (1<<2)

#define CPUID_01H_ECX_VMX (1<<5)

int intel_check_vmx_support(void)
{
    uint32_t ecx;

    __asm__ volatile("cpuid":"=c"(ecx):"a"(1):"rbx","rdx");

    if ((ecx & CPUID_01H_ECX_VMX) == 0)
    {
        print(L"VMX: Not supported by CPU\r\n");
        return 0;
    }

    uint64_t feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);

    if ((feature_control & (FEATURE_CONTROL_VMX_OUTSIDE_SMX | FEATURE_CONTROL_LOCKED)) == FEATURE_CONTROL_LOCKED)
    {
        print(L"VMX: Disabled by firmware\r\n");
        return 0;
    }

    if ((feature_control & FEATURE_CONTROL_LOCKED) == 0)
    {
        wrmsr(MSR_IA32_FEATURE_CONTROL, feature_control | FEATURE_CONTROL_VMX_OUTSIDE_SMX | FEATURE_CONTROL_LOCKED);
    }
    
    return 1;
}

#define MSR_IA32_VMX_BASIC 0x480
#define MSR_IA32_VMX_CR0_FIXED0 0x486
#define MSR_IA32_VMX_CR0_FIXED1 0x487
#define MSR_IA32_VMX_CR4_FIXED0 0x488
#define MSR_IA32_VMX_CR4_FIXED1 0x489

uint32_t intel_vmx_get_vmcs_revision_id(void)
{
    return rdmsr(MSR_IA32_VMX_BASIC) & ((1UL<<31)-1);
}

#define CR4_MCE (1ULL<<6)
#define CR4_OSFXSR (1ULL<<9)
#define CR4_OSXMMEXCPT (1ULL<<10)
#define CR4_OSXSAVE (1ULL<<18)
static uint64_t old_cr0 = 0;
static uint64_t old_cr4 = 0;
static EFI_PHYSICAL_ADDRESS vmxon_area_addr = 0;

int intel_enter_vmx_operation(EFI_PHYSICAL_ADDRESS vmxon_area)
{
    print(L"VMX: Entering VMX operation\r\n");

    old_cr0 = read_cr0();
    old_cr4 = read_cr4();

    write_cr0((old_cr0 | rdmsr(MSR_IA32_VMX_CR0_FIXED0)) & rdmsr(MSR_IA32_VMX_CR0_FIXED1));

    uint64_t vmx_cr4_fixed1 = rdmsr(MSR_IA32_VMX_CR4_FIXED1);
    uint64_t extensions_cr4_flags = CR4_OSXSAVE | CR4_OSXMMEXCPT | CR4_OSFXSR;
    uint64_t new_cr4 = (old_cr4 | rdmsr(MSR_IA32_VMX_CR4_FIXED0) | extensions_cr4_flags | CR4_MCE) & vmx_cr4_fixed1;

    if ((new_cr4 & CR4_MCE) == 0)
    {
        print(L"VMX: Machine-check exception handling is not supported in VMX operation!?\r\n");
        write_cr0(old_cr0);
        return 0;
    }


    if ((new_cr4 & extensions_cr4_flags) != extensions_cr4_flags)
    {
        print(L"VMX: Some requested CPU extensions in CR4 are not supported in VMX operation\r\n");
        write_cr0(old_cr0);
        return 0;
    }
    // No need to worry about CR4.VMXE: it must be 1 in VMX operation, so it will be set here
    write_cr4(new_cr4);

    *(uint32_t*)vmxon_area = intel_vmx_get_vmcs_revision_id();

    if (!vmxon(vmxon_area))
    {
        print(L"VMX: VMXON failed\r\n");
        write_cr0(old_cr0);
        write_cr4(old_cr4);
        return 0;
    }

    vmxon_area_addr = vmxon_area;
    print(L"VMX: VMXON succeeded!\r\n");
    return 1;
}
