CC=clang
TARGET_CFLAGS=-Wl,-entry:hyperlab_entry -Wl,-subsystem:efi_application -fuse-ld=lld-link -target x86_64-unknown-windows -nostdlib -ffreestanding -fshort-wchar -mno-red-zone
CFLAGS=$(TARGET_CFLAGS) -Wall -Wextra -Wno-error=strict-prototypes -pedantic -Werror -masm=intel -I/usr/include/efi -I./src

HYPERLAB_SRCS=src/hyperlab.c src/print.c src/vm_backends.c src/handle_exception.c src/exception_handlers.S src/gdt.c src/gdt_segment_helper.S src/halt.c
INTEL_SRCS=src/intel/vm_backend.c src/intel/vmx.c src/intel/vmcs.c src/intel/ept.c src/intel/enter_vm.S

OUT_IMG=hyperlab.img

all: $(OUT_IMG)

BOOTX64.EFI:
	$(CC) $(CFLAGS) -o BOOTX64.EFI $(HYPERLAB_SRCS) $(INTEL_SRCS)

$(OUT_IMG): clean BOOTX64.EFI
	dd if=/dev/zero of=$(OUT_IMG) bs=1k count=1440
	mformat -i $(OUT_IMG) -f 1440 ::
	mmd -i $(OUT_IMG) ::/EFI
	mmd -i $(OUT_IMG) ::/EFI/BOOT
	mcopy -i $(OUT_IMG) BOOTX64.EFI ::/EFI/BOOT

clean:
	rm BOOTX64.EFI $(OUT_IMG) || true
