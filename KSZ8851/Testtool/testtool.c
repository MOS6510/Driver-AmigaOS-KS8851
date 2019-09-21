/**
 * Integration test Etherbridge Device
 */


#include <clib/alib_protos.h>
#include <clib/alib_stdio_protos.h>
#include <clib/graphics_protos.h>
#include <strings.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/types.h>
#include <devices/sana2.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include "devdebug.h"
#include "janus/janus.h"
#include "janus/inline_janus.h"


#include "libnix/libinit.h"

#include "myassert.h"
#include "etherbridge_service.h"
#include "etherbridge.h"
#include "jan_int.h"
#include "tools.h"
#include "keyboard.h"

//#define TEST_JANUS_LIBRARY_BUG 0



//I can't include <stdio.h> for some reason so I need to define some little stuff here:
typedef void FILE;
int    setvbuf __P((void *, char *, int, size_t));
#define  stdout   (__sF[1])
extern FILE **__sF;
#define  _IONBF   2 /* setvbuf should set unbuffered */
FILE  *fopen __P((const char *, const char *));
size_t fread __P((void *, size_t, size_t, FILE *));


static const BYTE __attribute__((aligned(16))) BROADCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

#define COLOR02   "\033[32m"
#define COLOR03   "\033[33m"
#define NORMAL    "\033[0m"


#define EB_BURSTOUT 0x2000

#define DEVICE_NAME "etherbridge.device"

static int packetsize = 100;
static int AsmCopyFromBuffCounter = 0;
static int AsmCopyToBuffCounter   = 0;
static const char * DEVICE_PATH = "DEVS:networks/" DEVICE_NAME;
static struct MsgPort * port      = NULL;
static struct IOSana2Req * s2req  = NULL;
static BYTE deviceUnitToOpen      = 0;

//Testpacket that is used to send tests:
static unsigned char Packet[1500] = {0x18};

BOOL checkStackSpaceUnittest(int size);


// ###### LOCAL PROTOTYPES #######

extern struct Hook filterHook;
#if TEST_JANUS_LIBRARY_BUG
static void closeJanusLib(void);
#endif

// ######## LOCAL MACROS ########

#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

// ###### EXTERNALS ########

extern struct bridge_ether * w_param;
extern void exit(int);
extern struct ExecBase * SysBase;
extern ULONG CopyBuf(APTR funcPtr, ULONG to, ULONG from, ULONG len);

#if 0
extern struct Library * JanusBase;
#endif



/**
 * An real ethernet address
 */

#define ETHERNET_ADDRESS_LEN 6

typedef struct {
   UBYTE address[ETHERNET_ADDRESS_LEN];
} EthernetAddress;

static void printStringWithoutCrLf(const char * strStr)
{
   char c;
   while((c = (char)*strStr) != 0) {
      if (c == 0x0a || c == 0x0d) {
         strStr++;
         continue;
      }

      printf("%c", c);

      strStr++;
   }
}


/**
 * Copy Ethernet Address (6 bytes) as fast as possible!
 * This is the fastest version in C I know.
 *
 * @param src
 * @param dst
 */
static inline void fastCopyEthernetAddress( EthernetAddress * src,  EthernetAddress * dst) {
    ULONG * lsrc = (ULONG*)src->address;
    ULONG * ldst = (ULONG*)dst->address;

    //Will result into 3 Assembler lines. But could be made by hand in 2 lines!

   *ldst++          = *lsrc++;         //first 4 bytes of the address
   *(USHORT*)ldst   = *(USHORT*)lsrc;  //last  2 bytes of the address
}

void testCopyEthernetAddresses() {
   EthernetAddress src = {{1,2,3,4,5,0x81}};
   UBYTE dst[8] = {0,0,0,0,0,0,0,0}; //2 bytes longer => 2 x DWORD's'
   printf("testCopyEthernetAddresses...");

   fastCopyEthernetAddress(&src,(EthernetAddress*)dst);

   assertEquals(1, dst[0]);
   assertEquals(2, dst[1]);
   assertEquals(3, dst[2]);
   assertEquals(4, dst[3]);
   assertEquals(5, dst[4]);
   assertEquals(0x81, dst[5]);
   assertEquals(0, dst[6]); //should be unchanged
   assertEquals(0, dst[7]); //should be unchanged
   printf("success\n");
}

// Buffer Management function to copy packet memory
static __saveds
ULONG AsmCopyFromBuff(REG(a0, APTR to),
                      REG(a1, APTR from),
                      REG(d0, ULONG len))
{
   ++AsmCopyFromBuffCounter;
   CopyMem(from,to,len);

   //Trash any other register to force an crash!
   //asm volatile("move.l #%0,a0\n move.l #%0,a1\n move.l #%0,a2\n move.l #%0,a3\n move.l #%0,a4\n move.l #%0,a5\n move.l #%0,a6\n" : : "m" (0x18181818l) :  "memory" );
   //asm volatile("move.l #%0,d1\n move.l #%0,d2\n move.l #%0,d3\n move.l #%0,d4\n move.l #%0,d5\n move.l #%0,d6\n" : : "m" (0x18181818l) :  "memory" );

   return 1;
}

// Buffer Management function to copy packet memory
static __saveds
ULONG AsmCopyToBuff(
      REG(a0, APTR from),
      REG(a1, APTR to),
      REG(d0, ULONG len))
{
   ++AsmCopyToBuffCounter;
   CopyMem(from,to,len);

   //Destroy any other register to force an crash!
   //Ein Stack darf das. Da Etherbridge device muss sich schützen!
   //asm volatile("move.l #%0,a0\n move.l #%0,a1\n move.l #%0,a2\n move.l #%0,a3\n move.l #%0,a4\n move.l #%0,a5\n move.l #%0,a6\n" : : "m" (0x0) :  "memory" );
   //asm volatile("move.l #%0,d1\n move.l #%0,d2\n move.l #%0,d3\n move.l #%0,d4\n move.l #%0,d5\n move.l #%0,d6\n" : : "m" (0x0) :  "memory" );

   return 1;
}

