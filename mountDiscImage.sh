SDCARD_IMAGE_FOLDER="/home/alay/cx16Emu"        # folder where image lives
SDCARD_IMAGE_FILE="sdcard.img"                  # actual image filename
SDCARD_MOUNT_POINT="/mnt/cx16Img"               # mount point
CX16_EMULATOR_FOLDER="/home/alay/cx16Emu"       # emulator folder
MAKEFILE_FOLDER="/home/alay/CLionProjects/CX16Agi/"   # makefile folder
OPTIONAL_ARGUMENTS=""                           # optional args for emulator
LOOP_DEVICE="/dev/loop140"

# ---- Script ----

# Make sure loop device is free
if losetup | grep -q "$LOOP_DEVICE"; then
    echo "Detaching $LOOP_DEVICE (already in use)..."
    sudo losetup -d "$LOOP_DEVICE"
fi

# Full image path
IMG_PATH="$SDCARD_IMAGE_FOLDER/$SDCARD_IMAGE_FILE"

# Attach image to loop device (with partitions)
sudo losetup -P "$LOOP_DEVICE" "$IMG_PATH"

# Mount partition 1 of the loop device
sudo mount -o uid=$(id -u),gid=$(id -g) "${LOOP_DEVICE}p1" "$SDCARD_MOUNT_POINT"
