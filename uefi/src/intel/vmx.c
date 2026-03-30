#include <arch/x86/msr.h>
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
