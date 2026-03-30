#ifndef _PRINT_H_
#define _PRINT_H_

#include <efi.h>
void print_init(EFI_SYSTEM_TABLE* SystemTable);
void print(CHAR16* string);

#endif
