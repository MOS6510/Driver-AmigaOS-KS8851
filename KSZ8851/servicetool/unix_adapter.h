#ifndef UNIX_ADAPTER_H
#define UNIX_ADAPTER_H

/**
 * Defining some stuff to make the linux driver compile for AmigaOS.
 */
#include <string.h>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define __le16 unsigned short
#define __iomem volatile
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

struct sockaddr {
   u8 sa_data[4];
};

#define BMCR_FULLDPLX 1

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
   char mc[8];
   char dev_addr[8];
   int addr_len;
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

struct netdev_hw_addr {
   u8 addr[8];
};

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

#define irqreturn_t unsigned char

unsigned int ioread8(void __iomem *addr);
unsigned int ioread16(void __iomem *addr);
unsigned int ioread32(void __iomem *addr);
void iowrite8(u8 value, void __iomem *addr);
void iowrite16(u16 value, void __iomem *addr);

#define MODULE_DESCRIPTION(x)
#define MODULE_AUTHOR(c);
#define MODULE_LICENSE(x);
#define module_param_named(a,b,c,d);
#define MODULE_PARM_DESC(a,b)


//################## Somewhere from Linux kernel includes  ##################################################

#define list_for_each_entry(pos, head, member)           \
   for (pos = list_entry((head)->next, typeof(*pos), member);  \
        &pos->member != (head);  \
        pos = list_entry(pos->member.next, typeof(*pos), member))

#define netdev_hw_addr_list_for_each(ha, l) \
   list_for_each_entry(ha, &(l)->list, list)

#define netdev_hw_addr_list_count(x) 0
//#define netdev_hw_addr_list_count(l) ((l)->count)
#define netdev_mc_count(dev) netdev_hw_addr_list_count(&(dev)->mc)
#define netdev_mc_empty(dev) netdev_hw_addr_list_empty(&(dev)->mc)
#define netdev_for_each_mc_addr(ha, dev) for(;;)
#define netdev_warn(...)

#define SET_NETDEV_DEV(a,b)
#define ENOMEM 1
#define alloc_etherdev(size) (0l)
#define netdev_priv(net_device) (0l)


/*\
   netdev_hw_addr_list_for_each(ha, &(dev)->mc)*/

#define bool u8
enum probe_type {
   DUMMY = 1
};

struct module {
};

struct device {
   void * platform_data;
   void * driver_data;
};

struct platform_device_id {
};

struct platform_device {
   struct device  dev;
};

#define pm_message_t int

/**
 * Linux: The basic device driver structure
 */
struct device_driver {
   const char     *name;
   struct bus_type      *bus;

   struct module     *owner;
   const char     *mod_name;  /* used for built-in modules */

//   bool suppress_bind_attrs;  /* disables bind/unbind via sysfs */
   enum probe_type probe_type;

   const struct of_device_id  *of_match_table;
   const struct acpi_device_id   *acpi_match_table;

   int (*probe) (struct device *dev);
   int (*remove) (struct device *dev);
   void (*shutdown) (struct device *dev);
   int (*suspend) (struct device *dev, pm_message_t state);
   int (*resume) (struct device *dev);
   const struct attribute_group **groups;

   const struct dev_pm_ops *pm;
   void (*coredump) (struct device *dev);

   struct driver_private *p;
};


struct platform_driver {
   int (*probe)(struct platform_device *);
   int (*remove)(struct platform_device *);
   void (*shutdown)(struct platform_device *);
   int (*suspend)(struct platform_device *, pm_message_t state);
   int (*resume)(struct platform_device *);
   struct device_driver driver;
   const struct platform_device_id *id_table;
   bool prevent_deferred_probe;
};

#define netdev_err(a, ...)
#define netdev_info(a, ...)

//struct net_device * alloc_etherdev (int sizeof_priv);


//from "mii.h":

