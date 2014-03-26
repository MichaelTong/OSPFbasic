#ifndef OSPF_H
#define OSPF_H


extern Queue timerq;

//每隔1s触发一次，完成链路状态数据库老化及其他数据库内
//部管理工作，包括在必要的时候运行完整的路由选择计算。
class DBageTimer : public ITimer
{
public:
    virtual void action();
};
class OSPF
{
    const rtid_t myid; //路由器ID
    uns16 max_retrans_window; //最大传输窗口
    byte max_dds; //最多同时发送DD
    InAddr myaddr;
    //动态数据
    Interface *ifcs;//接口列表
    Table phyints; //物理接口列表
	Table krtdeletes; //需要删除的内核路由
    Table ospf_member; //OSPF组播成员列表，现在仅有224.0.0.5和224.0.0.6的组播有用
    uns16 ospf_mtu;//所有接口最大的MTU
    bool delete_neighbors;

    //以下两个构成一个等待建立邻接关系邻居链表
    Neighbor *g_adj_head;//头
    Neighbor *g_adj_tail;//尾
	uns32 PPAdjLimit;
	
    //DD处理
	int nbrs;
	int	n_dbx_nbrs;
    int local_inits;
    int remote_inits;

    //LSA处理
    byte *build_area;//组建LSA缓冲区（网络格式）
    uns16 build_size;//LSA缓冲区大小
    byte *orig_buff;//LSA缓冲区
    uns16 orig_size;//缓冲区大小
    bool orig_buff_in_use;//缓冲区是否被使用
    uns32 n_orig_allocs;

	//LSA数据库相关
    uns32 db_xsum;//LS数据库校验和
    Table rtrLSAs;//router-LSAs
    Table netLSAs;//network-LSAs
    LsaList replied_list;//最近发送的LSA，响应收到的旧的LSA
    LsaList MaxAge_list;//达到最大老化的LSA，需要被刷新
    uns32 total_lsas;//数据库中LSA总数
    LsaList dbcheck_list;//验证通过的LSA
    LsaList pending_refresh;//等待被刷新的LSA
    bool OverflowState;



    //泛洪
    Pkt	a_update; //当前进行泛洪的update
    Pkt	a_demand_upd;//当前在需要泛洪的接口进行泛洪的update



    //路由处理
    bool need_remnants; //是否需要内核上传路由信息
    int full_sched:1;//是否要进行路由规划
    uns32 n_dijkstras;//进行Dijkstras算法的次数
    Table abr_tbl;//abr列表





    int run_fsm(FSMmachine *table, int &i_state,int event);    //有输入的状态转移表，状态和时间，来得到产生的动作和改变后的状态
    Interface *find_ifc(uns32 addr, int phyint = -1);    //根据地址和物理端口号来查找端口
    Interface *next_ifc(uns32 addr, int phyint);    //找到比所给出地址和物理端口号大的最小的一个端口
    Interface *find_ifc(Pkt *pdesc);    //根据数据包查找端口
    Interface *find_nbr_ifc(InAddr nbr_addr);    //找到所给邻居地址的端口
    int getpkt(Pkt *pkt,int type,uns16 size);    //生成基本的IP包
    void freepkt(Pkt *pkt);    //释放包
    void delete_down_neighbors();    //遍历邻居表，删除断开的邻居
    void multi_join(int phyint,InAddr group);    //加入多播组
    void multi_leave(int phyint,InAddr group);    //离开多播组
    void phy_attach(int phyint);    //添加启用的物理端口到记录中
    void phy_detach(int phyint, InAddr addr);    //删除端口记录



    DBageTimer dbtim;
    Table *FindLSdb(Interface *,byte lstype);
    LSA	*FindLSA(Interface *, byte lstype, lsid_t lsid, rtid_t rtid);
    LSA	*myLSA(Interface *, byte lstype, lsid_t lsid);
    LSA	*AddLSA(Interface *, LSA *current, LShdr *hdr, bool changed);
    void DeleteLSA(LSA *lsap);
    LSA *NextLSA(byte, lsid_t, rtid_t);
    LSA *NextLSA(InAddr, int, byte, lsid_t, rtid_t);
    void update_lsdb_xsum(LSA *, bool add);
    void ParseLSA(LSA *lsap, LShdr *hdr);
    void UnParseLSA(LSA *lsap);
    LShdr *BuildLSA(LSA *lsap, LShdr *hdr=0);
    void send_updates();
    bool maxage_free(byte lstype);
    LShdr *orig_buffer(int ls_len);
    void free_orig_buffer(LShdr *);    //释放LS缓冲区
    void add_to_ifmap(Interface *ip);    //加入需要生成update的端口表
    void delete_from_ifmap(Interface *ip);    //从端口表删除引用

