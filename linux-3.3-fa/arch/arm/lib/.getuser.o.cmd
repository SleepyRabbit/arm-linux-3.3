cmd_arch/arm/lib/getuser.o := /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-gcc -Wp,-MD,arch/arm/lib/.getuser.o.d  -nostdinc -isystem /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/../lib/gcc/arm-unknown-linux-uclibcgnueabi/4.4.0/include -I/media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include -Iarch/arm/include/generated -Iinclude  -include /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-GM/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=fa626te -include asm/unified.h -msoft-float -gdwarf-2        -c -o arch/arm/lib/getuser.o arch/arm/lib/getuser.S

source_arch/arm/lib/getuser.o := arch/arm/lib/getuser.S

deps_arch/arm/lib/getuser.o := \
    $(wildcard include/config/thumb2/kernel.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/linkage.h \
  arch/arm/include/generated/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  /media/psf/Home/Documents/projects/camera/GM8136/arm-linux-3.3/linux-3.3-fa/arch/arm/include/asm/domain.h \
    $(wildcard include/config/io/36.h) \
    $(wildcard include/config/cpu/use/domains.h) \

arch/arm/lib/getuser.o: $(deps_arch/arm/lib/getuser.o)

$(deps_arch/arm/lib/getuser.o):
