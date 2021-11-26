make distclean
cp yzconfig .config
make zImage -j8
make socfpga_cyclone5_socdk.dtb
#make modules -j8
#make INSTALL_MOD_PATH=/home/hanglory/nfs_share/yz modules_install
