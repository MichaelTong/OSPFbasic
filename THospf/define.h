#include <stdarg.h>
#include <string.h>
#include <time.h>



typedef unsigned char byte;	// Byte(8位无符号数)
typedef unsigned short uns16;	// 16位无符号数
typedef unsigned int uns32;		// 32位无符号数
typedef char int8;		// 8位有符号数
typedef short int16;		// 16位有符号数
typedef int int32;		// 32位有符号数

typedef unsigned long word;


const uns32 Infinity = 0xffffffffL;
const uns16 MAX_COST = 0xffff;
const uns16 VL_MTU = 1500;
const int MAXPATH = 4;


const int MODX = 4102;

typedef unsigned int socklen;


inline uns32 ntoh32(uns32 value)

{
    uns32 swval;

    swval = (value << 24);
    swval |= (value << 8) & 0x00ff0000L;
    swval |= (value >> 8) & 0x0000ff00L;
    swval |= (value >> 24);
    return(swval);
}


inline uns32 hton32(uns32 value)

{
    return(ntoh32(value));
}


inline uns16 ntoh16(uns16 value)

{
    uns16 swval;

    swval = (value << 8);
    swval |= (value >> 8) & 0xff;
    return(swval);
}


inline uns16 hton16(uns16 value)

{
    return(ntoh16(value));
}