/* Generic MII registers. */
#define MII_BMCR     0x00  /* Basic mode control register */
#define MII_BMSR     0x01  /* Basic mode status register  */
#define MII_PHYSID1     0x02  /* PHYS ID 1                   */
#define MII_PHYSID2     0x03  /* PHYS ID 2                   */
#define MII_ADVERTISE      0x04  /* Advertisement control reg   */
#define MII_LPA         0x05  /* Link partner ability reg    */
#define MII_EXPANSION      0x06  /* Expansion register          */
#define MII_CTRL1000    0x09  /* 1000BASE-T control          */
#define MII_STAT1000    0x0a  /* 1000BASE-T status           */
#define  MII_MMD_CTRL      0x0d  /* MMD Access Control Register */
#define  MII_MMD_DATA      0x0e  /* MMD Access Data Register */
#define MII_ESTATUS     0x0f  /* Extended Status             */
#define MII_DCOUNTER    0x12  /* Disconnect counter          */
#define MII_FCSCOUNTER     0x13  /* False carrier counter       */
#define MII_NWAYTEST    0x14  /* N-way auto-neg test reg     */
#define MII_RERRCOUNTER    0x15  /* Receive error counter       */
#define MII_SREVISION      0x16  /* Silicon revision            */
#define MII_RESV1    0x17  /* Reserved...                 */
#define MII_LBRERROR    0x18  /* Lpback, rx, bypass error    */
#define MII_PHYADDR     0x19  /* PHY address                 */
#define MII_RESV2    0x1a  /* Reserved...                 */
#define MII_TPISTATUS      0x1b  /* TPI status for 10mbps       */
#define MII_NCONFIG     0x1c  /* Network interface config    */



//################## Hardware base ##########################################################################

// Base address of the KSZ8851 ethernet chip in Amiga memory address space
#define ETHERNET_BASE_ADDRESS (u32)0xd90000l
#define KS8851_REG_DATA_OFFSET 0x00
#define KS8851_REG_CMD_OFFSET  0x02




#define devm_platform_ioremap_resource(a,b) (u32)(ETHERNET_BASE_ADDRESS + KS8851_REG_DATA_OFFSET)

//Define some functions that are not realy used:
#define netif_dbg(...)
#define mdelay(x) Delay(x)
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define pr_err(...)
#define netif_info(...)
#define mutex_lock(...)
#define mutex_unlock(...)
void free_irq ( unsigned int irq, void * dev_id);
#define spin_lock(...)
#define spin_unlock(...)


//TODO: Has to be implemented:
struct sk_buff * netdev_alloc_skb ( struct net_device * dev, unsigned int   length);
void skb_reserve (   struct sk_buff * skb, int len);
unsigned char * skb_put (  struct sk_buff * skb, unsigned int   len);
#define __be16 u16
__be16 eth_type_trans ( struct sk_buff * skb, struct net_device * dev);
int netif_rx ( struct sk_buff * skb);
void netif_carrier_on  ( struct net_device * dev);
void netif_carrier_off ( struct net_device * dev);
void netif_wake_queue ( struct net_device * dev);
typedef irqreturn_t (*irq_handler_t)(int, void *);
int request_irq ( unsigned int   irq,
   irq_handler_t     handler,
   unsigned long     irqflags,
   const char *   devname,
   void *   dev_id);
void netif_start_queue (   struct net_device * dev);
void netif_stop_queue ( struct net_device * dev);
u16 cpu_to_le16(u16 var);
void disable_irq (   unsigned int irq);
void dev_kfree_skb(struct sk_buff *skb);
void enable_irq ( unsigned int irq);
void unregister_netdev (   struct net_device * dev);
void free_netdev (   struct net_device * dev);


//Modul magic:
#define module_platform_driver(...)
/*
#define module_platform_driver(__platform_driver) \
   module_driver(__platform_driver, platform_driver_register, \
         platform_driver_unregister)
#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
   return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
   __unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);
*/
static inline void *dev_get_drvdata(const struct device *dev)
{
   return dev->driver_data;
}

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
   return dev_get_drvdata(&pdev->dev);
}






#endif
