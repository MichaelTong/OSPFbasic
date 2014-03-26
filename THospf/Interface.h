#ifndef INTERFACE_H
#define INTERFACE_H

struct InterfaceConfig
{
    InAddr address;
    int phyint;

    InMask mask;
    uns16 mtu;
    uns16 IfIndex;

    int IfType;
    byte dr_pri;
    byte xmt_dly;
    byte rxmt_int;
    uns16 hello_int;
    uns16 if_cost;
    uns32 dead_int;
    uns32 poll_int;
    int auth_type;
    byte auth_key[8];
    int mc_fwd;
    int demand;
    int passive;
    int igmp;
};
//延迟接口上的指定路由器计算，直到发现后备指定路
//由器存在。
class WaitTimer : public Timer
{
    class Interface *ifc;
public:
    inline WaitTimer(class Interface *);
    virtual void action();
};

inline WaitTimer::WaitTimer(class Interface *i)
{
    ifc=i;
}
//定期从某个端口发出Hello的计时器
class HelloTimer_Ifc : public ITimer
{
    class Interface *ifc;
public:
    inline HelloTimer_Ifc(class Interface *);
    virtual void action();
};

inline HelloTimer_Ifc::HelloTimer_Ifc(class Interface  *i)
{
    ifc=i;
}
//延迟链路状态确认分组在接口上的组播，
//先会将要ACK的LSA加入到update中，
//直到到时间将其发出
class DAckTimer : public Timer
{
    class Interface *ifc;
public:
    inline DAckTimer(class Interface *);
    virtual void action();
};

inline DAckTimer::DAckTimer(class Interface *i)
{
    ifc=i;
}

enum
{
    IFT_BROADCAST = 1,
    IFT_PP,
    IFT_NBMA,
    IFT_P2MP,
};
class Interface
{
protected:
    InMask if_mask; //接口掩码
    uns16 mtu; //接口mtu
    int if_IfIndex; //接口序号
    int if_type;//接口类型，目前只支持广播接口
    int if_cost;//接口花费
    byte if_rxmt;//接口重传间隔时间
    byte if_xdelay;//传输延迟时间

    uns16 if_hellointerval;//Hello发送间隔
    uns32 if_deadinterval;//邻居Dead时间
    uns32 if_pollinterval;
    byte if_drpri; //优先级
    uns32 if_demand:1;
    autyp_t if_autype;//验证类型 目前只支持空
    byte if_passwd[8];//验证密码
    int passive; //被动接口，目前没有实现
    int if_mcfwd; //多播转发
    bool igmp_enabled; //是否开启组播
    bool updated;

    //根据设置和运行过程决定的
    InAddr if_net; //网段
    InAddr if_multi;//多播地址
    InAddr if_flood;//泛洪地址



    InAddr if_dr;//DR地址
    InAddr if_bdr;//BDR地址
    class Neighbor *if_dr_n;//作为DR的邻居
    int if_state;//接口状态
    Interface *next;//接口链表，下一个接口

    Pkt if_update;//需要发送的update
    Pkt if_dack;//延迟发送的ACK
    bool recv_update;//收到了Update，正在处理
    uns32 if_demand_time;
    bool flood;//是否要泛洪
    //一些维护功能的计时器
    WaitTimer if_waitt;//等待计时器
    HelloTimer_Ifc if_hellot;//Hello计时器
    DAckTimer if_ackt;//延迟发送ACK计时器

    uns32 db_xsum;	// Database checksum

    virtual void ifa_start() = 0;
    void ifa_Neighbor_start(int base_priority);
    void ifa_elect();
    void ifa_reset();
    void ifa_allNeighbors_event(int event);


public:
    InAddr if_addr;
    int if_physic;

    class Neighbor *if_Neighborlist;//邻居列表
    int if_Neighborcnt;//邻居数
    int if_fullcnt;//完全邻接的邻居数

    Interface(InAddr addr, int phy);
    virtual ~Interface();
    inline int state(); //得到状态
    inline InAddr net();    //得到网络段
    inline InAddr mask();   //得到掩码
    inline uns16 cost();    //得到花费

