#ifndef NEIGHBOR_H
#define NEIGHBOR_H


class Neighbor;

class InactTimer : public Timer
{
    Neighbor *np;
public:
    inline InactTimer(Neighbor *);
    virtual void action();
};
//计时器，用于邻居活跃计时，触发时
//表示在deadtime时间内没有收到hello
//应该断开邻接关系
inline InactTimer::InactTimer(Neighbor *n) :np(n)
{

}
//非广播方式向某一邻居发送Hello
class HelloTimer : public ITimer
{
    Neighbor *np;
public:
    inline HelloTimer(Neighbor *);
    virtual void action();
};

inline HelloTimer::HelloTimer(Neighbor *n) :np(n)
{

}
//用来保持以slave模式送出去的最后的数据库描
//述分组，直到主路由器收到了该分组为止。
class HoldTimer : public Timer
{
    Neighbor *np;
public:
    inline HoldTimer(Neighbor *);
    virtual void action();
};

inline HoldTimer::HoldTimer(Neighbor *n) : np(n)
{
}

//该计时器触发导致主路由器重新发送其刚刚发送过的数据库
//描述分组
class DDTimer : public ITimer
{
    Neighbor *np;
public:
    inline DDTimer(Neighbor *);
    virtual void action();
};

inline DDTimer::DDTimer(Neighbor *n) : np(n)
{
}
//该计时器触发将导致一个路由器向相邻路由器重新发送链路
//状态请求分组。
class LSRTimer : public ITimer
{
    Neighbor *np;
public:
    inline LSRTimer(Neighbor *);
    virtual void action();
};

inline LSRTimer::LSRTimer(Neighbor *n) : np(n)
{
}

//该计时器触发导致LSA重新传输到相邻路由器,从而实现
//可靠性泛洪算法。
class LSATimer : public Timer
{
    Neighbor *np;
public:
    inline LSATimer(Neighbor *);
    virtual void action();
};

inline LSATimer::LSATimer(Neighbor *n) : np(n)
{
}
//该计时器触发指示与相邻路由器的数据库交换处理被阻
//断，并且要求隔一段时间后重新启动。
class ProgressTimer : public Timer
{
    Neighbor *np;
public:
    inline ProgressTimer(Neighbor *);
    virtual void action();
};

inline ProgressTimer::ProgressTimer(Neighbor *nbr) : np(nbr)
{
}


class Neighbor
{
    InAddr n_addr; //邻居地址
    rtid_t n_id; //邻居路由器ID
    int n_time;

    int n_state;//邻居状态
    uns32 md5_seqno;
    byte n_opts;//邻居发送分组的option段
    byte n_imms;//邻居发送DD的imms段
    uns32 n_ddseq;//DD序列号
    bool database_sent;//需要发送整个数据库？
    int n_adj_pend:1,//等待建立邻接关系？
        remote_init:1;//由该邻居发起的DD交换？
    bool request_suppression;
    bool hellos_suppressed;
    Neighbor *next_pend;//下一个等待建立邻接关系的邻居

    //LSA重传列表
    class LsaList pend_retrans;//最近需要重传的LSA
    class LsaList retrans;//已经泛洪的LSA，但还没有到重传时间
    class LsaList failed_retrans;//发送了LSA还没有收到确认
    uns32 retrans_cnt;//所有重传的LSA
    uns16 retrans_window;//重传窗口，一次性发送LSA最大数目

