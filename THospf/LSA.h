#ifndef LSA_H
#define LSA_H

class Interface;
class TLink;

class LSA : public TblItem
{
protected:
    // LS头部，机器字节序存储
    age_t lsa_rcvage;	// 收到LSA时的年林
    byte lsa_opts;	// LS 选项
    const byte lsa_type;	// 类型
    seq_t lsa_seqno;	// 序列号
    xsum_t lsa_xsum;	// 校验和
    uns16 lsa_length;	// LSA字节长度

    byte *lsa_body;	// LSA 内容
    class Interface *lsa_ifp;// 链接范围内LSA的接口
    LSA	*lsa_agefwd;	// 老化列表中向前列表
    LSA	*lsa_agerv;	// 老化列表中向后列表
    uns16 lsa_agebin;	// 老化列表
    uns16 lsa_rxmt;	// 所在重发列表的数目，也是必须对泛洪进行确认的相邻路由的数目
    uns16 in_agebin:1,	// 在老化列表中？
          deferring:1,	// 等待延迟重新创建
          changed:1,	// 自从上次泛洪之后发生了变化
          exception:1,	//需要拷贝整个LSA
          rollover:1,	// 序号开始滚动
          e_bit:1,	// Type-2 external metric
          parsed:1,	// 被解析进行路由计算
          sent_reply:1,	// 发送对接收到旧的LSA的回复
          checkage:1,	// 排队等待校验
          min_failed:1,	// MinArrival失败
          we_orig:1;	// 自己生成
    uns16 lsa_hour;	// 停留在数据库中的小时数

    static LSA *AgeBins[MaxAge+1];// 老化列表
    static int Bin0;	// 年龄为0的元素
    static int32 RefreshBins[MaxAgeDiff]; // 刷新列表
    static int RefreshBin0; // 当前刷新的元素

    void hdr_parse(LShdr *hdr);
    virtual void parse(LShdr *);
    virtual void unparse();
    virtual void build(LShdr *);
    virtual void delete_actions();

public:
    RtrTblEntry	*source;	// Routing table entry of orig. rtr

    LSA(class Interface *, LShdr *, int blen = 0);
    virtual ~LSA();

    inline byte ls_type();
    inline lsid_t ls_id();
    inline rtid_t adv_rtr();
    inline seq_t ls_seqno();
    inline uns16 ls_length();
    inline bool do_not_age();
    inline age_t lsa_age();
    inline int is_aging();
    inline age_t since_received();

    int	cmp_instance(LShdr *hdr);
    int	cmp_contents(LShdr *hdr);
    void start_aging();
    void stop_aging();
    int	refresh(seq_t);
    void flood(class Neighbor *from, LShdr *hdr);

    virtual void reoriginate(int) {}
    virtual RtrTblEntry *rtentry();
    virtual void update_in_place(LSA *);

    friend class OSPF;
    friend class Neighbor;
    friend class Interface;
    friend class LsaListIterator;
    friend class DBageTimer;
    friend void hdr_parse(LSA *, LShdr *);
    friend LShdr& LShdr::operator=(class LSA &lsa);
    friend inline uns16 Age2Bin(age_t);
    friend inline age_t Bin2Age(uns16);
    friend void lsa_flush(LSA *);
};

// Inline functions
inline byte LSA::ls_type()
{
    return(lsa_type);
}
inline lsid_t LSA::ls_id()
{
    return((lsid_t) index1());
}
inline rtid_t LSA::adv_rtr()
{
    return((rtid_t) index2());
}
inline seq_t LSA::ls_seqno()
{
    return(lsa_seqno);
}
inline uns16 LSA::ls_length()
{
    return(lsa_length);
}
inline bool LSA::do_not_age()
{
    return((lsa_rcvage & DoNotAge) != 0);
}

inline	uns16 Age2Bin(age_t x)
{
    if (x <= LSA::Bin0)
        return (LSA::Bin0 - x);
    else
        return (MaxAge+1 + LSA::Bin0 - x);
}

