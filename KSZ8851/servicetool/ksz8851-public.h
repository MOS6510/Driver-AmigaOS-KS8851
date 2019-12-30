/**
 * Public Interface of the kzs8851 Network Driver
 */

#ifndef ksz8851_public_h
#define ksz8851_public_h

//TODO: Right?
#define MAC_MULTICAST_FILTER_SIZE 10

#define __start_packed
#define __end_packed __attribute__((packed))
#define UNUSED __attribute__((unused))

#define NIC_LINK_SPEED_100MBPS 100
#define NIC_LINK_SPEED_10MBPS  10
#define NIC_FULL_DUPLEX_MODE   1
#define NIC_HALF_DUPLEX_MODE   2

#define error_t int
#define uint_t uint16_t

/**
 * Ethernet Address
 */
typedef struct
 {
    union
    {
       uint8_t b[6];
       uint16_t w[3];
    } __end_packed;
 } __end_packed MacAddr;

/**
 * Mac Filter entry...
 */
 typedef struct
 {
    MacAddr addr;    ///<MAC address
    uint_t refCount; ///<Reference count for the current entry
 } MacFilterEntry;

/**
 * Common network device interface.
 */
typedef struct _NetInterface {
   void * nicContext;                     //Pointer to the "private part" of the driver

   //General driver access methods
   error_t (*init)(struct _NetInterface *);
   void (*deinit) (struct _NetInterface *);
   void (*reset)  (struct _NetInterface *, uint8_t methodFuncOp);
   void (*online) (struct _NetInterface *);
   void (*offline)(struct _NetInterface *);
   void (*processEvents)(struct _NetInterface *);
   error_t (*sendPacket)(struct _NetInterface *, uint8_t * buffer, size_t length);
   bool (*sendPacketPossible)(struct _NetInterface *, uint16_t size);
   void (*getDefaultNetworkAddress)(struct _NetInterface *, MacAddr *);


   //Accessing the TCP-Stack (from drivers Side): TODO move out...
   void (*rxPacketFunction)(MacAddr * src, MacAddr * dst, uint16_t packetType, uint8_t * buffer, uint16_t size );    //Function that process received packets (non-isr)
   void (*linkChangeFunction)(struct _NetInterface * interface);  //Function that processes links state changes (non-isr)

   int linkSpeed;                         //Link speed (100 or 10 MBit)
   int duplexMode;
   bool linkState;                        //connected or not
   MacAddr macAddr;                       //The used mac address of the NIC
   MacFilterEntry macMulticastFilter[MAC_MULTICAST_FILTER_SIZE];

} NetInterface;

/**
 * Delivers the main interfae to the driver...
 * @return
 */
extern const NetInterface * initModule(void);

#endif