// Packet filter hook
static __saveds
ULONG asmFilterHookFunction(
      REG(a0, struct Hook * hook),
      REG(a2, struct IOSana2Req * ioreq),
      REG(a1, APTR pktData)) {

   assertEquals(&filterHook, hook);
   assertEquals(ioreq, s2req);
   assertEquals(Packet,pktData);

   //Ein Stack darf das. Da Etherbridge device muss sich schützen!

   //asm volatile("move.l #%0,a0\n move.l #%0,a1\n move.l #%0,a2\n move.l #%0,a3\n move.l #%0,a4\n move.l #%0,a5\n move.l #%0,a6\n" : : "m" (0x0) :  "memory" );
   //asm volatile("move.l #%0,d1\n move.l #%0,d2\n move.l #%0,d3\n move.l #%0,d4\n move.l #%0,d5\n move.l #%0,d6\n" : : "m" (0x0) :  "memory" );

   return 1; // => D0, OK
}

static struct Hook filterHook = {
      .h_Entry = &asmFilterHookFunction,
      .h_SubEntry = NULL,
      .h_Data = 0l
};

//Used Sana2 (V2) Buffer Management Functions
static struct TagItem BMTags[]={
      { S2_CopyToBuff,   (ULONG)&AsmCopyToBuff},
      { S2_CopyFromBuff, (ULONG)&AsmCopyFromBuff},
      { S2_PacketFilter, (ULONG)&filterHook},
      { TAG_DONE, 0}};


static void testCallFilterHook() {
   printf("testCallFilterHook...");

   //Get the Hook back...
   struct IOSana2Req localSana2Request;
   struct TagItem *bufftag = FindTagItem(S2_PacketFilter,BMTags);
   assert(bufftag != 0l);
   struct Hook * foundFilterHook = (struct Hook *)bufftag->ti_Data;
   assertEquals(&filterHook, foundFilterHook);
   s2req = &localSana2Request; // => so that in the filter the assert check will work!

   ULONG res = CallFilterHook(foundFilterHook, &localSana2Request, &Packet);

   //Must be the value the filter hook returns
   assertEquals(1, res);

   //cleanup...
   s2req = NULL;
   printf("success\n");
}

static void testBufferManagementCallsInSendDirection() {
   printf("testBufferManagementCallsInSendDirection...");
   ULONG buffers[] = {0, 0x55555555 /*dst*/, 0x11111111, 0xaaaaaaaa /*src*/, 0x22222222};

   struct TagItem * tagCopyFromBuffer = FindTagItem(S2_CopyFromBuff,BMTags);
   assert(tagCopyFromBuffer);
   SANA2_CFB functionCopyFromBuffer = (SANA2_CFB)tagCopyFromBuffer->ti_Data;
   assert(functionCopyFromBuffer);
   assert(buffers[1] == 0x55555555);
   assert(buffers[3] == 0xaaaaaaaa);
   AsmCopyFromBuffCounter = 0;

   ULONG res = CopyBuf(functionCopyFromBuffer, (ULONG)&buffers[1], (ULONG)&buffers[3], sizeof(ULONG) );
   assert(res == 1);

   assertEquals(0x00000000, buffers[0]);
   assert(0x11111111 == buffers[2]);
   assert(0x22222222 == buffers[4]);
   assertEquals(buffers[1], 0xaaaaaaaa);
   assertEquals(buffers[3], 0xaaaaaaaa);
   assertEquals(1,AsmCopyFromBuffCounter);

   printf("success\n");
}

static void testBufferManagementCallsInReceiveDirection() {
   printf("testBufferManagementCallsInReceiveDirection...");
   ULONG buffers[] = {0, 0x55555555 /*dst*/, 0x11111111, 0xaaaaaaaa /*src*/, 0x22222222};

   struct TagItem * tagCopyToBuffer = FindTagItem(S2_CopyToBuff,BMTags);
   assert(tagCopyToBuffer);
   SANA2_CTB functionCopyToBuffer = (SANA2_CTB)tagCopyToBuffer->ti_Data;
   assert(functionCopyToBuffer);
   assertEquals(buffers[1], 0x55555555);
   assertEquals(buffers[3], 0xaaaaaaaa);
   AsmCopyToBuffCounter = 0;

   ULONG res = CopyBuf(functionCopyToBuffer, (ULONG)&buffers[1], (ULONG)&buffers[3], sizeof(ULONG) );
   assert(res == 1);

   assert(0x00000000 == buffers[0]);
   assert(0x11111111 == buffers[2]);
   assert(0x22222222 == buffers[4]);
   assertEquals(buffers[1], 0x55555555);
   assertEquals(buffers[3], 0x55555555);
   assertEquals(1, AsmCopyToBuffCounter);

   printf("success\n");
}

/**
 * Flush the device if possible. Device will be flushed even when the open count is not zero.
 * In this case device will set an flags und flushed when the last opener close the device.
 *
 * @param deviceName
 * @return
 */
