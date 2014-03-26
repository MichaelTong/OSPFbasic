#include "include.h"
#include "system.h"
#include "InterfaceFSM.h"



OSPF *ospf;
Queue timerq;		// 全局计时器列表
SysCalls *sys;
SPFtime sys_etime;

IPRtrTbl *inrttbl;	// IP路由表
FWDtbl *fa_tbl;        // 转发地址表
IPrte *default_route; // 默认路由
PatriciaTree EPath::nhdb; //下一跳数据库

OSPF::OSPF(uns32 rtid,SPFtime grace):myid(rtid)
{

    ifcs=0;

    max_retrans_window=8;
    max_dds=2;
    PPAdjLimit=0;

    myaddr=0;

    g_adj_head=0;
    g_adj_tail=0;

    nbrs=0;
    local_inits=0;
    remote_inits=0;
    full_sched = false;
    n_dijkstras = 0;
    need_remnants = true;
    orig_buff=0;
    orig_size=0;
    orig_buff_in_use=false;
    n_orig_allocs=0;
    total_lsas=0;

    delete_neighbors = false;

    n_dbx_nbrs=0;
    db_xsum = 0;
    //开始数据库老化
    dbtim.start(1*Timer::SECOND);

    inrttbl = new IPRtrTbl;
    //默认路由
    default_route = inrttbl->add(0, 0);
    fa_tbl = new FWDtbl;

    phyints.clear();
    replied_list.clear();
    MaxAge_list.clear();
    dbcheck_list.clear();
    pending_refresh.clear();
    krtdeletes.clear();
}

OSPF::~OSPF()
{
    dbtim.stop();

    inrttbl->root.clear();
    fa_tbl->root.clear();
    default_route=0;
    EPath::nhdb.clear();

    delete []orig_buff;
    phyints.clear();
    replied_list.clear();
    MaxAge_list.clear();
    dbcheck_list.clear();
    freepkt(&a_update);
    freepkt(&a_demand_upd);
    krtdeletes.clear();

    for(int i=0;i<MaxAge+1;i++)
        LSA::AgeBins[i]=0;
    LSA::Bin0=0;
    for(int i=0;i<MaxAgeDiff;i++)
        LSA::RefreshBins[i]=0;
    LSA::RefreshBin0=0;
}

//每次循环会调用一次，来看是否到达计时器的时间，如果到达则激发计时器对应的动作
void OSPF::tick()
{
    Timer *tqelt;
    uns32 sec;
    uns32 msec;

    sec = sys_etime.sec;
    msec = sys_etime.ms;
    while ((tqelt = (Timer *) timerq.gethead()))
    {
        if (tqelt->cost0 > sec)
            break;
        if (tqelt->cost0 == sec && tqelt->cost1 > msec)
            break;
        tqelt = (Timer *) timerq.rmhead();
        tqelt->fire();
    }
}

int OSPF::timeout()
{
    Timer *tqelt;
    SPFtime wakeup_time;

    if (!(tqelt = (Timer *) timerq.gethead()))
        return(-1);

    wakeup_time.sec = tqelt->cost0;
    wakeup_time.ms = tqelt->cost1;
    if (time_less(wakeup_time, sys_etime))
        return(0);
    else
        return(time_diff(wakeup_time, sys_etime));
}

void OSPF::phy_up(int phyint)
{
    IfcList iter(this);
    Interface *ip;
    while((ip=iter.get_next()))
    {
        if(ip->if_physic==phyint)
        {
            ip->ifc_fsm(IFE_UP);
            full_sched=true;
        }
    }
}

void OSPF::phy_down(int phyint)
{
    IfcList iter(this);
    Interface *ip;
    while((ip=iter.get_next()))
    {
        if(ip->if_physic==phyint)
            ip->ifc_fsm(IFE_DOWN);
    }
}

//根据地址和物理端口号来查找端口
Interface  *OSPF::find_ifc(uns32 xaddr, int phy)
{
    IfcList iter(this);
    Interface *ip;

    while ((ip = iter.get_next()))
    {
        if (phy != -1 && phy != ip->if_physic)
            continue;
        if (ip->if_addr == xaddr)
            break;
    }

    return(ip);
}

