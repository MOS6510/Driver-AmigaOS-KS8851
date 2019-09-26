#ifndef UNIX_ADAPTER_H
#define UNIX_ADAPTER_H

/**
 * Defining some stuff to make the linux driver compile for AmigaOS.
 */

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

#define irqreturn_t

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


#define netdev_for_each_mc_addr(ha, dev) for(;;)
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
};

struct platform_device_id {
};

struct platform_device {

};

#define pm_message_t int

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


//################## Hardware base ##########################################################################

// Base address of the KSZ8851 ethernet chip in Amiga memory address space
#define ETHERNET_BASE_ADDRESS (u32)0xd90000l
#define KS8851_REG_DATA_OFFSET 0x00
#define KS8851_REG_CMD_OFFSET  0x02




#define devm_platform_ioremap_resource(a,b) (u32)(ETHERNET_BASE_ADDRESS + KS8851_REG_DATA_OFFSET)



#endif
