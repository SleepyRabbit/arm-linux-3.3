cmd_arch/arm/mm/cache-fa.o := /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-gcc -Wp,-MD,arch/arm/mm/.cache-fa.o.d  -nostdinc -isystem /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/../lib/gcc/arm-unknown-linux-uclibcgnueabi/4.4.0/include -I/media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-GM/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=fa626te -include asm/unified.h -msoft-float -gdwarf-2        -c -o arch/arm/mm/cache-fa.o arch/arm/mm/cache-fa.S

source_arch/arm/mm/cache-fa.o := arch/arm/mm/cache-fa.S

deps_arch/arm/mm/cache-fa.o := \
    $(wildcard include/config/arch/gemini.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/linkage.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
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
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/memory.h \
    $(wildcard include/config/need/mach/memory/h.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/page/offset.h) \
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
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/page.h \
    $(wildcard include/config/cpu/copy/v3.h) \
    $(wildcard include/config/cpu/copy/v4wt.h) \
    $(wildcard include/config/cpu/copy/v4wb.h) \
    $(wildcard include/config/cpu/copy/feroceon.h) \
    $(wildcard include/config/cpu/copy/fa.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/arm/lpae.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
  include/asm-generic/getorder.h \
  arch/arm/mm/proc-macros.S \
    $(wildcard include/config/cpu/use/domains.h) \
    $(wildcard include/config/cpu/dcache/writethrough.h) \
    $(wildcard include/config/pm/sleep.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/asm-offsets.h \
  include/generated/asm-offsets.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/arm/thumbee.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/iwmmxt.h) \

arch/arm/mm/cache-fa.o: $(deps_arch/arm/mm/cache-fa.o)

$(deps_arch/arm/mm/cache-fa.o):
