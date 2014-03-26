//OSPF数据报首部
struct SpfPkt
{
    byte vers;		// 8位OSPF版本号 1
    byte type;		// 8位类型 1
    uns16 plen;		// 16位数据报长度 2
    rtid_t srcid;	// Router ID 4
    aid_t p_aid;	// Area ID 4
    uns16 chksum;		// 校验和 2
    uns16 autype;	// 认证类型（0 无，1 明文，2 密码） 2
    union  		// 认证数据 8
    {
        char aubytes[8];
        uns32 auwords[2];
        struct
        {
            uns16 crypt_mbz;
            byte keyid;
            byte audlen;
            uns32 seqno;
        } crypt;
    } un;
};



struct LSRef
{
    uns32 ls_type;	// Link state type
    lsid_t ls_id;	// Link State ID
    rtid_t ls_org;	// Advertising router
};



enum
{
    SPT_HELLO = 1,	// Hello
    SPT_DD = 2,		// DD
    SPT_LSREQ = 3,	// LSR
    SPT_UPD = 4,	// LSU
    SPT_LSACK = 5,	// ACK
};

enum
{
    AUT_NONE = 0,	// 无验证
    AUT_PASSWD = 1,	// 明文
    AUT_CRYPT = 2,	// 密码
};



struct HelloPkt
{
    SpfPkt header;
    uns32 Hello_mask;
    uns16 Hello_hint;
    byte Hello_opts;
    byte Hello_pri;
    uns32 Hello_dint;
    rtid_t Hello_dr;
    rtid_t Hello_bdr;
};



struct DDPkt
{
    SpfPkt header;
    uns16 dd_mtu;
    byte dd_opts;
    byte dd_imms;	// Init/more/master/slave
    uns32 dd_seqno;
};



enum
{
    SPO_TOS = 0x01,	// TOS capability
    SPO_EXT = 0x02,	// Flood external-LSAs? (i.e., not stub)
    SPO_MC = 0x04,	// Running MOSPF?
    SPO_NSSA = 0x08,	// NSSA area?
    SPO_PROP = 0x08,	// Propagate LSA across areas (NSSA)
    SPO_EA = 0x10,	// Implement External attributes LSA?
    SPO_DC = 0x20,	// Implement demand circuit extensions?
    SPO_OPQ = 0x40, // Implement Opaque-LSAs?
};

enum
{
    DD_INIT = 0x04,	// Init
    DD_MORE = 0x02,	// More
    DD_MASTER = 0x01,	// Master/salve
};

typedef SpfPkt ReqPkt;		// LS Req
typedef SpfPkt AckPkt;		// LS Update

struct UpdPkt
{
    SpfPkt header;
    uns32 upd_no;
};
