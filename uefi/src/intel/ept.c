#include <arch/x86/msr.h>
#include <efi.h>
#include <intel/ept.h>
#include <intel/vm_backend.h>
#include <intel/vmcs.h>
#include <intel/vmx.h>
#include <halt.h>
#include <print.h>

int intel_ept_alloc_page(EFI_PHYSICAL_ADDRESS* out_addr)
{
    EFI_PHYSICAL_ADDRESS addr = 0;
    EFI_STATUS status = BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);

    if (status != EFI_SUCCESS)
    {
        printf(L"EPT: Could not allocate a new page, status = 0x%x\r\n", status);
        *out_addr = 0;
        return 0;
    }

    BootServices->SetMem((void*)addr, 0x1000, 0);
    *out_addr = addr;
    return 1;
}

void intel_ept_free_page(EFI_PHYSICAL_ADDRESS addr)
{
    EFI_STATUS status = BootServices->FreePages(addr, 1);

    if (status == EFI_SUCCESS)
    {
        return;
    }

    printf(L"BUG: Invalid page free detected! FreePages(0x%x, 1) returned 0x%x!\r\n", addr, status);
    halt();
}

#define MSR_IA32_VMX_EPT_VPID_CAP 0x48c
#define VMX_EPT_VPID_CAP_EPT_PAGE_WALK_LENGTH_4_SUPPORTED (1ULL<<6)
#define VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_UC_SUPPORTED (1ULL<<8)
#define VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_WB_SUPPORTED (1ULL<<14)
#define VMX_EPT_VPID_CAP_EPT_ACCESSED_DIRTY_FLAGS_SUPPORTED (1ULL<<21)

static EFI_PHYSICAL_ADDRESS ept_root = 0;

int intel_ept_initialize(void)
{
    uint64_t vmx_ept_vpid_cap = rdmsr(MSR_IA32_VMX_EPT_VPID_CAP);

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_PAGE_WALK_LENGTH_4_SUPPORTED) == 0)
    {
        print(L"EPT: Page walk length of 4 is not supported!\r\n");
        return 0;
    }

    uint64_t ept_ptr = EPT_PTR_PAGE_WALK_LENGTH(4);

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_WB_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_MEMORY_TYPE_WB;
        print(L"EPT: Using WB as EPT memory type\r\n");
    }
    else if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_MEMORY_TYPE_UC_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_MEMORY_TYPE_UC;
        print(L"EPT: Using UC as EPT memory type\r\n");
    }
    else
    {
        print(L"EPT: Cannot determine the supported EPT memory type\r\n");
        return 0;
    }

    if ((vmx_ept_vpid_cap & VMX_EPT_VPID_CAP_EPT_ACCESSED_DIRTY_FLAGS_SUPPORTED) != 0)
    {
        ept_ptr |= EPT_PTR_ENABLE_ACCESSED_DIRTY_FLAGS;
        print(L"EPT: Accessed and dirty flags enabled\r\n");
    }

    if (!intel_ept_alloc_page(&ept_root))
    {
        print(L"EPT: Could not allocate a new page for root EPT table\r\n");
        ept_root = 0;
        return 0;
    }

    ept_ptr |= ept_root;

    if (!vmwrite(VMCS_EPT_PTR, ept_ptr))
    {
        print(L"EPT: Could not write EPT pointer to VMCS\r\n");
        intel_ept_free_page(ept_root);
        ept_root = 0;
        return 0;
    }
    return 1;
}

struct ept_table_indexes
{
    uint16_t pml4e;
    uint16_t pdpte;
    uint16_t pde;
    uint16_t pte;
};

static void intel_ept_convert_guest_address_to_ept_indexes(size_t guest_address, struct ept_table_indexes* out_table)
{
    out_table->pml4e = (guest_address >> 39) & 0x1ff;
    out_table->pdpte = (guest_address >> 30) & 0x1ff;
    out_table->pde = (guest_address >> 21) & 0x1ff;
    out_table->pte = (guest_address >> 12) & 0x1ff;
}

