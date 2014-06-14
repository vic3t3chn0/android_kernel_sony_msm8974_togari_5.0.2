# export stuff
export ARCH=arm
export PATH=/home/tom/android_kernels/gcc-linaro-4.9-2015.02-3-x86_64_arm-linux-gnueabihf/bin/:$PATH
export CROSS_COMPILE=arm-linux-gnueabihf-

# make clean
rm -rf ../../android_kernels/build_togari/zImage-dtb
rm -rf ../../android_kernels/build_togari/boot.img
make distclean

# build kernel
make lp_togari_defconfig
make -j5
cp arch/arm/boot/zImage-dtb ../../android_kernels/build_togari

# build image
cd ../../android_kernels/build_togari
./mkbootimg --base 0x00000000 --kernel zImage-dtb --ramdisk_offset 0x02000000 --tags_offset 0x01E00000 --pagesize 2048 --cmdline "androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3b7 ehci-hcd.park=3 androidboot.bootdevice=msm_sdcc.1 vmalloc=300M dwc3.maximum_speed=high dwc3_msm.prop_chg_detect=Y
" --ramdisk ramdisk.cpio.gz -o boot.img
cd ../../android_kernels/kernel_togari
