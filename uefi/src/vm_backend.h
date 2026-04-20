#ifndef _VM_BACKEND_H_
#define _VM_BACKEND_H_

#include <efi.h>
#include <stdint.h>
#include "vm_context.h"

#define PROT_READ 1U
#define PROT_WRITE 2U
#define PROT_EXEC 4U
#define PROT_USER 8U

struct vm_backend
{
    int (*init)(EFI_SYSTEM_TABLE* SystemTable);
    int (*create_vm)(void);
    int (*destroy_vm)(void);
    int (*map_page)(size_t guest_phys_addr, unsigned int protection, void* host_page_addr);
    int (*unmap_page)(size_t guest_phys_addr);
    int (*get_vm_context)(struct vm_context* out_context);
    int (*set_vm_context)(struct vm_context* new_context);
    int (*enter_vm)(void);
    int (*deinit)(void);
};

#endif
