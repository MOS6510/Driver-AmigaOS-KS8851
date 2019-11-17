#ifndef __DEVDEBUG__
#define __DEVDEBUG__ 


//Bit masks for debugging
#define VERBOSE_DEVICE   1<<1
#define VERBOSE_HW       1<<2
#define VERBOSE_CONFFILE 1<<3


#if DEBUG > 0
#define DEBUGOUT(x)  printdebug x
#define DBEUGOUTI printi
void printdebug(short dbLevel, char * format, ...);
void print(short pri, char * text);
void printi(short pri, unsigned long int wert1);
#else
#define DEBUGOUT(x)
#define DBEUGOUTI
#define print(x,y)
#define printi(x,y)
#endif

extern int    debugLevel;

#endif
