CC = gcc
LD = ld
AS = nasm

CFLAGS = -m64 -ffreestanding -fno-stack-protector -nostdlib -mno-red-zone -Iinclude/kernel -Wall -Wextra
APP_CFLAGS = -m64 -ffreestanding -fno-stack-protector -nostdlib -mno-red-zone -fno-pie -no-pie -Iinclude/kernel -Wall -Wextra
LDFLAGS = -n -T linker.ld -m elf_x86_64
APP_LDFLAGS = -n -T src/apps/shell/linker.ld -m elf_x86_64
ASFLAGS = -f elf64

ASM_SOURCES = $(wildcard src/boot/*.asm) $(wildcard src/kernel/*.asm)
C_SOURCES = $(wildcard src/kernel/*.c)
APP_C_SOURCES = $(wildcard src/apps/shell/*.c)
OBJ = $(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)
APP_OBJ = $(APP_C_SOURCES:.c=.o)

# All assembly files use elf64 format
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJ)

src/apps/shell/%.o: src/apps/shell/%.c
	$(CC) $(APP_CFLAGS) -c $< -o $@

shell.elf: $(APP_OBJ)
	$(LD) $(APP_LDFLAGS) -o shell.elf $(APP_OBJ)

iso: kernel.bin shell.elf
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	mkdir -p ramdisk_root/bin
	cp shell.elf ramdisk_root/bin/shell
	echo "Hello from Zoho OS VFS!" > ramdisk_root/test.txt
	tar -cvf ramdisk.tar -C ramdisk_root bin test.txt
	# Create raw HDD image (64MB)
	dd if=/dev/zero of=hdd.img bs=1M count=64
	# Write TAR to LBA 2048 (1MB offset)
	dd if=ramdisk.tar of=hdd.img seek=2048 conv=notrunc
	rm -rf ramdisk_root ramdisk.tar
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'insmod all_video' >> isodir/boot/grub/grub.cfg
	echo '' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "Zoho OS" {' >> isodir/boot/grub/grub.cfg
	echo '	set gfxpayload=keep' >> isodir/boot/grub/grub.cfg
	echo '	multiboot2 /boot/kernel.bin' >> isodir/boot/grub/grub.cfg
	echo '	boot' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o zoho_os.iso isodir

run: iso
	qemu-system-x86_64 -cdrom zoho_os.iso -drive file=hdd.img,format=raw -serial stdio -smp 4 -m 512M -device qemu-xhci -device usb-tablet

docs: documentation.tex
	pdflatex -interaction=nonstopmode documentation.tex
	pdflatex -interaction=nonstopmode documentation.tex

clean:
	rm -rf src/boot/*.o src/kernel/*.o src/apps/shell/*.o kernel.bin shell.elf zoho_os.iso isodir ramdisk_root ramdisk.tar hdd.img documentation.pdf documentation.aux documentation.log documentation.out documentation.toc
