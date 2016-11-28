# Define cross-compiler
CC=powerpc-4xx-gcc

# Define kernel and installation paths
KERNEL_DIR ?= /second/source/linux-xlnx
INST_DIR ?= /mnt/ramdisk/opt/xt_fpga

# Hold current directory for future use
PWD := $(shell pwd)

# Register kernel objects into module
obj-m += xt_fpga.o
xt_fpga-objs := xtables_fpga.o dpi_accel.o

# List of module files for install and clean
MODULE_FILES=*.o .*.cmd *.ko *.mod.c .tmp_versions Module.symvers modules.order
MODULE_KO=xt_fpga.ko

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	rm -rf $(MODULE_FILES)

install:
	cp -rf $(MODULE_KO) $(INST_DIR)