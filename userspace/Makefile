# Define cross-compiler and CFLAGS
CC 		= powerpc-4xx-gcc
CFLAGS 	= -O2 -Wall -shared -fPIC \
			--sysroot=/opt/ELDK/5.5/powerpc-4xx/sysroots/ppc440e-linux/ \
			-I/opt/ELDK/5.5/powerpc-4xx/rootfs-lsb-dev/usr/include/

# Define installation folder
INST_DIR ?= /mnt/ramdisk/lib/xtables

libxt_fpga.so: libxt_fpga.o
	$(CC) $(CFLAGS) -o $@ $^

install:
	cp -f libxt_fpga.so $(INST_DIR)

clean:
	rm -rf *.*o