    class LsaList ddlist;//DD表
    class LsaList reqlist;//LSA Request表
    int	req_goal;//还需发送的请求数
    Pkt n_update;//需要发送的update
    Pkt n_imack;//立即回复的ACK
    Pkt n_ddpkt;//当前发送的DD
    InactTimer inactt;//判断邻居死掉的计时器
    HelloTimer hellot;//发送hello的计时器
    HoldTimer holdt;//用来保持slave模式发送出去的最后的DD，知道master收到了该分组为止
    DDTimer ddt;//DD重传计时器
    LSRTimer lsrt;//LSReq重传计时器
    LSATimer lsat;//LSUpdate重传计时器
    ProgressTimer progt;//触发时表示相邻路由的数据库交换被阻断，需要重启

protected:
    Neighbor *next; //邻居列表中的下一个
    Interface *n_ifc;//与之相关的接口

public:
    byte prior;//优先级
    InAddr n_dr;//DR地址
    InAddr n_bdr;//BDR地址

    Neighbor(Interface *,rtid_t id, InAddr addr);
    virtual ~Neighbor();

    inline Interface *ifc();    //得到对应的端口
    inline int state();         //得到当前状态
    inline int declared_dr();   //宣称是DR？
    inline int declared_bdr();  //宣称是BDR？
    inline int is_dr();         //是DR？
    inline int is_bdr();        //是BDR？
    inline InAddr addr();       //得到地址
    inline rtid_t id();         //得到路由ID
    inline int priority();      //得到优先级
    inline int supports(int option);
    inline void build_imack(LShdr *hdr);    //组建立即的ACK

    void add_to_retranslist(LSA *lsap);
    bool remove_from_retranslist(LSA *lsap);
    bool remove_from_pending_retrans(LSA *lsap);
    LSA	*get_next_retrans(LsaList * &list, uns32 &nexttime);
    void clear_retrans_list();
    bool changes_pending();

    void recv_dd(struct Pkt *pdesc);
    void process_dd(LShdr *hdr,byte *end);
    void recv_req(struct Pkt *pdesc);
    void recv_update(struct Pkt *pdesc);
    void recv_ack(struct Pkt *pdesc);
    void negotiate(byte opts);
    void exit_dbxchg();

    void send_hello();
    void send_dd();
    void retrans_dd();
    void send_req();
    void retrans_updates();

    void dd_free();
    void nbr_fsm(int event);
    void nba_eval1();
    void nba_eval2();
    void nba_snapshot();
    void nba_exchangedone();
    void nba_reeval();
    void nba_clr_lists();
    void nba_delete();
    int add_to_update(LShdr *hdr);


    int ospf_rmreq(LShdr *hdr, int *rq_cmp);
    void start_adjacency(); //开始建立邻接关系
    void AddPendAdj();      //加入到等待建立邻接表
    void DelPendAdj();      //从等待建立邻接表删除
    bool adv_as_full();     //已经是FULL状态？

    friend class Interface;
    friend class PPIfc;
    friend class NBMAIfc;
    friend class P2MPIfc;
    friend class OSPF;
    friend class NbrList;
    friend class InactTimer;
    friend class HelloTimer;
    friend class HoldTimer;
    friend class DDTimer;
    friend class LSRTimer;
    friend class LSATimer;
    friend class ProgressTimer;
    friend Neighbor *GetNextAdj();
};
inline Interface *Neighbor::ifc()
{
    return (n_ifc);
}
inline int Neighbor::state()
{
    return (n_state);
}
inline int Neighbor::declared_dr()
{
    return (n_dr == n_addr);
}
inline int Neighbor::declared_bdr()
{
    return (n_bdr == n_addr);
}
inline int Neighbor::is_dr()
{
    return (n_ifc->dr()==n_addr);
}

inline int Neighbor::is_bdr()
{
    return (n_ifc->bdr()==n_addr);
}

inline InAddr Neighbor::addr()
{
    return n_addr;
}

inline rtid_t Neighbor::id()
{
    return n_id;
}

inline int Neighbor::priority()
{
    return (int)prior;
}

inline int Neighbor::supports(int option)
{
    return ((n_opts&option)!=0);
}

inline void Neighbor::build_imack(LShdr *hdr)
{
    n_ifc->if_build_ack(hdr, &n_imack, this);
}
#endif // NEIGHBOR_H
