#include <efi.h>
#include <print.h>
#include <stdarg.h>
#include <stdint.h>

static SIMPLE_TEXT_OUTPUT_INTERFACE* ConOut = NULL;
void print_init(EFI_SYSTEM_TABLE* SystemTable)
{
    ConOut = SystemTable->ConOut;
}

void print(CHAR16* string)
{
    ConOut->OutputString(ConOut, string);
}

void vsnprintf(CHAR16* out_buffer, size_t out_length, CHAR16* fmt, va_list varargs)
{
    if (out_length == 0)
    {
        return;
    }

    size_t fmt_i = 0;
    size_t out_i = 0;
    while (out_i < out_length - 1)
    {
        switch (fmt[fmt_i])
        {
        case L'\0':
            out_buffer[out_i] = L'\0';
            return;
        case L'%':
            fmt_i++;
            switch (fmt[fmt_i])
            {
            case L'\0':
                out_buffer[out_i] = L'\0';
                return;
            case L'S':
            case L's':
                {
                    CHAR16* str = va_arg(varargs, CHAR16*);
                    size_t i = 0;

                    while (str[i] != L'\0' && out_i < out_length - 1)
                    {
                        out_buffer[out_i] = str[i];
                        out_i++;
                        i++;
                    }
                }
                break;
            case L'x':
            case L'X':
                {
                    uint64_t value = va_arg(varargs, uint64_t);
                    
                    CHAR16* hex_chars = L"0123456789ABCDEF";

                    size_t digit = 15;

                    while (out_i < out_length - 1)
                    {
                        out_buffer[out_i] = hex_chars[
                            (value >> (digit * 4)) & 0xf
                        ];

                        out_i++;
                        if (digit == 0)
                        {
                            break;
                        }

                        digit--;
                    }
                }
                break;
            default:
                out_buffer[out_i] = fmt[fmt_i];
                out_i++;
                break;
            }
            fmt_i++;
            break;
        default:
            out_buffer[out_i] = fmt[fmt_i];
            out_i++;
            fmt_i++;
            break;
        }
    }
    out_buffer[out_i] = L'\0';
}

void snprintf(CHAR16* out_buffer, size_t length, CHAR16* fmt, ...)
{
    va_list varargs;

    va_start(varargs, fmt);

    vsnprintf(out_buffer, length, fmt, varargs);

    va_end(varargs);
}

void printf(CHAR16* fmt, ...)
{
    va_list varargs;

    va_start(varargs, fmt);

    CHAR16 buffer[0x400];

    vsnprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), fmt, varargs);

    print(buffer);

    va_end(varargs);
}