    inline RouterRtrTblEntry *find_abr(uns32 rtrid);
    RouterRtrTblEntry *add_abr(uns32 rtrid);

    void dbage(); //数据库老化
    void deferred_lsas();   //延迟创建LSA
    void checkages();   //检查一定年龄LSA的校验和
    void refresh_lsas();    //刷新LSA
    void maxage_lsas(); //年龄恰好达到MaxAge的LSA重新泛洪，从老化列表取出来，放到单独的LSA表，在适当的时候会被回收
    void free_maxage_lsas();//释放达到最大年龄的LSA
    void schedule_refresh(LSA *);//规划要刷新的LSA
    void do_refreshes();//刷新LSA

    int	originated(Neighbor *, LShdr *hdr, LSA *database_copy);     //判断LSA是否是自己生成的
    seq_t ospf_get_seqno(byte lstype, LSA *lsap, int forced);   //得到一个序列号
    void age_prematurely(LSA *);    //让一个LSA过期
    LSA	*lsa_reorig(Interface *,LSA *olsap, LShdr *hdr, int forced);    //重新生成LSA

    void add_to_update(LShdr *hdr, bool demand_upd);    //将LSA加入到update中
    void build_update(Pkt *pkt, LShdr *hdr, uns16 mtu, bool demand);    //组建update

    void rtsched(LSA *newlsa, RtrTblEntry *old_rte);    //看路由是否需要重新计算
    void full_calculation();    //进行路由计算
    void dijk_init(Queue &cand);    //初始化dijkstra
    void add_cand_node(Interface *ip, TNode *node, Queue &cand);    //加入候选
    void dijkstra();
    void rt_scan(); //扫描路由表，删除已经失效的路由
    void krt_sync();//与内核同步

public:
    class rtrLSA *mylsa;//本路由产生的LSA
    int n_active_if;
    int n_routers;//可到达的路由
    class Interface **ifmap;//router-LSA与接口的映射关系
    int ifmap_valid;//映射关系有效？
    int size_ifmap;//映射关系表大小
    int n_ifmap;//当前映射关系表条目数

    void rl_orig(int forced);
    OSPF(uns32 rtid_t,SPFtime grace);
    ~OSPF();
    inline rtid_t my_id();
    inline InAddr my_addr();
    InAddr if_addr(int phyint);
    void ConfigInterface(struct InterfaceConfig *m,int opt);    //设置端口
    void phy_up(int phyint);    //开启物理端口
    void phy_down(int phyint);  //关闭物理端口
    void resolvePkt(int phyint, InPkt *pkt, int plen);  //解析数据包
	void krt_delete_notification(InAddr net, InMask mask);
	void remnant_notification(InAddr net, InMask mask);

    void tick();
    int timeout();

    friend class IfcList;
    friend class Timer;
    friend class ITimer;
    friend class Neighbor;
    friend class Interface;
    friend class LSA;
    friend class TNode;
    friend class PPIfc;
    friend class P2MPIfc;
    friend class DRIfc;
    friend class RtrTblEntry;
    friend class rtrLSA;
    friend class netLSA;
    friend class EPath;
    friend class IPrte;
    friend class FWDtbl;
    friend class FWDrte;
    friend void lsa_flush(class LSA *);
    friend class DBageTimer;
    friend Neighbor *GetNextAdj();
};

extern OSPF *ospf;
inline RouterRtrTblEntry *OSPF::find_abr(uns32 rtrid)
{
    return ((RouterRtrTblEntry *) abr_tbl.find(rtrid));
}
inline rtid_t OSPF::my_id()
{
    return (myid);
}

inline InAddr OSPF::my_addr()
{
    return(myaddr);
}

enum
{
    ADD_ITEM,
    DELETE_ITEM,
};
void lsa_flush(LSA *lsap);

extern IPRtrTbl *inrttbl;
extern FWDtbl *fa_tbl;
extern IPrte *default_route;

#endif // OSPF_H
