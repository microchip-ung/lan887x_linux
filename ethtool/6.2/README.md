Ethtool support for TC10
==========================

Contents:

1. Supported kernel versions
2. Build and Installation Procedure

1. Supported Platforms & kernel version
----------------------
    - x86/x64 bit PC
	- Linux kernel version v6.1.x

4. Build and Installation Procedure
-----------------------------------
    The following instructions work fine for a PC build environment, embedded
    platforms may need slight build modifications, consult your platform documentation.
    a. Obtain the v6.2 source tree and build it.
    b. Update sources as following

	Copy and overwrite <ethtool-workspace>/netlink/extapi.h
	Copy and overwrite <ethtool-workspace>/netlink/settings.c
	Copy and overwrite <ethtool-workspace>/netlink/tc10.c
	Copy and overwrite <ethtool-workspace>/uapi/linux/ethtool_netlink.h
	Copy and overwrite <ethtool-workspace>/uapi/linux/ethtool.h
	Copy and overwrite <ethtool-workspace>/ethtool.c
	Copy and overwrite <ethtool-workspace>/Makefile.am

	c. Build updated sources

	make distclean && ./autogen.sh && ./configure && make && sudo make install
