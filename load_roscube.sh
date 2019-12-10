if [ "$1" = "disable" ]
then
    echo "Unload Module"
    sudo sh -c "echo 0x21 > /sys/bus/i2c/devices/i2c-7/delete_device"
    sudo sh -c "echo 0x27 > /sys/bus/i2c/devices/i2c-7/delete_device"
    sudo modprobe -r  gpio_pca953x
    sudo modprobe -r i2c_i801
else
    echo "Load Module"
    sudo modprobe i2c_i801
    sudo modprobe gpio_pca953x
    sudo sh -c "echo pca9535 0x21 > /sys/bus/i2c/devices/i2c-7/new_device"
    sudo sh -c "echo pca9534 0x27 > /sys/bus/i2c/devices/i2c-7/new_device"
fi