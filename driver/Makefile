obj-m += hyperfuzz.o

HYPERFUZZ_OBJS = src/hyperfuzz_entry.o

hyperfuzz-objs := $(HYPERFUZZ_OBJS) #$(INTEL_OBJS)

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
