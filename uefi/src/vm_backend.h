#ifndef _VM_BACKEND_H_
#define _VM_BACKEND_H_

#include <efi.h>
#include <stdint.h>
#include <vm_context.h>

#define PROT_READ 1U
#define PROT_WRITE 2U
#define PROT_EXEC 4U
#define PROT_USER 8U

#define ATTR_CODE_OR_DATA_SEGMENT (1U<<0)
#define ATTR_PRESENT (1U<<1)
#define ATTR_64BIT_MODE_ACTIVE (1U<<2)
#define ATTR_32BIT_SEGMENT (1U<<3)
#define ATTR_GRANULARITY (1U<<4)
#define ATTR_UNUSABLE (1U<<5)

struct vm_backend
{
    int (*init)(EFI_SYSTEM_TABLE* SystemTable);
    int (*create_vm)(void);
    int (*destroy_vm)(void);
    int (*map_page)(size_t guest_phys_addr, unsigned int protection, void* host_page_addr);
    int (*unmap_page)(size_t guest_phys_addr);
    int (*get_vm_context)(struct vm_context* out_context);
    uint32_t (*compute_segment_attributes)(uint8_t dpl, uint8_t type, uint8_t flags);
    int (*enter_vm)(struct vm_context* context, struct vm_context* out_context);
    int (*deinit)(void);
};

#endif