static BOOL flushDevice(const char * deviceName) {

   struct Library *devicePointer;
   struct Node *node;
   BOOL res = false;
   BOOL found = false;

   printf("flush.");
   Forbid();
   for (node = SysBase->DeviceList.lh_Head; node->ln_Succ; node = node->ln_Succ) {
      devicePointer = (struct Library *) node;

      if (devicePointer->lib_Node.ln_Name) {
         if (0 == strcmp(deviceName, devicePointer->lib_Node.ln_Name)) {
            found = true;
//            if (devicePointer->lib_OpenCnt != 0) {
//               printf("Warning: device is in use ( count=%ld ). Will not flush. \n", (ULONG)devicePointer->lib_OpenCnt);
//            }

            //Call DevExpunge()...this is valid even when the device is in use.
            //In this case the device would set the LIBF_DELEXP Flag...
            RemDevice((struct Device *) devicePointer);

            //Device request Delay Expunge? Should only happen when device is not closed!
            if (devicePointer->lib_Flags & LIBF_DELEXP) {
               printf("delayed.\n");
            }
            res = TRUE;

            break;
         }
      }
   }
   printf("flushed.");
   Permit();

//   if (!found) {
//      printf(" Device %s is not in memory. Nothing to expunge.\n", DEVICE_NAME);
//   }

   return res;
}


/*
 * Helper function to display Sana2 IOError...
 */
static void PrintIOError(struct IOSana2Req * s2){
   if (s2 == NULL) {
      return;
   }
   switch(s2->ios2_Req.io_Error)
   {
      case S2ERR_NO_ERROR     :       puts("S2ERR_NO_ERROR\n");
      break;
      case S2ERR_OUTOFSERVICE :       puts("S2ERR_OUTOFSERVICE");
      break;
      case S2ERR_MTU_EXCEEDED :       puts("S2ERR_MTU_EXCEEDED");
      break;
      case S2ERR_NO_RESOURCES :       puts("S2ERR_NO_RESOURCES");
      break;
      case S2ERR_BAD_STATE    :       puts("S2ERR_BAD_STATE");
      break;
      case IOERR_ABORTED      :       puts("IOERR_ABORTED");
      break;
      default                 :       printf(" io_Error(0x%x)\n",s2->ios2_Req.io_Error);
      break;
   }

   switch(s2->ios2_WireError)
   {
      case S2WERR_BUFF_ERROR:         puts(" S2ERR_BUFF_ERROR");
      break;

      case S2WERR_GENERIC_ERROR:      puts(" S2WERR_GENERIC_ERROR");
      break;

      default:
         printf("S2ERR_... (%ld)\n", s2->ios2_WireError);
         break;
   }
}


void createPortAndDeviceRequest(){

   assert(s2req == NULL);
   assert(port == NULL);

   port = CreatePort("Disk-Device",0);
   assert(port);
   s2req = (struct IOSana2Req*)CreateExtIO(port,sizeof(struct IOSana2Req));
   assert(s2req);
   assert(((ULONG)s2req & 1) == 0); //check of aligned 16 bit...
   s2req->ios2_Req.io_Device = (struct Device*)-1;
   s2req->ios2_BufferManagement = &BMTags;
}

void closePortAndRequest(){

   if (s2req) {
      //Check that device was closed before. If it is open this would be an memory leak.
      //assertEquals((struct Device*)-1, s2req->ios2_Req.io_Device);
      assertEquals((struct Unit*)-1, s2req->ios2_Req.io_Unit);

      //Free now unsed IORequest:
      DeleteExtIO(&s2req->ios2_Req);
      s2req = NULL;
   }
   if (port) {
      DeletePort(port);
      port = NULL;
   }
}

void closeDevice(){
   //Close Device BEFORE port and request!
   if (NULL == s2req) {
      printf("closeDevice(): No IORequest anymore.\n");
      return;
   }

   if (s2req->ios2_Req.io_Device == (struct Device *)-1 ||
         s2req->ios2_Req.io_Unit == (struct Unit *)-1) {
      printf("closeDevice(): Device is already closed.\n");
      return;
   }

   //last IORequest (if any) must be finished and not pending (NT_REPLYMSG means "was replied back to port".
   assertEquals( s2req->ios2_Req.io_Message.mn_Node.ln_Type, NT_REPLYMSG );

   CloseDevice((void *)s2req);

   //When device is closed device it should mark the iorequest as invalid:
   if((struct Device *)-1 != s2req->ios2_Req.io_Device || (struct Unit *)-1 != s2req->ios2_Req.io_Unit) {
      printf("closeDevice(): Warning: device is not marked as invalid after close? (io_Device=0x%lx)!\n", (ULONG)s2req->ios2_Req.io_Device);
   }

   //Delay(TICKS_PER_SECOND * 2);
}

BOOL openDevice(const char * devicePath){
   const int FLAGS = 0;
   assert(s2req);
   //printf("open.");
   BYTE res = OpenDevice(devicePath, deviceUnitToOpen, (struct IORequest*)s2req, FLAGS);
   //printf("open called.");
   if (s2req->ios2_Req.io_Error != 0 || res != 0) {
      printf("Open Device Failed: res=%d,\n io_Error=%ld\n io_Device=0x%lx\n io_Unit=0x%lx\n", res, (LONG)s2req->ios2_Req.io_Error,
            (LONG) s2req->ios2_Req.io_Device, (LONG)s2req->ios2_Req.io_Unit);
      PrintIOError(s2req);
      return FALSE;
   }
   //Valid Device?
   assert(s2req->ios2_Req.io_Device != (struct Device*)-1);

   //Device must change the TagList to something else...
   assert(s2req->ios2_BufferManagement != BMTags);

   return TRUE;
}

/**
 * Perform an sync command to the device.
 * @param cmd cmd to be executed
 * @return true when success, false when not ok.
 */
BOOL performDeviceRequest(UWORD cmd) {
   assert(s2req);
   assert(s2req->ios2_Req.io_Device != (struct Device *)-1);
   s2req->ios2_Req.io_Command = cmd;
   if ( DoIO( (void*) s2req) != 0 ) {
      return FALSE;
   }
   return TRUE;
}

