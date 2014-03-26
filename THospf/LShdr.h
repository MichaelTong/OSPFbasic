//LSA
struct LShdr
{
    age_t ls_age;       //����
    byte ls_opts;       //ѡ��
    byte ls_type;       //LSA����
    lsid_t ls_id;       //LS ID
    rtid_t ls_org;      //����·��
    seq_t ls_seqno;     //LS���к�
    xsum_t ls_xsum;     //LSУ���
    uns16 ls_length;    //LS����

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
    byte tos;   //����TOSֵ
    byte hibyte;//metric��λ
    uns16 metric;
};



struct RTRhdr
{
    byte rtype;//Router LSA����
    byte zero;//û��ʹ��
    uns16 nlinks;//������
};

enum
{
    RTYPE_B = 0x01, //�߽�·��ABR
    RTYPE_E = 0x02, //ASBR
    RTYPE_V = 0x04, //��������
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
    LT_PP = 1,      //��Ե�����
    LT_TNET = 2,    //Transit����
    LT_STUB = 3,    //STUB����
    LT_VL = 4,      //������������
};

//network-LSAͷ��
struct NetLShdr
{
    InAddr netmask;//��������
};


