#ifndef UNIX_ADAPTER_H
#define UNIX_ADAPTER_H

//Doing stuff to make the linux driver parts compilable for AmigaOS

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define __le16 unsigned short
#define __iomem
#define ____cacheline_aligned

struct mutex {
   u8 dummy;
};

struct mii_if_info {
   u8 dummy;
};

typedef struct {
   u8 dummy;
} spinlock_t;

#define ETH_ALEN 8

#define hw 0

#define true 1
#define false 0

struct net_device {
   struct stats {
      int rx_dropped;
      int rx_frame_errors;
      int rx_length_errors;
      int rx_bytes;
      int rx_packets;
      int rx_over_errors;
      int tx_bytes;
      int tx_packets;
   } stats;
   int irq;
   int flags;
};

#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQF_TRIGGER_LOW 0
#define NETDEV_TX_BUSY 0
#define CRC32_POLY_BE 0
#define IFF_PROMISC 0
#define IFF_ALLMULTI 1
#define IFF_MULTICAST 2




#define netdev_tx_t int

struct sk_buff {
   u8 * data;
   int len;
   int protocol;
};

#define link 0
#define ifup 1
#define ifdown 0
#define NETDEV_TX_OK 1
#define NETDEV_TX_OK 1

#define irqreturn_t


unsigned int ioread8(void __iomem *addr);
unsigned int ioread16(void __iomem *addr);
unsigned int ioread32(void __iomem *addr);
void iowrite8(u8 value, void __iomem *addr);
void iowrite16(u16 value, void __iomem *addr);


// Base address of the chip in Amiga memory address space
#define ETHERNET_BASE_ADDRESS (u32)0xd90000l
#define CMD_REGSITER_OFFSET 0x02

#endif