void performAsyncDeviceRequest(UWORD cmd) {
   assert(s2req);
   assert(s2req->ios2_Req.io_Device != (struct Device *)-1);
   s2req->ios2_Req.io_Command = cmd;
   SendIO( (void*) s2req);
}

void testopenDeviceWithInvalidUnitNumber() {
   printf("testopenDeviceWithInvalidUnitNumber...");
   createPortAndDeviceRequest();
   deviceUnitToOpen = 1;

   BOOL openRes = openDevice(DEVICE_PATH);
   deviceUnitToOpen = 0;

   assert(openRes == FALSE);
   closePortAndRequest();
   printf("succeeded\n");
}

static char line[200] = {0};

static const char * readLine(FILE * fd)
{
   size_t size;
   int ptr = 0;
   line[0] = 0;
   while((size = fread(&line[ptr],1,1,fd)) != 0 && (ptr < sizeof(line) ) )
   {
      if (line[ptr] == 0x0d || line[ptr] == 0x0a)
      {
         if (ptr<2) { ptr = 0; continue; }

         line[ptr+1] = 0;
         return line;
      }
      ++ptr;
   }
   return (size != 0) ? line : NULL;
}

static void showDeviceConfig()
{
   const char * cline;
   FILE * fd = fopen("env:sana2/etherbridge.config", "r");
   puts("--------------- Used device configuration -------------");

   if (fd != NULL)
   {
      while((cline = readLine(fd)) != NULL )
      {
         if (cline[0] == '#') continue;

         printf("%s",cline);
      }

      fclose((ULONG)fd);
   }
   puts("--------------- end of config -------------------------");
}


void testOpenDeviceUnitNullShowVersion(){
   printf("testOpenDeviceUnitNullShowVersion...");

   createPortAndDeviceRequest();
   if (openDevice(DEVICE_PATH))
   {
      //Check for right function pointers:
      struct Library * deviceLib = &s2req->ios2_Req.io_Device->dd_Library;
      printf("\nNeg=%d, Pos=%d, version Major=%d, version minor=%d:\n",
            deviceLib->lib_NegSize, deviceLib->lib_PosSize, deviceLib->lib_Version, deviceLib->lib_Revision, (char*)deviceLib->lib_IdString);
      printStringWithoutCrLf((const char*)deviceLib->lib_IdString);
      printf("\n");

      // MUST have 6 Device functions!
      assertEquals(36, deviceLib->lib_NegSize);

      TEST_LEAVE(true);

      closeDevice();
   }

   closePortAndRequest();
}

void testConfigureDevice(){
   int res = false;


   printf("testConfigureDevice...");
   createPortAndDeviceRequest();
   if (openDevice(DEVICE_PATH)) {

      //First request the station address
      performDeviceRequest(S2_GETSTATIONADDRESS);
      assertEquals( S2ERR_NO_ERROR, s2req->ios2_Req.io_Error );
      printf("1.");

      //Configure the device with that address
      copyEthernetAddress(s2req->ios2_DstAddr, s2req->ios2_SrcAddr);
      performDeviceRequest(S2_CONFIGINTERFACE);
      assertEquals(S2ERR_NO_ERROR, s2req->ios2_Req.io_Error);
      printf("2.");

      //A second try to configure should fail!
      performDeviceRequest(S2_CONFIGINTERFACE);
      assertEquals(S2ERR_BAD_STATE, s2req->ios2_Req.io_Error);
      assertEquals(S2WERR_IS_CONFIGURED, s2req->ios2_WireError);
      printf("3.");

      //device should also be online now.
      //An DoEvent ONLINE should return immediately
      s2req->ios2_WireError = S2EVENT_ONLINE;
      res = performDeviceRequest(S2_ONEVENT);
      assertEquals(S2ERR_NO_ERROR, s2req->ios2_Req.io_Error);
      assertEquals(S2EVENT_ONLINE, s2req->ios2_WireError);
      printf("4.");

      res = TRUE;
      closeDevice();

      puts("succeeded");
   }
   closePortAndRequest();

   assert(res);
}

void testSendPackets(){
   int i;
   printf("testSendPackets...");
   createPortAndDeviceRequest();
   assertEquals(true, openDevice(DEVICE_PATH));
   performDeviceRequest(S2_CONFIGINTERFACE); //only to be sure config + online

   s2req->ios2_Req.io_Flags   = IOF_QUICK;
   s2req->ios2_Data           = Packet;
   s2req->ios2_DataLength     = packetsize;
   s2req->ios2_PacketType     = 0x800;
   copyEthernetAddress(BROADCAST,s2req->ios2_DstAddr);
   for(i=0;i<10;i++) {
      assertEquals(true, performDeviceRequest(CMD_WRITE));
   }
   puts("succeeded");

   closeDevice();
   closePortAndRequest();
}

void testSendPacketsAsBroadcasts(){
   int i;
   printf("testSendPacketsAsBroadcasts...");
   createPortAndDeviceRequest();
   assertEquals(true, openDevice(DEVICE_PATH));

   s2req->ios2_Req.io_Flags   = IOF_QUICK ;
   s2req->ios2_Data           = Packet;
   s2req->ios2_DataLength     = packetsize;
   s2req->ios2_PacketType     = 0x800;
   for(i=0;i<10;i++) {
      assertEquals(true, performDeviceRequest(S2_BROADCAST));
   }

   puts("succeeded");

   closeDevice();
   closePortAndRequest();
}

