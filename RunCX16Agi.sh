#!/bin/bash
set -euo pipefail

# ---- Constants / Arguments ----
SDCARD_IMAGE_FOLDER="/home/alay/cx16Emu"        # folder where image lives
SDCARD_IMAGE_FILE="sdcard.img"                  # actual image filename
SDCARD_MOUNT_POINT="/mnt/cx16Img"               # mount point
CX16_EMULATOR_FOLDER="/home/alay/cx16Emu"       # emulator folder
MAKEFILE_FOLDER="/home/alay/CLionProjects/CX16Agi/"   # makefile folder
OPTIONAL_ARGUMENTS=""                           # optional args for emulator
LOOP_DEVICE="/dev/loop20"

# ---- Script ----

# Make sure loop device is free
if losetup | grep -q "$LOOP_DEVICE"; then
    echo "Detaching $LOOP_DEVICE (already in use)..."
    sudo losetup -d "$LOOP_DEVICE"
fi

# Go to the makefile folder and build
cd "$MAKEFILE_FOLDER"
sudo make clean
sudo make

# Full image path
IMG_PATH="$SDCARD_IMAGE_FOLDER/$SDCARD_IMAGE_FILE"

# Attach image to loop device (with partitions)
sudo losetup -P "$LOOP_DEVICE" "$IMG_PATH"

# Mount partition 1 of the loop device
sudo mount -o uid=$(id -u),gid=$(id -g) "${LOOP_DEVICE}p1" "$SDCARD_MOUNT_POINT"

pwd

# Copy files to the mounted SD card and emulator folder
cp agi.cx16* "$SDCARD_MOUNT_POINT/"
cp agi.cx16* "$CX16_EMULATOR_FOLDER/"

# Rename files in current folder to uppercase (Linux is case-sensitive)
for f in agi.cx16*; do
    mv "$f" "$(echo "$f" | tr '[:lower:]' '[:upper:]')"
done

# Unmount and detach loop device
sudo umount "$SDCARD_MOUNT_POINT"
sudo losetup -d "$LOOP_DEVICE"

# Go to emulator folder
cd "$CX16_EMULATOR_FOLDER"

# Run emulator with or without optional arguments
if [ -z "$OPTIONAL_ARGUMENTS" ]; then
    ./x16emu -sdcard "$IMG_PATH" -prg "agi.cx16" -run -debug d -echo > /home/alay/CLionProjects/CX16Agi/output.txt -dump V  -warp
else
    ./x16emu -sdcard "$IMG_PATH" -prg "agi.cx16" -run -debug d -echo $OPTIONAL_ARGUMENTS > /home/alay/CLionProjects/CX16Agi/output.txt  -dump V
fi

# Return to makefile folder
cd "$MAKEFILE_FOLDER"
