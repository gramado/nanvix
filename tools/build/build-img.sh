# 
# Copyright(C) 2011-2018 Pedro H. Penna   <pedrohenriquepenna@gmail.com> 
#              2016-2018 Davidson Francis <davidsondfgl@gmail.com>
#
# This file is part of Nanvix.
#
# Nanvix is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Nanvix is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Nanvix.  If not, see <http://www.gnu.org/licenses/>.
#

# Paths
CURDIR="$( cd "$(dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PORTS_DIR=$(readlink -f $CURDIR/../../src/ubin/PORTS)

# Root credentials.
ROOTUID=0
ROOTGID=0

# User credentials.
NOOBUID=1
NOOBUID=1

[[ $(id -u) -eq 0 ]] && LOOP=$(losetup -f)

# HD Image?
BUILD_HD_IMAGE=0

# Toolchain
STRIP=$TARGET-elf-nanvix-strip
OBJCOPY=$TARGET-elf-nanvix-objcopy

# Checks if TARGET is OR1K, if so, use Qemu
if [ "$TARGET" = "or1k" ];
then
	QEMU_VIRT=qemu-or1k
fi

#
# Inserts disk in a loop device.
#   $1 Disk image name.
#
function insert {
	losetup $LOOP $1
	mount $LOOP /mnt
}

#
# Ejects current disk from loop device.
#
function eject {
	umount $LOOP
	losetup -d $LOOP
}

# Generate passwords file
#   $1 Disk image name.
#
function passwords
{
	file="passwords"
	
	$QEMU_VIRT bin/useradd $file root root $ROOTGID $ROOTUID
	$QEMU_VIRT bin/useradd $file noob noob $NOOBUID $NOOBUID

	chmod 600 $file
	
	$QEMU_VIRT bin/cp.minix $1 $file /etc/$file $ROOTUID $ROOTGID
	
	# House keeping.
	rm -f $file
}

#
# Formats a disk.
#   $1 Disk image name.
#   $2 Number of inodes.
#   $3 File system size (in blocks).
#
function format {
	$QEMU_VIRT bin/mkfs.minix $1 $2 $3 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /etc $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /sbin $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /bin $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /home $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /home/rep1 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /home/rep2 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /dev $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mkdir.minix $1 /home/mysem/ $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mknod.minix $1 /dev/null 666 c 0 0 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mknod.minix $1 /dev/tty 666 c 0 1 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mknod.minix $1 /dev/klog 666 c 0 2 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mknod.minix $1 /dev/ramdisk 666 b 0 0 $ROOTUID $ROOTGID
	$QEMU_VIRT bin/mknod.minix $1 /dev/ramdisk1 666 b 1 0 $ROOTUID $ROOTGID
}

#
# Copy a given folder (and its files) to a root directory disk image.
#   $1 Disk image name.
#   $2 Target Folder
#
function copy_folder
{
	disk="$1"
	folder="$2"

	# Seems like mkdir.minix do not works well with leading '/'
	# lets check and remove if any
	if [ "${folder: -1}" = "/" ];
	then
		folder=${folder%?}
	fi

	complete_path=$(readlink -f "$folder")
	basefolder=$(basename "$folder")
	prefix=$(dirname "$complete_path")

	# Make directories first
	find "$complete_path" -type d -print0 | while read -d $'\0' cfolder
	do
		newpath=${cfolder#"$prefix"}
		if [ "${newpath:0:1}" != "/" ];
		then
			newpath="/$newpath"
		fi

		echo "Creating $newpath folder..."
		$QEMU_VIRT bin/mkdir.minix "$disk" "$newpath" "$ROOTUID" "$ROOTGID"
	done
	echo ""

	# Copy files
	find "$complete_path" -type f -print0 | while read -d $'\0' cfolder
	do
		newpath=${cfolder#"$prefix"}
		if [ "${newpath:0:1}" != "/" ];
		then
			newpath="/$newpath"
		fi

		echo "Copying $cfolder file into $newpath..."
		$QEMU_VIRT bin/cp.minix "$disk" "$cfolder" "$newpath" "$ROOTUID" "$ROOTGID"
	done
	echo ""
}

#
# Copy files to a disk image.
#   $1 Target disk image.
#
function copy_files
{
	chmod 666 tools/img/inittab
	chmod 600 tools/img/inittab
	
	$QEMU_VIRT bin/cp.minix $1 tools/img/inittab /etc/inittab $ROOTUID $ROOTGID
	
	passwords $1
	
	for file in bin/sbin/*; do
		filename=`basename $file`
		if [[ "$filename" != *.sym ]]; then
			$QEMU_VIRT bin/cp.minix $1 $file /sbin/$filename $ROOTUID $ROOTGID
		fi;
	done
	
	for file in bin/ubin/*; do
		filename=`basename $file`
		if [[ "$filename" != *.sym ]]; then
			$QEMU_VIRT bin/cp.minix $1 $file /bin/$filename $ROOTUID $ROOTGID
		fi;
	done

	# For each folder in the PORTS directory, copy those that have
	# the 'binaries' folder into the disk image
	for folder in $(ls -d $PORTS_DIR/*)
	do
		# Skip folders that do not contains the 'binaries' folder inside
		if [ ! -d "$folder/binaries" ]
		then
			continue
		fi

		echo "Copying $(basename $folder) port into initrd..."

		# Iterate over each folder and copy them into the disk image
		for targ_folder in $(ls -d $folder/binaries/*)
		do
			copy_folder initrd.img $targ_folder
		done
	done
}

#
# Strip a binary from it's debug symbols and
# add a GNU debug link to the original binary
# $1 The binary to strip
#
function strip_binary
{
	mkdir -p $BINDIR/symbols/bin/{sbin,ubin}
	if [[ "$1" != *.sym ]]; then
		# Get debug symbols from kernel
		$OBJCOPY --only-keep-debug $1 $BINDIR/symbols/$1.sym
		# Remove debug symbols
		$STRIP --strip-debug $1
	fi;
}

# Build live nanvix image.
if [ "$1" = "--build-iso" ];
then
	strip_binary bin/kernel

	mkdir -p nanvix-iso/boot/grub
	cp bin/kernel nanvix-iso/kernel
	cp initrd.img nanvix-iso/initrd.img
	cp tools/img/menu.lst nanvix-iso/boot/grub/menu.lst
	cp tools/img/stage2_eltorito nanvix-iso/boot/grub/stage2_eltorito
	sed -i 's/fd0/cd/g' nanvix-iso/boot/grub/menu.lst
	genisoimage -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 \
		-input-charset utf-8 -boot-info-table -o nanvix.iso nanvix-iso
elif [ "$1" = "--build-floppy" ];
then
	strip_binary bin/kernel

	cp -f tools/img/blank.img nanvix.img
	insert nanvix.img
	cp bin/kernel /mnt/kernel
	cp initrd.img /mnt/initrd.img
	cp tools/img/menu.lst /mnt/boot/menu.lst
	eject
else
	for file in bin/sbin/*; do
		if [[ "$file" != *.sym ]]; then
			strip_binary $file
		fi;
	done

	for file in bin/ubin/*; do
		if [[ "$file" != *.sym ]]; then
			strip_binary $file
		fi;
	done

	# Build HDD image.
	if [ "$BUILD_HD_IMAGE" -eq 1 ];
	then
		dd if=/dev/zero of=hdd.img bs=1024 count=65536
		format hdd.img 1024 32768
		copy_files hdd.img
	fi

	# Build initrd image.
	dd if=/dev/zero of=initrd.img bs=1024 count=65536
	format initrd.img 1024 64535
	copy_files initrd.img
	initrdsize=`stat -c %s initrd.img`
	maxsize=`grep "INITRD_SIZE" include/nanvix/config.h | grep -Po "(0x[0-9]+|[0-9]+)"`
	maxsize=`printf "%d\n" $maxsize`
	if [ $initrdsize -gt $maxsize ]; then
		echo "NOT ENOUGH SPACE ON INITRD, size: $initrdsize / maxsize: $maxsize"
		echo "INITRD SIZE is $initrdsize"
		rm *.img
		exit -1
	fi 
fi