#define EPT_READABLE (1ULL<<0)
#define EPT_WRITABLE (1ULL<<1)
#define EPT_EXECUTABLE (1ULL<<2)

// if not counting MAXPHYADDR (M), bits [51:12] seem to be allocated for the address of the next entry
// (bits [51:M] are documented to be reserved and must be zero)
#define EPT_ADDRESS_MASK (((1ULL<<52)-1ULL) ^ ((1ULL<<12)-1ULL))

int intel_ept_map_page(size_t guest_address, unsigned int protection, void* host_address)
{
    if ((protection & PROT_READ) == 0)
    {
        return 0;
    }

    if (ept_root == 0)
    {
        return 0;
    }

    struct ept_table_indexes table_indexes;
    intel_ept_convert_guest_address_to_ept_indexes(guest_address, &table_indexes);

    uint64_t* pml4e_ptr = (uint64_t*)(ept_root + table_indexes.pml4e * 8);
    uint64_t pml4e = *pml4e_ptr;

    if ((pml4e & EPT_READABLE) == 0)
    {
        EFI_PHYSICAL_ADDRESS new_pdpt = 0;
        if (!intel_ept_alloc_page(&new_pdpt))
        {
            print(L"EPT: Could not allocate a new page for a new PDPT\r\n");
            return 0;
        }

        pml4e = new_pdpt | EPT_READABLE | EPT_WRITABLE | EPT_EXECUTABLE;

        *pml4e_ptr = pml4e;
    }

    pml4e &= EPT_ADDRESS_MASK;

    uint64_t* pdpte_ptr = (uint64_t*)(pml4e + table_indexes.pdpte * 8);
    uint64_t pdpte = *pdpte_ptr;

    if ((pdpte & EPT_READABLE) == 0)
    {
        EFI_PHYSICAL_ADDRESS new_pd = 0;
        if (!intel_ept_alloc_page(&new_pd))
        {
            print(L"EPT: Could not allocate a new page for a new PD\r\n");
            return 0;
        }

        pdpte = new_pd | EPT_READABLE | EPT_WRITABLE | EPT_EXECUTABLE;

        *pdpte_ptr = pdpte;
    }

    pdpte &= EPT_ADDRESS_MASK;

    uint64_t* pde_ptr = (uint64_t*)(pdpte + table_indexes.pde * 8);
    uint64_t pde = *pde_ptr;

    if ((pde & EPT_READABLE) == 0)
    {
        EFI_PHYSICAL_ADDRESS new_pt = 0;
        if (!intel_ept_alloc_page(&new_pt))
        {
            print(L"EPT: Could not allocate a new page for a new PT\r\n");
            return 0;
        }

        pde = new_pt | EPT_READABLE | EPT_WRITABLE | EPT_EXECUTABLE;

        *pde_ptr = pde;
    }

    pde &= EPT_ADDRESS_MASK;

    uint64_t* pte_ptr = (uint64_t*)(pde + table_indexes.pte * 8);

    uint64_t pte = *pte_ptr;

    if ((pte & EPT_READABLE) != 0)
    {
        // Do not allow modification of an already allocated page table entry
        print(L"EPT: Attempted to remap an already present guest page\r\n");
        printf(L"Indexes: PML4E: 0x%x PDPTE: 0x%x PDE: 0x%x PTE: 0x%x\r\n",
            table_indexes.pml4e, table_indexes.pdpte, table_indexes.pde, table_indexes.pte);
        return 0;
    }

    pte = (uint64_t)host_address & EPT_ADDRESS_MASK;
    pte |= EPT_READABLE;

    if (protection & PROT_WRITE)
    {
        pte |= EPT_WRITABLE;
    }

    if (protection & PROT_EXEC)
    {
        pte |= EPT_EXECUTABLE;
    }

    *pte_ptr = pte;
    return 1;
}
