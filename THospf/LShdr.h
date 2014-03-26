//LSA
struct LShdr
{
    age_t ls_age;       //年龄
    byte ls_opts;       //选项
    byte ls_type;       //LSA类型
    lsid_t ls_id;       //LS ID
    rtid_t ls_org;      //产生路由
    seq_t ls_seqno;     //LS序列号
    xsum_t ls_xsum;     //LS校验和
    uns16 ls_length;    //LS长度

    LShdr& operator=(class LSA &);
    bool verify_cksum();
    void generate_cksum();
    inline bool do_not_age();
};


enum
{
    LST_RTR = 1,	// Router-LSAs
    LST_NET = 2,	// Network-LSAs
    MAX_LST = 2,
};

inline bool LShdr::do_not_age()
{
    return((ls_age & DoNotAge) != 0);
}



struct TOSmetric
{
    byte tos;   //非零TOS值
    byte hibyte;//metric高位
    uns16 metric;
};



struct RTRhdr
{
    byte rtype;//Router LSA类型
    byte zero;//没有使用
    uns16 nlinks;//连接数
};

enum
{
    RTYPE_B = 0x01, //边界路由ABR
    RTYPE_E = 0x02, //ASBR
    RTYPE_V = 0x04, //虚拟连接
};

struct RtrLink
{
    rtid_t link_id;
    InAddr link_data;
    byte link_type;
    byte n_tos;
    uns16 metric;
};

enum
{
    LT_PP = 1,      //点对点链接
    LT_TNET = 2,    //Transit网络
    LT_STUB = 3,    //STUB网络
    LT_VL = 4,      //虚拟链接网络
};

//network-LSA头部
struct NetLShdr
{
    InAddr netmask;//网络掩码
};


