#include <efi.h>

static SIMPLE_TEXT_OUTPUT_INTERFACE* ConOut = NULL;
#include "print.h"
#include <efi.h>

void print_init(EFI_SYSTEM_TABLE* SystemTable)
{
    ConOut = SystemTable->ConOut;
}

void print(CHAR16* string)
{
    ConOut->OutputString(ConOut, string);
}
