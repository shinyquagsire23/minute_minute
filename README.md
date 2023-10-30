# minute_minute

doot dooo do do doot

boot1 replacement based on minute, includes stuff like DRAM init and PRSH handling.

## redNAND

redNAND allows replacing one or more of the Wii Us internal storage devices (SLCCMPT, SLC, MLC) with partitions on the SD card. redNAND is implemented in stroopwafel, but configured through minute. The SLC and SLCCMPT parition are without the ECC/HMAC data. \
To prepare an SD card for usage with minute you can either use the `Format redNAND` option in the `Backup and Restore` menu or partition the SD card manually on the PC.

### Format redNAND

The `Format redNAND` option will erase the FAT32 partition and recreate it smaller to make room for the redNAND partitions. If it is already small enough it won't be touched. All other partitions will get deleted an the three redNAND partitions will be created and the content of the real devices will be cloned to them.

### Partition manually

You don't need all three partitions, only the ones you intend to redirect. The first partition in the MBR (not necessarily the pyhsical order) needs to be the FAT32 partition. The order of the redNAND partitions doesn't matter, as long as they are not the first one. Only primary partitions are supported. The purpose of the partition is identified by it's ID (file system type).
The types are:

- SLCCMPT: `0x0D`
- SLC: `0x0E` (FAT16 with LBA)
- MLC (needs SCFM): `0x83` (Linux ext2/ext3/ext4)
- MLC (no SCFM): `0x07` (NTFS)

Windows Disk Mangement doesn't support multiple partitions on SD cards, so you need to use a third party tool like Minitool Partition Wizard. The SLC partitions need to be exaclty 512MiB (1048576 Sectors). If you want to write a SLC.RAW image form minute or and slc.bin from the nanddumper to it, you first need to strip the ECC data from it. If you want to use an exiting MLC image of a 32GB console, the MLC Partition needs to be exactly 60948480 sectors.

### SCFM

SCFM is a block level write cache for the MLC which resides on the SLC. This creates a coupeling between to SLC and the MLC, which needs to be consistent at all times. You can not restore one without the other. This also means using the red MLC with the sys SLC or the other way around is not allowed unless explicitly enabled to prevent damage to the sys nand. \
MLC only redirection is still possible by disabeling SCFM. But that requires a MLC, which is consitent without SCFM. There are two ways to achive that. Either [rebuid a fresh MLC](https://gbatemp.net/threads/how-to-upgrading-rebuilding-wii-u-internal-memory-mlc.636309/) on the redNAND partition or use a MLC Dump which was obtained through the [recovery_menu](https://github.com/jan-hofmeier/recovery_menu/releases). Format the Partition to NTFS before writing the backup to change the ID to the correct type. The MLC clone obtained bye `Format redNAND` option requires SCFM, same for the mlc.bin obtained bei the original nanddumper.

### redNAND configuration

redNAND is configured by the [sd:/minute/rednand.ini](config_example/rednand.ini) config file.
In the `partitions` section you configure which redNAND partitions should be used. You can ommit partitions that you don't want to use, but minute will warn about ommited if the parition exists on the SD. \
In the `scfm` section you configure the SCFM options. `disable` will disable the SCFM, which is required for MLC only redirection. Minute will also check if the type of the MLC partition matches this setting. The `allow_sys` allows configurations that would make your sys scfm inconsitent. This option is strongly discuraged and can will lead to corruption and dataloss on the sys nand if you don't know what you are doing.
