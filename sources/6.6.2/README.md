Linux Device drivers for Microchip LAN7431 Ethernet Controller and LAN887X PHY
==============================================================================

Contents:

1. Supported Platforms & kernel versions
2. Device support
3. Driver structure and Source Files
4. Build and Installation Procedure


1. Supported Platforms & kernel version
----------------------
    - x86/x64 bit PC
	- Ethtool supported version is v6.2(build sources from ethtool/6.2 directory)


2. Device support
-----------------

    - EVB-LAN7431 Rev-2
	jumpers: j5, j9, j10, j11, j16(3.3v)

    - EVB-LAN8870 Rev-B(or later)
	jumpers: j4, VDD SEL - EDS, j15, j18, j20

3. Driver structure and Source files
------------------------------------
    This driver package will create the two modules (drivers):

    lan743x.ko:		Device driver for LAN7431
    microchip_t1.ko:	Device driver for LAN8870

    Source files:
	LAN8870 driver source files
	 - drivers/net/phy/Kconfig
	 - drivers/net/phy/microchip_t1.c
	 - drivers/net/phy/phy-c45.c
	 - drivers/net/phy/phy_device.c

	LAN743X driver source files
	 - drivers/net/ethernet/microchip/lan743x_ethtool.c 
	 - drivers/net/ethernet/microchip/lan743x_main.c
	 - drivers/net/ethernet/microchip/lan743x_main.h
	 - drivers/net/ethernet/microchip/lan743x_ptp.c

	Ethtool
	 - Documentation/networking/ethtool-netlink.rst
	 - net/ethtool/Makefile
	 - net/ethtool/netlink.c
	 - net/ethtool/netlink.h
	 - net/ethtool/tc10.c

	Include
	 - include/linux/ethtool.h
	 - include/linux/phy.h
	 - include/linux/mdio.h
	 - include/uapi/linux/ethtool_netlink.h
	 - include/uapi/linux/ethtool.h
	 - include/uapi/linux/mdio.h


4. Build and Installation Procedure
-----------------------------------
    The following instructions work fine for a PC build environment, embedded
    platforms may need slight build modifications, consult your platform documentation.

    a. Obtain the kernel source tree for the platform in use and build it.
	Reboot to the kernel you built.
    b. Update sources as following
	Copy and overwrite <Your-linux-version>/drivers/net/ethernet/microchip/lan743x_main.h
	Copy and overwrite <Your-linux-version>/drivers/net/ethernet/microchip/lan743x_main.c
	Copy and overwrite <Your-linux-version>/drivers/net/ethernet/microchip/lan743x_ethtool.c
	Copy and overwrite <Your-linux-version>/drivers/net/ethernet/microchip/lan743x_ptp.c
	Copy and overwrite <Your-linux-version>/drivers/net/phy/phy_device.c
	Copy and overwrite <Your-linux-version>/drivers/net/phy/phy-c45.c
	Copy and overwrite <Your-linux-version>/drivers/net/phy/microchip_t1.c
	Copy and overwrite <Your-linux-version>/drivers/net/phy/Kconfig
	Copy and overwrite <Your-linux-version>/Documentation/networking/ethtool-netlink.rst
	Copy and overwrite <Your-linux-version>/net/ethtool/Makefile
	Copy and overwrite <Your-linux-version>/net/ethtool/netlink.c
	Copy and overwrite <Your-linux-version>/net/ethtool/netlink.h
	Copy and overwrite <Your-linux-version>/net/ethtool/tc10.c
	Copy and overwrite <Your-linux-version>/include/linux/ethtool.h
	Copy and overwrite <Your-linux-version>/include/linux/phy.h
	Copy and overwrite <Your-linux-version>/include/linux/mdio.h
	Copy and overwrite <Your-linux-version>/include/uapi/linux/ethtool_netlink.h
	Copy and overwrite <Your-linux-version>/include/uapi/linux/ethtool.h
	Copy and overwrite <Your-linux-version>/include/uapi/linux/mdio.h

	Delete the object files if they exists.
	   rm drivers/net/ethernet/microchip/lan743x_ethtool.o
	   rm drivers/net/ethernet/microchip/lan743x_main.o
	   rm drivers/net/ethernet/microchip/lan743x_ptp.o
       rm drivers/net/phy/microchip_t1.o
    c. If you are compiling RL3 sources for the first time, run following commands. Otherwise follow from step(d) onwards.
	Note: run following commands to enable LAN743X and MICROCHIP_T1 kernel modules
		./scripts/config --set-val CONFIG_LAN743X m
		./scripts/config --set-val CONFIG_MICROCHIP_T1 m
	sudo make
	sudo make modules_install
	sudo make install
	sudo update-grub
	Reboot to the kernel you built.
    d. Compile drivers at the root of your linux-<linux-version> source tree.
	make drivers/net/ethernet/microchip/lan743x.ko
	make drivers/net/phy/microchip_t1.ko
	You should get no errors.
    e. Unload the existing drivers.
	sudo rmmod lan743x
	sudo rmmod microchip_t1
	Note that microchip_t1 module may not be loaded by default.
    f. Load the new drivers in order.
	sudo insmod drivers/net/phy/microchip_t1.ko
	sudo insmod drivers/net/ethernet/microchip/lan743x.ko
	Note this loading is not "install" drivers on the kernel and rebooting
	the PC will load the original in-kernel drivers.
	To see the driver kernel log, open a separate terminal window then run
	dmesg -w
	you can check if the drivers are loaded correctly.
