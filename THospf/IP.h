typedef uns32 InAddr;
typedef	uns32 InMask;

//IP数据报首部

typedef struct InPkt
{
    byte i_vhlen;	// 4位版本和4位头部长度 1
    byte i_tos;		// 8位服务类型TOS 1
    uns16 i_len;	// 16位总长度 2
    uns16 i_id;		// 16位标识 2
    uns16 i_ffo;	// 3位标识和13位偏移 2
    byte i_ttl;		// 8位TTL 1
    byte i_prot; 	// 8位协议 1
    uns16 i_chksum;	// 16位首部检验和 2
    InAddr i_src; 	// 32位源IP地址 4
    InAddr i_dest; 	// 32位目的IP地址 4
} InPkt;

//IP常用数字
const byte IHLVER = 0x45;
const byte PROT_OSPF = 89;
const byte DEFAULT_TTL = 64;
const byte PREC_IC = 0xc0;
