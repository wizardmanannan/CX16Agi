SDCARD_IMAGE_FOLDER="/home/alay/cx16Emu" # folder where image lives
SDCARD_IMAGE_FILE="sdcard.img" # actual image filename
SDCARD_MOUNT_POINT="/mnt/cx16Img" # mount point
CX16_EMULATOR_FOLDER="/home/alay/cx16Emu" # emulator folder
MAKEFILE_FOLDER="/home/alay/CLionProjects/CX16Agi/" # makefile folder
OPTIONAL_ARGUMENTS="" # optional args for emulator
LOOP_DEVICE="/dev/loop120"
# ---- Script ----
# Unmount if currently mounted
if mountpoint -q "$SDCARD_MOUNT_POINT"; then
    echo "Unmounting $SDCARD_MOUNT_POINT..."
    sudo umount "$SDCARD_MOUNT_POINT"
fi

# Detach the loop device if it is attached
if losetup | grep -q "$LOOP_DEVICE"; then
    echo "Detaching $LOOP_DEVICE..."
    sudo losetup -d "$LOOP_DEVICE"
fi
