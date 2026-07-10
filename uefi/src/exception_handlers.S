.intel_syntax noprefix

.macro CREATE_EXCEPTION_HANDLER name, exception_number
.global \name
\name:
cli
push r15
push r14
push r13
push r12
push r11
push r10
push r9
push r8
push rdi
push rsi
push rbp
push rbx
push rdx
push rcx
push rax
mov ecx, \exception_number
mov rdx, rsp
and rsp, ~0xf
push 0
jmp handle_exception
.endm

CREATE_EXCEPTION_HANDLER handle_divide_error, 0
CREATE_EXCEPTION_HANDLER handle_debug_exception, 1
CREATE_EXCEPTION_HANDLER handle_nmi, 2
CREATE_EXCEPTION_HANDLER handle_breakpoint, 3
CREATE_EXCEPTION_HANDLER handle_overflow_exception, 4
CREATE_EXCEPTION_HANDLER handle_bound_range_exceeded, 5
CREATE_EXCEPTION_HANDLER handle_invalid_opcode, 6
CREATE_EXCEPTION_HANDLER handle_device_not_available, 7
CREATE_EXCEPTION_HANDLER handle_double_fault, 8
CREATE_EXCEPTION_HANDLER handle_coprocessor_segment_overrun, 9
CREATE_EXCEPTION_HANDLER handle_invalid_tss, 10
CREATE_EXCEPTION_HANDLER handle_segment_not_present, 11
CREATE_EXCEPTION_HANDLER handle_stack_fault, 12
CREATE_EXCEPTION_HANDLER handle_general_protection_fault, 13
CREATE_EXCEPTION_HANDLER handle_page_fault, 14
CREATE_EXCEPTION_HANDLER handle_reserved15, 15
CREATE_EXCEPTION_HANDLER handle_x87_fpu_fp_error, 16
CREATE_EXCEPTION_HANDLER handle_alignment_check_exception, 17
CREATE_EXCEPTION_HANDLER handle_machine_check_exception, 18
CREATE_EXCEPTION_HANDLER handle_simd_fp_exception, 19
CREATE_EXCEPTION_HANDLER handle_virtualization_exception, 20
CREATE_EXCEPTION_HANDLER handle_control_protection_exception, 21