//根据数据包查找端口
Interface *OSPF::find_ifc(Pkt *pdesc)

{
    IfcList iter(this);
    Interface *ip;
    InAddr ipsrc;
    InAddr ipdst;
    //得到源地址和目的地址
    ipsrc = ntoh32(pdesc->iphead->i_src);
    ipdst = ntoh32(pdesc->iphead->i_dest);

    while ((ip = iter.get_next()))
    {
    //对比参数
        if (pdesc->phyint != -1 && ip->if_physic != pdesc->phyint)
            continue;

        else if ((ipsrc & ip->mask()) != ip->net())
            continue;
        else if (ipdst == AllDRouters || ipdst == AllSPFRouters)
            return(ip);
        else if (ipdst == ip->if_addr)
            return(ip);
    }

    return(0);
}

//解析数据包
void OSPF::resolvePkt(int phyint, InPkt *pkt, int plen)
{
    SpfPkt *spfpkt;
    Interface *ip=0;
    Neighbor *np=0;
    Pkt pdesc(phyint, pkt);

    spfpkt = pdesc.spfpkt;

    if (ntoh32(spfpkt->srcid) == myid)
        return;

    if (plen < ntoh16(pkt->i_len))
        return;// 长度小
    else if (pdesc.bsize < (int) sizeof(SpfPkt))
        return;// 长度小
    else if (pdesc.bsize < ntoh16(spfpkt->plen))
        return;// 长度小;
    else if (spfpkt->vers != OSPFv2)
        return;// 版本不对
    else if (!(ip = find_ifc(&pdesc)))
        return;// 没有这个端口
    else
    {
        np = ip->find_Neighbor(ntoh32(pkt->i_src), ntoh32(spfpkt->srcid));
        if (spfpkt->type != SPT_HELLO && !np)
            return;// 没有邻居
        else if (!ip->verify(&pdesc, np))
            return;// 验证错误
        else if (ntoh32(pkt->i_dest) == AllDRouters &&
                 ip->state() <= IFS_OTHER)
            return;// 不是DR
    }


    switch (spfpkt->type)
    {
    case SPT_HELLO:	// Hello
        ip->recv_hello(&pdesc);
        break;
    case SPT_DD:		// DD
        np->recv_dd(&pdesc);
        break;
    case SPT_LSREQ:	// LSR
        np->recv_req(&pdesc);
        break;
    case SPT_UPD:		// LSA
        np->recv_update(&pdesc);
        break;
    case SPT_LSACK:	// ACK
        np->recv_ack(&pdesc);
        break;
    default:		// 错误的类型
        break;
    }
}

//找到比所给出地址和物理端口号大的最小的一个端口
Interface  *OSPF::next_ifc(uns32 xaddr, int phy)
{
    IfcList iter(this);
    Interface *ip;
    Interface *best;

    best = 0;
    while ((ip = iter.get_next()))
    {
        if (phy > ip->if_physic)
            continue;
        if (phy == ip->if_physic && xaddr >= ip->if_addr)
            continue;
        if (best)
        {
            if (best->if_physic < ip->if_physic)
                continue;
            if (best->if_physic == ip->if_physic &&
                    best->if_addr <  ip->if_addr)
                continue;
        }
        best = ip;
    }

    return(best);
}
//链路状态发生改变时，需要删除内核的一些路由
void OSPF::krt_delete_notification(InAddr net, InMask mask)

{
    IPrte *rte;

    if ((rte = inrttbl->find(net, mask)) && rte->valid())
    {
        KrtSync *item;
        item = new KrtSync(net, mask);
        krtdeletes.add(item);
    }
}
//删除残留的路由
void OSPF::remnant_notification(InAddr net, InMask mask)

{
    IPrte *rte;

    if (!(rte = inrttbl->find(net, mask)) || !rte->valid())
    {
        sys->rtdel(net, mask, 0);
    }
}
//添加边界路由
RouterRtrTblEntry *OSPF::add_abr(uns32 rtrid)

{
    RouterRtrTblEntry *rte;

    if ((rte = (RouterRtrTblEntry *) abr_tbl.find(rtrid)))
        return(rte);

    rte = new RouterRtrTblEntry(rtrid);
    abr_tbl.add(rte);
    return(rte);
}
