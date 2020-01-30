# About
This is the AmigaOS network driver ("SANA2") for the build-in network chip ("Microchip KS8851") of the [Amiga 1200+ project](https://bitbucket.org/jvandezande/amiga-1200/src/master/).

Current beta Version (1.0 build 3) seems stable. Work is still in progress...

# Features
- AmigaOS >= 2.01
- Written in "C" very small parts of 68k assembler... ( yes! :-) )
- Parts are based on my old [Etherbridge project](https://www.heiko-pruessing.de/projects/etherbridge/eb.html)
- Amiga SANA2R3 compatible (S2_DMACopyFromBuff32 supported, S2_DMACopyToBuff32 still left)
- Working with Roadshow (others should also work but not yet tested)
- Speed: ftp pull: (about 230kb/s), ftp push: (about 430kb/s), measured with ACA1233-26 (68030/26) and a FTP server network + ftp command line tool from Roadshow distribution
- Device can use network chip in big endian or little endian mode (initial switch to big endian)
- Adjustable Ethernet mac address from config file (network chip in Amiga1200+ has no special eeprom for ethernet MAC address)
- Device supports a two layered architecture (low and highlevel) which may in future make it easy to port it to other network chips. Layers are:
  - Highlevel AmigaOS network device driver (complex)
  - Lowlevel driver which support raw access to the network ship itself
- makefile creates ADF images
  

# What is currently still not working?
- No network statistics are currently available: All are „zero"
- In receive direction, packet content is memory copied into temporary buffer which slows down speed a little bit
- Support for S2_DMACopyToBuff32 is still missing
- With „slow" network connection (10MBit) or with faster processors (>= 68040) transmitted packets may be lost in some situations
- IPv4 multicasts not supported yet
- loopback mode not supported yet
- promiscuous mode not supported yet
- Heavy use of Disable()/Enable() methods which may reduces responses in the multitasking environment of the AmigaOS a little bit
- No AmigaOS installer script yet (only a small shell script which copies the files into the right place)

# Binary Distribution
- Binary versions will be available soon.

# Installation 
The device is called "ksz8851.device" and must be installed in "devs:networks" of the AmigaOS installation.
There is a simple script which copies the device and a configuration file to the right place. No installer script now but will come in near future.

# Compile
You need a 68k cross-compiler to build like GNU gcc 3.3 (see [https://github.com/bebbo/amiga-gcc](https://github.com/bebbo/amiga-gcc) ).

On the bash execute:

```bash
make ARCH=020 
```
which builds the release version for MC68020. If you want the debug version than execute

```bash
make ARCH=020 debug
```

If you have connected your Mac or PC via serial cable with Amiga Explorer, you can directly install it on a real Amiga via

```bash
make ARCH=020 install
```

More comming soon...

# License
GPL V2.0
