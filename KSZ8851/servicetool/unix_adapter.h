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


#define MAX_MCAST_LST         32
#define HW_MCAST_SIZE         8

/* Receive multiplex framer header info */
struct type_frame_head {
   u16   sts;         /* Frame status */
   u16   len;         /* Byte count */
};

union ks_tx_hdr {
   u8      txb[4];
   __le16  txw[2];
};

struct ks_net {
   struct net_device *netdev;
   void __iomem      *hw_addr;
   void __iomem      *hw_addr_cmd;
   union ks_tx_hdr      txh ____cacheline_aligned;
   struct mutex         lock; /* spinlock to be interrupt safe */
   struct platform_device *pdev;
   struct mii_if_info   mii;
   struct type_frame_head  *frame_head_info;
   spinlock_t     statelock;
   u32         msg_enable;
   u32         frame_cnt;
   int         bus_width;

   u16         rc_rxqcr;
   u16         rc_txcr;
   u16         rc_ier;
   u16         sharedbus;
   u16         cmd_reg_cache;
   u16         cmd_reg_cache_int;
   u16         promiscuous;
   u16         all_mcast;
   u16         mcast_lst_size;
   u8       mcast_lst[MAX_MCAST_LST][ETH_ALEN];
   u8       mcast_bits[HW_MCAST_SIZE];
   u8       mac_addr[6];
   u8                      fid;
   u8       extra_byte;
   u8       enabled;
};

extern u8 ks_rdreg8(struct ks_net *ks, int offset);
u16 ks_rdreg16(struct ks_net *ks, int offset);
void ks_wrreg8(struct ks_net *ks, int offset, u8 value);
void ks_wrreg16(struct ks_net *ks, int offset, u16 value);

#endif
