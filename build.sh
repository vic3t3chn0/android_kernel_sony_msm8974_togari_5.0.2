export TCHAIN=android_kernels/arm-cortex_a15-linux-gnueabihf-linaro_4.9.3-2015.03/bin/arm-cortex_a15-linux-gnueabihf-

# make clean
#rm -rf ../../android_kernels/build_togari/zImage-dtb
#rm -rf ../../android_kernels/build_togari/boot.img
#rm -rf ../../android_kernels/zip_togari/boot.img
#rm -rf ../../android_kernels/zip_togari/Zombie.zip

ARCH=arm CROSS_COMPILE=~/$TCHAIN make distclean

# build kernel
ARCH=arm CROSS_COMPILE=~/$TCHAIN make rhine_togari_row_defconfig
ARCH=arm CROSS_COMPILE=~/$TCHAIN make -j5
#cp arch/arm/boot/zImage-dtb ../../android_kernels/build_togari

# build image
#cd ../../android_kernels/build_togari

#./mkbootimg --base 0x00000000 --kernel zImage-dtb --ramdisk_offset 0x02000000 --tags_offset 0x01E00000 --pagesize 2048 --cmdline "androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3b7 ehci-hcd.park=3 androidboot.bootdevice=msm_sdcc.1 vmalloc=300M dwc3.maximum_speed=high dwc3_msm.prop_chg_detect=Y
#" --ramdisk ramdisk.cpio.gz -o boot.img

#cp boot.img ../../android_kernels/zip_togari

#cd ../../android_kernels/zip_togari

#zip -r Zombie.zip META-INF boot.img

#cd ../../android_kernels/kernel_togari