    inline InAddr dr(); //DR
    inline InAddr bdr();//BDR
    inline byte rxmt_interval();    //重传间隔
    inline void if_build_dack(LShdr *hdr);  //组建延迟ACK
    inline void if_send_dack(); //发送延迟ACK
    inline void if_send_update();   //发送update
    inline int unnumbered();

    void restart();
    void gen_msg( Pkt *pdesc);
    int verify(Pkt *pdesc, Neighbor *np);      //验证包
    void recv_hello( Pkt *pdesc);
    void send_hello(bool empty=false);
    int build_hello( Pkt *,uns16 size);
    int add_to_update(LShdr *hdr);
    void if_build_ack(LShdr *hdr, Pkt *pkt=0, class Neighbor *np=0);


    void AddTypesToList(byte lstype, class LsaList *lp);
    void delete_lsdb();


    void finish_pkt(Pkt *pdesc,InAddr addr);
    int type();
    void ifc_fsm(int event);

    virtual void if_send(Pkt *, InAddr);    //从端口发出数据包
    virtual void Neighbor_send( Pkt *, Neighbor *); //向某邻居发数据包
    virtual Neighbor *find_Neighbor(InAddr, rtid_t);    //找到某邻居
    virtual void set_IdAddr(Neighbor *, rtid_t, InAddr);    //设置邻居ID或地址
    virtual RtrLink *rl_insert(RTRhdr *, RtrLink *) = 0;    //加入router lsa
    virtual int rl_size();
    void nl_orig(int forced);   //生成network lsa
    LShdr *nl_raw_orig();
    virtual int adjacency_wanted(class Neighbor *np);
    virtual void send_hello_response(Neighbor *np);
    virtual void start_hellos();
    virtual void restart_hellos();
    virtual void stop_hellos();
    virtual bool elects_dr();
    virtual EPath *add_parallel_links(EPath *, TNode *);

    friend class IfcList;
    friend class NbrList;
    friend class WaitTimer;
    friend class HelloTimer_Ifc;
    friend class InactTimer;
    friend class HelloTimer;
    friend class HoldTimer;
    friend class DDTimer;
    friend class LSRTimer;
    friend class LSATimer;
    friend class OSPF;
    friend class Neighbor;
    friend class LSA;

};

inline int Interface::state()
{
    return if_state;
}

inline InAddr Interface::net()
{
    return if_net;
}

inline InAddr Interface::mask()
{
    return if_mask;
}
inline uns16 Interface::cost()
{
    return(if_cost);
}
inline InAddr Interface::dr()
{
    return if_dr;
}

inline InAddr Interface::bdr()
{
    return if_bdr;
}
inline byte Interface::rxmt_interval()
{
    return(if_rxmt);
}

inline  void Interface::if_build_dack(LShdr *hdr)
{
    if_build_ack(hdr);
    if (!if_ackt.is_running())
        if_ackt.start(1*Timer::SECOND);
}
inline  void Interface::if_send_dack()
{
    if_send(&if_dack, if_flood);
}
inline void Interface::if_send_update()
{
    if (if_update.iphead)
        if_send(&if_update, if_flood);
}
inline int Interface::unnumbered()
{
    return (if_addr == 0);
}



class DRIfc : public Interface
{
public:
    inline DRIfc(InAddr addr, int phy);
    virtual int adjacency_wanted(class Neighbor *np);
    virtual RtrLink *rl_insert(RTRhdr *, RtrLink *);
    virtual void ifa_start();
    virtual bool elects_dr();
};

inline DRIfc::DRIfc(InAddr addr, int phy) : Interface(addr, phy)
{
}

class BroadcastIfc:public DRIfc
{
public:
    inline BroadcastIfc(InAddr addr,int phy);
};

inline BroadcastIfc::BroadcastIfc(InAddr addr, int phy): DRIfc(addr, phy)
{
    if_type=IFT_BROADCAST;
}

#endif // INTERFACE_H
