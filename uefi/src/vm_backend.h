#ifndef _VM_BACKEND_H_
#define _VM_BACKEND_H_

#include <efi.h>
#include <stdint.h>
#include "vm_context.h"

struct vm_backend
{
    int (*init)(EFI_SYSTEM_TABLE* SystemTable);
    int (*create_vm)(void);
    int (*destroy_vm)(void);
    int (*get_vm_context)(struct vm_context* out_context);
    int (*set_vm_context)(struct vm_context* new_context);
    int (*enter_vm)(void);
    int (*deinit)(void);
};

#endif
