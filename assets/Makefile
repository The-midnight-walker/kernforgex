# SPDX-License-Identifier: GPL-2.0
#
# vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :
#
# writting kernel modules and driver Makefile starter

MODULE_C :=

obj-m += $(MODULE_C).o
#$(MODULE_C)-objs :=
ccflags-y += -DDEBUG

PWD    := $(CURDIR)
KDIR   := /lib/modules/$(shell uname -r)/build
ARCH   := x86_64
CROSS_COMPILE ?=

INDENT := indent
#uncommnent if you are on kernel tree or specify checkpatch.pl script path
CHECKPATCH ?=		 #$(KDIR)/scripts/checkpatch.pl

#=============
#output style
#=============

define print_err
	@printf "\n\033[1;31m$(1)\033[0m\n\n"
endef

define print_info_y
	@printf "\n\033[1;33m$(1)\033[0m\n\n"
endef

define print_info_g
	@printf "\n\033[1;32m$(1)\033[0m\n\n"
endef

#==========
#   build
#===========

.PHONY: all build install clean compile-bear lkcode-style indent checkpatch sa sa_sparse sa_gcc sa_flawfinder sa_cppcheck tarxz-pkg help

all: clean lkcode-style sa compile-bear

build:
	$(call print_info_y,--- Building '${MODULE_C}.ko' : KDIR=${KDIR} ARCH=${ARCH} CROSS_COMPILE=$(CROSS_COMPILE) ---)
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	$(call print_info_g,module $(MODULE_C).ko built.)
	#TODO: add if statement to check the existence of the file .ko

install:
	$(call print_info_y,--- installing ---)

	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	@sudo depmod

clean:
	$(call print_info_y,--- cleaning build files ---)

	$(MAKE) -C $(KDIR) M=$(PWD) clean

	@rm -rf *~ bkp/ compile_commands.json .*.cmd *.o *.ko *.mod *.mod.c *.symvers *.order

#TODO:mv compile-bear build-with-bear
#modify the building .... banner
compile-bear:
	$(call print_info_y,--- building 'compile_commands.json' ---)

	@bear -- $(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	#TODO: add if statement to check the existence of the file .ko

#=======================
#linux kernel code style
#=======================

lkcode-style: indent checkpatch

indent:
	$(call print_info_y,--- applying kernel code style indentation ---)

	@mkdir -p bkp; cp -f *.[chsS] bkp/ 2>/dev/null || true
	@${INDENT} -linux --line-length95 *.[chsS] 2>/dev/null || true

checkpatch:
ifneq ($(CHECKPATCH),)
	$(call print_info_y,--- kernel code style check with checkpatch.pl ---)

	@$(CHECKPATCH) --no-tree -f --max-line-length=95 *.[ch]

else
	$(call print_err,--- checkpatch.pl script not found. Specify 'CHECKPATCH' in the Makefile ---)
endif

#====================
#sa : static analysis
#====================

sa: sa_sparse sa_gcc sa_flawfinder sa_cppcheck

sa_sparse:
	$(call print_info_y,--- static analysis with sparse ---)

	$(MAKE) C=2 CHECK="/usr/bin/sparse" -C $(KDIR) M=$(PWD) modules

sa_gcc:
	$(call print_info_y,--- static analysis with gcc ---)

	$(MAKE) W=1 -C $(KDIR) M=$(PWD) modules

sa_flawfinder:
	$(call print_info_y,--- static analysis with flawfinder ---)

	@flawfinder *.[ch]

sa_cppcheck:

	$(call print_info_y,--- static analysis with cppcheck ---)
	@cppcheck -v --force --enable=all -i .tmp_versions/ -i *.mod.c -i bkp/ --suppress=missingIncludeSystem .

PKG_NAME := ${MODULE_C}
tarxz-pkg:
	@rm -f ../${PKG_NAME}.tar.xz 2>/dev/null
	$(MAKE) clean
	$(call print_info_y,--- packaging ---)
	@tar caf ../${PKG_NAME}.tar.xz *
	@ls -l ../${PKG_NAME}.tar.xz
	$(call print_info_y,=== package created: ../$(PKG_NAME).tar.xz ===)

help:
	$(call print_info_y,=== Makefile Help ===)
	@echo 'build       : (default) Build the kernel module (.ko)'
	@echo 'all         : Run bear, lkcode-style, static analysis, and build'
	@echo 'clean       : Remove build artifacts and bkp/ folder'
	@echo 'sa          : Run all static analyzers (sparse, gcc W=1, flawfinder, cppcheck)'
	@echo 'tarxz-pkg   : Create a tarball of the sources'
