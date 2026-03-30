#include <efi.h>
#include <print.h>

static SIMPLE_TEXT_OUTPUT_INTERFACE* ConOut = NULL;
void print_init(EFI_SYSTEM_TABLE* SystemTable)
{
    ConOut = SystemTable->ConOut;
}

void print(CHAR16* string)
{
    ConOut->OutputString(ConOut, string);
}