void testBurstTest(){
   printf("testBurstTest...30 pkts");
   createPortAndDeviceRequest();
   assertEquals(true, openDevice(DEVICE_PATH));

   s2req->ios2_Data           = 0;
   s2req->ios2_DataLength     = 30; //20 packets
   s2req->ios2_PacketType     = 0x800;
   if (!performDeviceRequest(EB_BURSTOUT)) {
      assertEquals(0, s2req->ios2_Req.io_Error);
   }

   puts("succeeded");

   closeDevice();
   closePortAndRequest();
}

static void testReadAndAbort() {

   const int packetsToBeRead = 30;
   printf("testReadAndAbort..30 pkts..");

   flushDevice(DEVICE_NAME);
   printf("0.");
   createPortAndDeviceRequest();
   assert(openDevice(DEVICE_PATH));
   printf("1.");
   //First request the station address
   performDeviceRequest(S2_GETSTATIONADDRESS);
   assertEquals(S2ERR_NO_ERROR, s2req->ios2_Req.io_Error);
   printf("2.");
   //Configure the device with that address
   copyEthernetAddress(s2req->ios2_DstAddr, s2req->ios2_SrcAddr);
   performDeviceRequest(S2_CONFIGINTERFACE);
   assert(S2ERR_NO_ERROR == s2req->ios2_Req.io_Error || S2ERR_BAD_STATE == s2req->ios2_Req.io_Error);
   printf("3.");

   int i;
   for(i=0;i<packetsToBeRead;i++) {

      //Use a packet type to receive that doesn't exists because we don't want to receive anything here...
      s2req->ios2_PacketType = 0x18;

      //=> Queuing request only, do not wait...
      performAsyncDeviceRequest(CMD_READ);
      assertEquals(S2ERR_NO_ERROR, s2req->ios2_Req.io_Error );
      //Request must be pending...
      assertEquals(NT_MESSAGE, s2req->ios2_Req.io_Message.mn_Node.ln_Type);

      //=> Abort IORequest again...
      AbortIO((struct IORequest*)s2req);
      assertEquals(IOERR_ABORTED, s2req->ios2_Req.io_Error );

      //Ensure that the iorequest is really finished aborting.
      assertEquals(IOERR_ABORTED, WaitIO((struct IORequest*)s2req) );

      printf("(%d)", i);
   }

   printf("4.");

   closeDevice();

   printf("5.");

   closePortAndRequest();

   printf("succeeded.\n");
}

void testGetNextFreeBufFirstFree() {
   printf("testGetNextFreeBufFirstFree...");
   w_param = AllocMem(sizeof(struct bridge_ether), MEMF_ANY);
   assert(w_param);

   w_param->cmd[0] = 0x01;
   w_param->cmd[1] = 0x80;

   char result = getNextFreeTransmitBufferIndexWait();

   assertEquals(0, result);

   FreeMem(w_param, sizeof(struct bridge_ether));

   puts("succeeded");
}

void testGetNextFreeBufSecondFree() {
   printf("testGetNextFreeBufSecondFree...");
   w_param = AllocMem(sizeof(struct bridge_ether), MEMF_ANY);
   assert(w_param);

   w_param->cmd[0] = 0x80;
   w_param->cmd[1] = 0x1;

   char result = getNextFreeTransmitBufferIndexWait();

   assertEquals(1, result);

   FreeMem(w_param, sizeof(struct bridge_ether));

   puts("succeeded");
}

void testGetNextFreeBufNothingFree() {

   printf("testGetNextFreeBufNothingFree...");
   w_param = AllocMem(sizeof(struct bridge_ether), MEMF_ANY);
   assert(w_param);

   w_param->cmd[0] = 0x80;
   w_param->cmd[1] = 0x80;

   char result = getNextFreeTransmitBufferIndexWait();

   assertEquals(-1, result);

   FreeMem(w_param, sizeof(struct bridge_ether));

   puts("succeeded");
}

void testDelayOneSecond(){
   printf("testDelayOneSecond...");
   Forbid();
   clock_t time1 = clock();

   Delay(TICKS_PER_SECOND * 2); //=> 50 ticks per second! = Should delay for one second...

   clock_t time2 = clock();
   clock_t diff = time2 - time1;
   Permit();

   printf("%ld clocks\n", (ULONG)diff);
   assertEqualsDelta(2*CLOCKS_PER_SEC, diff, 4 );
   puts("testDelayOneSecond...finished.");
}

void testGetSpecialStatistics() {
   struct Sana2SpecialStatHeader * stats = NULL;
   const int maxStats = 20;
   int sizeOfStatsStruct = 0;

   printf("testGetSpecialStatistics...");

   createPortAndDeviceRequest();
   if (!openDevice(DEVICE_PATH)) {
      puts("failed to open device");
      goto end;
   }

   sizeOfStatsStruct = sizeof(struct Sana2SpecialStatHeader) - 4 + maxStats * sizeof(struct Sana2SpecialStatRecord);
   stats = AllocMem(sizeOfStatsStruct, MEMF_ANY | MEMF_CLEAR);
   if (stats) {
      s2req->ios2_StatData = stats;
      stats->RecordCountMax = maxStats;
      stats->RecordCountSupplied = 0;
      if (!performDeviceRequest(S2_GETSPECIALSTATS)) {
         goto end;
      }

      //printf("Statistics: %ld. ", (ULONG) stats->RecordCountSupplied);
      assert(stats->RecordCountSupplied > 0);
      assert(stats->RecordCountSupplied < maxStats);
      assertEquals(9, stats->RecordCountSupplied);

      puts("succeeded");
   }

end:
   if (stats) {
      FreeMem(stats, sizeOfStatsStruct);
      stats = NULL;
   }
   closeDevice();
   closePortAndRequest();
}

