sudo umount ./test
sudo rmmod TEST_fs
dd bs=4096 count=100 if=/dev/zero of=image
./mkfs image
insmod TEST_fs.ko
mount -o loop -t TEST_fs image ./test
dmesg
sudo chmod 0777 ./test -R
