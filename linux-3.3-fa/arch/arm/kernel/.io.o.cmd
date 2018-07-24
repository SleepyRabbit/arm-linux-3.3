cmd_arch/arm/kernel/io.o := /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-gcc -Wp,-MD,arch/arm/kernel/.io.o.d  -nostdinc -isystem /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/../lib/gcc/arm-unknown-linux-uclibcgnueabi/4.4.0/include -I/media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-GM/include -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -O2 -marm -fno-dwarf2-cfi-asm -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=fa626te -msoft-float -Uarm -Wframe-larger-than=1024 -fno-stack-protector -fomit-frame-pointer -g -fno-inline-functions-called-once -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack    -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(io)"  -D"KBUILD_MODNAME=KBUILD_STR(io)" -c -o arch/arm/kernel/io.o arch/arm/kernel/io.c

source_arch/arm/kernel/io.o := arch/arm/kernel/io.c

deps_arch/arm/kernel/io.o := \
  include/linux/export.h \
    $(wildcard include/config/symbol/prefix.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/types.h \
  include/asm-generic/int-ll64.h \
  arch/arm/include/generated/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc4.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/posix_types.h \
  include/linux/io.h \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/has/ioport.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/io.h \
    $(wildcard include/config/arm/dma/mem/bufferable.h) \
    $(wildcard include/config/pcmcia/soc/common.h) \
    $(wildcard include/config/pci.h) \
    $(wildcard include/config/isa.h) \
    $(wildcard include/config/pccard.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/swab.h \
  include/linux/byteorder/generic.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/memory.h \
    $(wildcard include/config/need/mach/memory/h.h) \
    $(wildcard include/config/page/offset.h) \
    $(wildcard include/config/thumb2/kernel.h) \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/have/tcm.h) \
    $(wildcard include/config/arm/patch/phys/virt.h) \
    $(wildcard include/config/phys/offset.h) \
  include/linux/const.h \
  arch/arm/include/generated/asm/sizes.h \
  include/asm-generic/sizes.h \
  arch/arm/mach-GM/include/mach/memory.h \
  arch/arm/mach-GM/include/mach/platform/memory.h \
  arch/arm/mach-GM/include/mach/platform/platform_io.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/system.h \
    $(wildcard include/config/arm/lpae.h) \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/cpu/fmp626.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/fa526.h) \
    $(wildcard include/config/cpu/fa626te.h) \
    $(wildcard include/config/platform/gm8210.h) \
    $(wildcard include/config/arch/has/barriers.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/sa110.h) \
    $(wildcard include/config/cpu/v6.h) \
  include/linux/linkage.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/linkage.h \
  include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  include/linux/typecheck.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/irqflags.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
    $(wildcard include/config/arm/thumb.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/hwcap.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/outercache.h \
    $(wildcard include/config/outer/cache/sync.h) \
    $(wildcard include/config/outer/cache.h) \
  include/asm-generic/cmpxchg-local.h \
  include/asm-generic/cmpxchg.h \
  include/asm-generic/pci_iomap.h \
    $(wildcard include/config/no/generic/pci/ioport/map.h) \
    $(wildcard include/config/generic/pci/iomap.h) \
  arch/arm/mach-GM/include/mach/io.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/page.h \
    $(wildcard include/config/cpu/copy/v3.h) \
    $(wildcard include/config/cpu/copy/v4wt.h) \
    $(wildcard include/config/cpu/copy/v4wb.h) \
    $(wildcard include/config/cpu/copy/feroceon.h) \
    $(wildcard include/config/cpu/copy/fa.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/glue.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/pgtable-2level-types.h \
  include/asm-generic/getorder.h \

arch/arm/kernel/io.o: $(deps_arch/arm/kernel/io.o)

$(deps_arch/arm/kernel/io.o):