/**
 * bcopy() was accidentally replaced by memcpy() which results in huge trouble!
 * They have different parameter orders! this should never happen again!
 */
void testBCopy() {
   printf("testBCopy...");
   BYTE src[6];
   BYTE dst[10];
   src[0] = 1;
   src[1] = 2;
   src[2] = 3;
   src[3] = 4;
   src[4] = 5;
   src[5] = 6;

   dst[0] = 11;
   dst[1] = 22;
   dst[2] = 33;
   dst[3] = 44;
   dst[4] = 55;
   dst[5] = 66;

   dst[6] = 77;
   dst[7] = 88;
   dst[8] = 99;
   dst[9] = 111;

   bcopy( src, dst, 6);

   assertEquals(dst[0], 1);
   assertEquals(dst[1], 2);
   assertEquals(dst[2], 3);
   assertEquals(dst[3], 4);
   assertEquals(dst[4], 5);
   assertEquals(dst[5], 6);

   assertEquals(dst[6], 77);

   printf("success!\n");
}

void testCopyEthernAddressesToIOreq() {
   printf("testCopyEthernAddressesToIOreq...");
   struct IOSana2Req destReq;
   destReq.ios2_SrcAddr[0] = 01;
   destReq.ios2_SrcAddr[1] = 02;
   destReq.ios2_SrcAddr[2] = 03;
   destReq.ios2_SrcAddr[3] = 04;
   destReq.ios2_SrcAddr[4] = 05;
   destReq.ios2_SrcAddr[5] = 06;
   BYTE secure[4] = {18,19};
   pc_u_char from[6] = {11,12,13,14,15,16};

   copyEthernetAddress(from, destReq.ios2_SrcAddr);

   assertEquals(11, destReq.ios2_SrcAddr[0] );
   assertEquals(12, destReq.ios2_SrcAddr[1] );
   assertEquals(13, destReq.ios2_SrcAddr[2] );
   assertEquals(14, destReq.ios2_SrcAddr[3] );
   assertEquals(15, destReq.ios2_SrcAddr[4] );
   assertEquals(16, destReq.ios2_SrcAddr[5] );
   assertEquals(18, secure[0]);
   assertEquals(19, secure[1]);

   //Some code parts use the right command to address the SrcAddr of the IORequest.
   //Check that that the two variants are identical.
   assert((APTR)destReq.ios2_SrcAddr == (APTR)&destReq.ios2_SrcAddr);

   printf("success\n");
}

/**
 * Check that struct alignment is on WORD boundary...
 */
void testGeneralStructAlignment() {
   printf("testGeneralStructAlignment()\n");

   typedef struct {
      pc_u_char EthServerVer; // 1 Byte
      pc_u_int  EthServerRe;  // 2 bytes
   } TestStruct;

   // => + 1 padding = MUST be 4 bytes in size
   assertEquals(4, sizeof(TestStruct));
}

void cleanupAtExit(void) {

   puts("cleanupAtExit:");

   //In case that methods atexit() throws itself an assertion break the recursion here...
   static int recursiveCounter = 0;
   if (recursiveCounter++) {
      return;
   }

   closeDevice();
   printf("closePortAndRequest():");
   closePortAndRequest();
   printf("done\n");

   printf("final flush:");
   flushDevice(DEVICE_NAME);
   printf("done\n");

#if TEST_JANUS_LIBRARY_BUG
   closeJanusLib();
#endif

   puts("cleanupAtExit ends.");
}


//test that the task stack has enough space:
BOOL checkStackSpaceUnitTest(int size) {
   struct Task * thisTask = FindTask(0);
   int stackSize = thisTask->tc_SPUpper - thisTask->tc_SPLower;
   if (stackSize < size) {
      printf("Your stack size is too low! (%ld). Use at least 5k!\n", (ULONG)stackSize);
      return false;
   }

   printf("Stack size currently used: %ld (from %d)\n", (ULONG)thisTask->tc_SPUpper - (ULONG)thisTask->tc_SPReg, stackSize);
   return true;
}

static void testPrintEthernetAddress() {
   UBYTE etherAddr[6] = {1,2,3,4,5,6};

   printf("testPrintEthernetAddress(): \n");

   printf( "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n", 
   (ULONG)etherAddr[0],  (ULONG)etherAddr[1],  (ULONG)etherAddr[2],  (ULONG)etherAddr[3],  (ULONG)etherAddr[4],  (ULONG)etherAddr[5] );

#if DEBUG > 0
   DEBUGOUT((VERBOSE_HW,"\n\rEthernet: "));
   printEthernetAddress(etherAddr);
   DEBUGOUT((VERBOSE_HW,"\n\r"));
#endif
}

/**
 * Check that device is correct installed under: DEVS:networks and not DEVS: !
 */
static void testDeviceCorrectInstalled() {
   BPTR fd;

   printf("checkDeviceCorrectInstalled...");

   //SHOULD NOT installed in this place!
   fd = Open("DEVS:etherbridge.device", MODE_OLDFILE);
   assertEquals(0, fd);

   //This is the right place to be installed:
   fd = Open("DEVS:networks/etherbridge.device", MODE_OLDFILE);
   assert(fd != NULL);
   Close(fd);

   printf("success\n");
}

static void testFlushDeviceWhenStillOpen() {
   printf("testFlushDeviceWhenStillOpen...");
   createPortAndDeviceRequest();
   assert(openDevice(DEVICE_PATH)); //keep device it open

   flushDevice(DEVICE_NAME);

   //Device should set the "Delete Expunge Flag"
   assert( s2req->ios2_Req.io_Device->dd_Library.lib_Flags & LIBF_DELEXP );
   closeDevice();
   //TODO Check that device was expunged last at last DevClose();


   closePortAndRequest();
   printf("success\n");
}

