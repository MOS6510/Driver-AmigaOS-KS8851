/**
 * Generic public interface of a hardware near driver
 */

#ifndef _hardware_public_h
#define _hardware_public_h

//TODO: Right?
#define MAC_MULTICAST_FILTER_SIZE 10

#define __start_packed
#define __end_packed __attribute__((packed))
#define UNUSED __attribute__((unused))

#define NIC_LINK_SPEED_100MBPS 100
#define NIC_LINK_SPEED_10MBPS  10
#define NIC_FULL_DUPLEX_MODE   1
#define NIC_HALF_DUPLEX_MODE   2

#define uint_t uint16_t

#define error_t signed char

#define ERROR_WRONG_IDENTIFIER -1
#define ERROR_OUT_OF_MEMORY -2
#define NO_ERROR 0
#define ERROR_INVALID_LENGTH -3
#define ERROR_FAILURE -4
#define ERROR_INVALID_PACKET -5
#define NO_CHIP_FOUND -6

#define STACK_SIZE_MINIMUM 5000

#if DEBUG > 0
extern void traceout(char * format, ...);
#define TRACE_INFO traceout
#define TRACE_DEBUG traceout
#else
#define TRACE_INFO(...)
#define TRACE_DEBUG(...)
#endif

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

   //Reference to the "private part" of the driver ("Device Context")
   void * nicContext;

   //Probe for available hardware (check if hardware is available)
   error_t (*probe)(struct _NetInterface *);

   //General driver access methods
   error_t (*init)(struct _NetInterface *);

   //Destructor
   void (*deinit) (struct _NetInterface *);
   void (*reset)  (struct _NetInterface *, uint8_t methodFuncOp);

   //go online (ISR will be also installed)
   void (*online) (struct _NetInterface *);
   void (*offline)(struct _NetInterface *);

   //Get the used signal number for low level access of the hardware, set in "online")
   ULONG (*getUsedSignalNumber)(struct _NetInterface *);

   bool (*processEvents)(struct _NetInterface *);
   error_t (*sendPacket)(struct _NetInterface *, uint8_t * buffer, size_t length);
   bool (*sendPacketPossible)(struct _NetInterface *, uint16_t size);
   void (*getDefaultNetworkAddress)(struct _NetInterface *, MacAddr *);

   const char * (*getConfigFileName)(void);


   //Accessing the TCP-Stack (from drivers Side): TODO move out...
   void (*onPacketReceived)(uint8_t * rawPacketEthernet, uint16_t size ); //Function that process received packets (non-isr)
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
extern NetInterface * initModule(void);

#endif