inline	age_t Bin2Age(uns16 x)
{
    return((age_t) Age2Bin(x));
}

inline	int LSA::is_aging()
{
    if (!valid())
        return(false);
    else if ((lsa_rcvage & DoNotAge) != 0)
        return(false);
    else if (lsa_rcvage == MaxAge)
        return(false);
    else
        return(true);
}

inline	age_t LSA::lsa_age()
{
    return (is_aging() ? Bin2Age(lsa_agebin) : lsa_rcvage);
}

inline	age_t LSA::since_received()
{
    if (is_aging())
        return (lsa_age() - lsa_rcvage);
    else
        return (Bin2Age(lsa_agebin));
}


//泛洪范围
enum
{
    LocalScope = 1,
    AreaScope,
    GlobalScope,
};
 //Router LSA和Network LSA的基类，用于最短路径计算
class TNode : public LSA, public Q_Elt
{
protected:
    class Link *t_links; // 中继链接
    RtrTblEntry	*t_dest;	// 目的路由表

    byte dijk_run:1,	// 是否在计算路由
         t_direct:1;	// 直接与跟节点相连？

    byte t_state;	// 三种状态：UNINIT，ONTREE，ONCAND
    TNode *t_parent;	// SPF树的父母节点

    EPath *t_epath;	// 路径
public:
    TNode(LShdr *, int blen);
    virtual ~TNode();
    void tlp_link(TLink *tlp);
    void unlink();
    void dijk_install();
    void add_next_hop(TNode *parent, int index);
    bool has_members(InAddr group);
    InAddr ospf_find_gw(TNode *parent, InAddr, InAddr);

    friend class OSPF;
    friend class netLSA;
    friend class rtrLSA;
    friend class RtrTblEntry;
};

enum
{
    DS_UNINIT = 0, 	// 未初始化
    DS_ONCAND,		// 在候选表上
    DS_ONTREE,		// 在最短路径上
};

//用来表示于一个中继节点链接
class Link
{
public:
    Link *l_next;	// 单连接列表
    uns32 l_id;		// 邻居节点编号
    uns32 l_data;	// Link data
    byte l_ltype;	// Link type
    uns16 l_fwdcst;	// 转发花费

    inline Link();
    friend class rtrLSA;
    friend class netLSA;
    friend class OSPF;
};

inline Link::Link():l_next(0)
{

}

//中继链接
class TLink : public Link
{
public:
    TNode *tl_nbr; //邻居节点
    uns16 tl_rvcst;//花费

    inline TLink();
    friend class rtrLSA;
    friend class netLSA;
    friend class OSPF;
};

inline TLink::TLink():tl_nbr(0),tl_rvcst(MAX_COST)
{

}
//中继节点的STUB链接
class SLink : public Link
{
    IPrte *sl_rte;//STUB路由表条目
public:
    friend class rtrLSA;
    friend class OSPF;
};

class rtrLSA : public TNode
{
    uns16 n_links;
    byte rtype;
public:
    rtrLSA(LShdr *, int blen);
    inline bool is_abr();
    inline bool is_asbr();
    inline bool has_VLs();
    virtual void reoriginate(int forced);
    virtual void parse(LShdr *hdr);
    virtual void unparse();
    virtual void build(LShdr *hdr);
    friend class OSPF;
    friend class RouterRtrTblEntry;
};

inline bool rtrLSA::is_abr()
{
    return((rtype & RTYPE_B) != 0);
}
inline bool rtrLSA::is_asbr()
{
    return((rtype & RTYPE_E) != 0);
}
inline bool rtrLSA::has_VLs()
{
    return((rtype & RTYPE_V) != 0);
}

class netLSA : public TNode
{
public:
    netLSA(LShdr *, int blen);
    virtual void reoriginate(int forced);
    virtual void parse(LShdr *hdr);
    virtual void unparse();
    virtual void build(LShdr *hdr);
    virtual void delete_actions();
    friend class OSPF;
};
#endif // LSA_H
