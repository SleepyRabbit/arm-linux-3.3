cmd_drivers/mtd/nand/nand.o := /opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-ld -EL    -r -o drivers/mtd/nand/nand.o drivers/mtd/nand/nand_base.o drivers/mtd/nand/nand_bbt.o ; scripts/mod/modpost drivers/mtd/nand/nand.o