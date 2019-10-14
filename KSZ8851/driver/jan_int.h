/* 
 * Copyright (C) 1997 - 1999 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef JAN_INT_H
#define JAN_INT_H

#include <sys/types.h>

#include "device.h"


#define bool char
#define TRUE  1
#define true  1
#define FALSE 0
#define false 0

extern const UBYTE BROADCAST_ADDRESS[6];

#if DEBUG >0
void printEthernetAddress(UBYTE * addr);
#else
   #define printEthernetAddress(addr)
#endif

bool connectPCService(struct EtherbridgeUnit *etherUnit, char * sPCCmd, char * sPCPktDrvCmd, char * sPCPktDrvPar);
void disconnectPCService();
bool isEtherbridgeServiceDeleted();
void CallPC();
bool serviceReadPackets (struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice);
bool serviceWritePackets(struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice) ;
char getNextFreeTransmitBufferIndexWait();
LONG milliSecondsToTicks(ULONG milliSeconds);
void SetIrqAndIOBase(int IOBase, unsigned char Irq);
bool ShutDownPCServer();
void uninstallHandler();
BOOL installPollHandler(struct Task * forTask, ULONG signalBitForTask);
void uninstallPollHandler();
void SetPromMode(BOOL);
void ShutDownPktDrv();
unsigned int waitForCmdExecuted(UBYTE Port,int MaxMilliTime);
void copyEthernetAddress(const BYTE * from, BYTE * to);
ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData);


struct ip { //hier Big-Endian !!!
        u_char  ip_v:4,                 /* version */
                ip_hl:4;                /* header length */
        u_char  ip_tos;                 /* type of service */
        short   ip_len;                 /* total length */
        u_short ip_id;                  /* identification */
        short   ip_off;                 /* fragment offset field */
        u_char  ip_ttl;                 /* time to live */
        u_char  ip_p;                   /* protocol */
        u_short ip_sum;                 /* checksum */
        u_char  ip_src[4],ip_dst[4];    /* source and dest address */
};

struct udphdr {
        u_short uh_sport;               /* source port */
        u_short uh_dport;               /* destination port */
        short   uh_ulen;                /* udp length */
        u_short uh_sum;                 /* udp checksum */
};

struct tcphdr {
        u_short th_sport;               /* source port */
        u_short th_dport;               /* destination port */
        ULONG   th_seq;                 /* sequence number */
        ULONG   th_ack;                 /* acknowledgement number */
        u_char  th_off:4,               /* data offset */
                th_x2:4;                /* (unused) */
        u_char  th_flags;
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20
        u_short th_win;                 /* window */
        u_short th_sum;                 /* checksum */
        u_short th_urp;                 /* urgent pointer */
};


#define IPPROTO_IP              0               /* dummy for IP */
#define IPPROTO_ICMP            1               /* control message protocol */
#define IPPROTO_GGP             3               /* gateway^2 (deprecated) */
#define IPPROTO_TCP             6               /* tcp */
#define IPPROTO_EGP             8               /* exterior gateway protocol */
#define IPPROTO_PUP             12              /* pup */
#define IPPROTO_UDP             17              /* user datagram protocol */
#define IPPROTO_IDP             22              /* xns idp */

#define IPPROTO_RAW             255             /* raw IP packet */
#define IPPROTO_MAX             256



#endif
