Makefile:
KERNELDIR ?= /opt/buildroot/buildroot-2025.02.3/output/build/linux-custom/
ARCH ?= arm64
CROSS_COMPILE ?= /opt/buildroot/buildroot-2025.02.3/output/host/bin/aarch64-buildroot-linux-gnu-

obj-m := rtc.o

all:
	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean

