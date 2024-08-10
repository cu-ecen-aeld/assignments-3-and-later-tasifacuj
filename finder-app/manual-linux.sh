#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
COLOR_GREEN="\e[32m"
COLOR_DEF="\e[39m"
COLOR_RED="\e[31m"

echomsg() { echo -e $COLOR_GREEN"$@"$COLOR_DEF >/dev/stderr; }
echoerr() { echo -e $COLOR_RED"$@"$COLOR_DEF >/dev/stderr; }


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

echomsg "Fixing dtc-lexer"

 sed -i '/YYLTYPE yylloc;/d' $OUTDIR/linux-stable/scripts/dtc/dtc-lexer.l || {
    echoerr "Failed to apply lex patch $FINDER_APP_DIR/../above.patch"
 }


if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echomsg "make clean"
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper

    echomsg "make defconfig"
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig

    echomsg "make all"
    make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    # echomsg "make modules"
    # make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

    # echomsg "make devicetree"
    # make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs

    # echo ">>> tmp stop"
    # exit 0
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}" || {
    echoerr "Failed to copy image to ${OUTDIR}"
    exit 1
}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"

echomsg "create rootfs"
cd "${OUTDIR}/rootfs"
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p  var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
test -f ${OUTDIR}/rootfs/bin/busybox || {
    echomsg "Make and install busybox"
    make distclean
    make defconfig
    make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-
    make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- install
} 

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# Library dependencies
#       [Requesting program interpreter: /lib/ld-linux-aarch64.so.1]
#  0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
#  0x0000000000000001 (NEEDED)             Shared library: [libresolv.so.2]
#  0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cd "${OUTDIR}/rootfs"
cp -af "$SYSROOT/lib/ld-linux-aarch64.so.1" lib
cp -af "$SYSROOT/lib64/ld-2.33.so" lib64

cp -af $SYSROOT/lib64/libm.so.6 lib64
cp -af $SYSROOT/lib64/libm-2.33.so lib64

cp -af $SYSROOT/lib64/libresolv.so.2 lib64
cp -af $SYSROOT/lib64/libresolv-2.33.so lib64

cp -af $SYSROOT/lib64/libc.so.6 lib64
cp -af $SYSROOT/lib64/libc-2.33.so lib64

# TODO: Make device nodes
echomsg "Make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1


# TODO: Clean and build the writer utility
echomsg "Clean and build the writer utility in $FINDER_APP_DIR"
cd $FINDER_APP_DIR
export CROSS_COMPILE=aarch64-none-linux-gnu-
make clean
make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp "$FINDER_APP_DIR/writer" "${OUTDIR}/rootfs/home"
cp "$FINDER_APP_DIR/finder.sh" "${OUTDIR}/rootfs/home"
cp "$FINDER_APP_DIR/finder-test.sh" "${OUTDIR}/rootfs/home"
cp -r "$FINDER_APP_DIR/../conf" "${OUTDIR}/rootfs/home"
cp "$FINDER_APP_DIR/autorun-qemu.sh" "${OUTDIR}/rootfs/home"

# TODO: Chown the root directory

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"
echomsg "manual-linux.sh completed"