const char *getcpu(void)
{
  UWORD attnflags = SysBase->AttnFlags;

  if (attnflags & AFF_68060) return "68060";
  if (attnflags & AFF_68040) return "68040";
  if (attnflags & AFF_68030) return "68030";
  if (attnflags & AFF_68020) return "68020";
  if (attnflags & AFF_68010) return "68010";
  return "68000";
}

const char * getfpu(void)
{
   UWORD attnflags = SysBase->AttnFlags;

   if (attnflags & AFF_68882) return "68882";
   if (attnflags & AFF_68881) return "68881";
   return "";
}

static void printCacheControl()
{
   ULONG cc = CacheControl(0,0);
   if (cc & CACRF_CopyBack) printf(" CopyBack");
   if (cc & CACRF_EnableI) printf(" ICache");
   if (cc & CACRF_EnableD) printf(" DCache");
   if (cc & CACRF_IBE) printf(" IBurst");
   puts("");
}

static void testEtherbridgeStructureSize()
{
   printf("Sizeof Etherbridge Struct in DPRAM = %d\n", sizeof(struct bridge_ether));
   assertEquals(13712, sizeof(struct bridge_ether));
}

//testing Janus Library bug...
#if 0

/**
 * Terminates the PC side via key press to the PC directly...
 */
void terminatePCSideServer() {
   executePCCommand("\n"); //Simple CR
}

void stopTask(struct Task * task)
{
   if (NULL == task) { puts("Unknown task!"); return; }

   Forbid();
   Remove (&task->tc_Node);
   task->tc_State = 0xff; //freeze...
   Enqueue ((struct List *) &SysBase->TaskWait, &task->tc_Node);
   Permit();
}

void continueTask(struct Task * task)
{
   if (NULL == task) { puts("Unknown task!"); return; }

   Forbid();
   Remove (&task->tc_Node);
   task->tc_State = TS_READY;
   Enqueue ((struct List *) &SysBase->TaskReady, &task->tc_Node);
   Permit();
}

void openJanusLib(void) {
   if (!JanusBase) {
      JanusBase = OpenLibrary("janus.library", 36l);
      if (JanusBase) {
         //printf("Janus Library Version %d.%d\n", JanusBase->lib_Version, JanusBase->lib_Revision);
      }
      else {
         puts("Unable to open Janus library!");
      }
   }
}

void closeJanusLib(void) {
   if (JanusBase) {
      CloseLibrary(JanusBase);
      JanusBase = NULL;
   }
}

/**
 * Tests the found "janus bug"
 */
void testJanusLibBug() {
   printf("testJanusLibBug...");
   struct ServiceData * serviceData = NULL;

   if (JanusBase) {
      //Take the etherbridge device...
      BYTE result = GetService(&serviceData, 0x181272, 1, 0, 0);
      if (result == JSERV_OK) {
         struct Task * task = FindTask("ZaphodServiceTask");
         if (NULL == task) {
            task = FindTask("ZesTask"); //in JanusLib 36.84 task was renamed (by me)!
         }
         assert(task);

         //Freeze task and release service...
         stopTask(task);
         WaitTOF();
         ReleaseService(serviceData);
         WaitTOF();
         continueTask(task);
         puts("end");
      } else {
         puts("Unable to get Etherbridge device from PC side...");
      }
   } else {
      puts("Janus library not open.");
   }
}
#endif


