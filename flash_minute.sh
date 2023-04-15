#!/bin/zsh

./build_copy.sh

disk="invalid"
for d in /dev/disk*; do
    val=$(diskutil info $d | grep -c 'Built In SDXC Reader')
    if [ "$val" -eq "1" ]; then
        disk=$d
        diskutil info $disk
        break
        #echo $disk
    fi
done

if [[ "$disk" == "invalid" ]]; then
    echo "No disk found to flash."
    exit -1
fi

disk_partition=$disk
disk_partition+="s1"

echo "Writing to: $disk"
diskutil mount $disk_partition
#sudo dd if=boot1.img of="$disk" status=progress
cp fw.img /Volumes/UNTITLED/fw.img
diskutil eject $disk