int main(int argc, char * argv[])
{
   int repeats = 0;
   struct Task * thisTask = FindTask(NULL);
   //Switch off stout buffering...
   setvbuf(stdout, NULL, _IONBF, 20 );
   debugLevel = 0; //1000;

   //If given get the count of repeats of the unit tests
   if (argc >= 2) {
      repeats = atoi(argv[1]);
      printf("Repeating unit test %d times...\n", repeats);
   }

   //Cleanup some stuff when exit...
   atexit (cleanupAtExit);

   printf(COLOR02);
   printf("Etherbridge Unit and Integration Tests\n");
   printf("Unit test compiled: %s %s\n", __DATE__, __TIME__);
   printf(NORMAL);
   printf("Testing device %s...\n\n", DEVICE_PATH);
   printf("CPU/FPU: %s %s", getcpu(), getfpu() ); printCacheControl();
   printf("Exec Library: V%d.%d, IdString = %s",
         SysBase->LibNode.lib_Version, SysBase->LibNode.lib_Revision,
         (const char*)SysBase->LibNode.lib_IdString);
   DEBUGOUT((1,"Unittest taskID = 0x%lx\n", thisTask));
   printf("This process: 0x%lx (%s)\n", (ULONG)thisTask, thisTask->tc_Node.ln_Name);
#if TEST_JANUS_LIBRARY_BUG
   openJanusLib();
   if (JanusBase)
      printf("Janus Library Version %d.%d\n", JanusBase->lib_Version, JanusBase->lib_Revision);
#endif
   checkStackSpaceUnitTest(5000);
   showDeviceConfig();

   //Unit Tests:
   puts("###### Unit Test Run #####");
   testEtherbridgeStructureSize();
   testCopyEthernetAddresses();
   testPrintEthernetAddress();
   testBCopy();
   testCallFilterHook();
   testBufferManagementCallsInSendDirection();
   testBufferManagementCallsInReceiveDirection();
   testCopyEthernAddressesToIOreq();
   testGeneralStructAlignment();
   testDelayOneSecond();
   testGetNextFreeBufFirstFree();
   testGetNextFreeBufSecondFree();
   //testGetNextFreeBufNothingFree();

   do {
      printf("\n############## Integration Rest, still to run %d ########################\n\n", (int)repeats);

      //Integration tests:
      flushDevice(DEVICE_NAME);
      testopenDeviceWithInvalidUnitNumber();
      testFlushDeviceWhenStillOpen();
      testDeviceCorrectInstalled();
      testOpenDeviceUnitNullShowVersion();
      testGetSpecialStatistics();
      testConfigureDevice();
      testSendPackets();
      testBurstTest(); //<= bis hier ok
      testReadAndAbort();
      //testJanusLibBug();

      puts("Tests finished");

      //Every 5th run terminate the PC server...
#if TEST_JANUS_LIBRARY_BUG
      if (i % 5 == 0) {
         terminatePCSideServer();
      }
#endif
      cleanupAtExit();

   } while (repeats--);

   return 0;

#if 0

        struct MsgPort * port      = 0;
        struct IOSana2Req * s2req  = 0;

        int i;
        ULONG ulMSek=0;
        char bBurstTest=0;

        for(i=0;i<1500;i++)
        {
           Packet[i] = 0x18; //(i & 0xff);
        }

        //default ist mein Device zu oeffnen....
        strncpy(Devname,"DEVS:networks/etherbridge.device",100);


        if (argc<2)
        {
           printf("Usage: %s <Packet Count> (<device> oder BURSTTEST) \n",argv[0]);
           exit(0);
        }
        else
        {
           sscanf(argv[1],"%d",&packetanz);
        }

        //Anderes Device angegeben?
        if(argc==3)
        {
           //oder Schluesselwort BURSTTEST?
           if(!strcmp("BURSTTEST",argv[2]))
           {
              //BurstTest
              bBurstTest = 1;
           }
           else
           {
              // Nein, Devicename
              strncpy(Devname,argv[2],100);
           }
        }


        for(i=0;i<1500;i++) Packet[i]=i;




        port  = CreatePort("Disk-Device",0);
        s2req = (struct IOSana2Req*)CreateExtIO(port,sizeof(struct IOSana2Req));
        s2req->ios2_BufferManagement=&BMTags;

        printf("Device oeffnen..."); fflush(stdout);
        err   = OpenDevice(Devname, 0, (void*)s2req, 0);
        if(err!=0)
        {
           printf("Fehler : ");
           PrintIOError(s2req);
        }
        else
        {
           puts("\n");

           // Device konfigurieren...
           printf("Device konfigurieren...");
           s2req->ios2_Req.io_Command = S2_CONFIGINTERFACE;
           if ( DoIO( (void*) s2req)!=0 )
           {
              printf("Fehler : ");
              PrintIOError(s2req);
           }
           else
              puts("\n");

           // gehe auf online...
           printf("Device auf Online schalten...");
           s2req->ios2_Req.io_Command = S2_ONLINE;
           if ( DoIO( (void*) s2req)!=0 )
           {
              printf("Fehler : ");
              PrintIOError(s2req);
              goto ende;
           }
           else
              puts("\n");

           if (bBurstTest)
           {
              //Raw Burst Test (geht nur mit etherbridge.device (CMD = 0x2000) )
              printf("Burst Test: %d Packete a 1500 Bytes:\n",packetanz);
              DateStamp(&von);

              s2req->ios2_Req.io_Flags   = 0;
              s2req->ios2_Req.io_Command = EB_BURSTOUT;
              s2req->ios2_Data           = 0;
              s2req->ios2_DataLength     = packetanz; //Anzahl der zu versendenten Packete
              s2req->ios2_PacketType     = 0;
              if (DoIO((void *)s2req)!=0)
              {
                printf("Fehler bei DoIO() : ");
                PrintIOError(s2req);
                goto ende;
              }

              DateStamp(&bis);

              ulMSek = CalcSeconds(&von,&bis);
              printf("Ergebnis\n");
              printf("--------\n");
              printf(" %.3f KB", 1500 * packetanz / 1000.0);
              printf(" in %ld Millisekunden\n",ulMSek);
              printf(" ==> %.3f KB / s\n", 1500 * packetanz / (double)ulMSek);


           }
           else
           {
              //Normale Packet verschicken
              printf("verschicke %d Packete a %d Bytes:\n",packetanz,packetsize);
              DateStamp(&von);
              for(i=0;i<packetanz ; i++)
              {
                s2req->ios2_Req.io_Flags   = IOF_QUICK ;
                //s2req->ios2_Req.io_Command = CMD_WRITE;
                s2req->ios2_Req.io_Command = S2_BROADCAST;
                s2req->ios2_Data           = Packet;
                s2req->ios2_DataLength     = packetsize;
                s2req->ios2_PacketType     = 0x800;
                if (DoIO((void *)s2req)!=0)
                {
                  printf("Fehler bei DoIO() : ");
                  PrintIOError(s2req);
                  goto ende;
                }
                //else
                //  printf(".\n");
              }
              DateStamp(&bis);

              ulMSek = CalcSeconds(&von,&bis);
              printf("Ergebnis\n");
              printf("--------\n");
              printf(" %.3f KB",packetsize * packetanz / 1000.0);
              printf(" in %ld Millisekunden\n",ulMSek);
              printf(" ==> %.3f KB / s\n",packetsize * packetanz / (double)ulMSek);



              /*
              PrintIOError(s2req);

              printf("Device wieder Offline\n");
              s2req->ios2_Req.io_Command=S2_OFFLINE;
              DoIO(s2req);*/

              PrintIOError(s2req);
           }


        }

        //PrintIOError(s2req);
        ende:

        if(!err)     { printf("Schliesse Device..."); CloseDevice((void *)s2req); printf("ok\n"); }
        if(s2req)    DeleteExtIO((void *)s2req);
        if(port)     DeletePort(port);

#endif

}
