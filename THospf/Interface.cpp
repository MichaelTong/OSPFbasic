#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"

Interface::Interface(InAddr addr,int phy)
    : if_waitt(this),if_hellot(this),if_ackt(this)
{
    if_addr=addr;
    if_physic=phy;
    passive=0;
    igmp_enabled=false;

    db_xsum=0;
    next=0;
    if_dr=0;
    if_bdr=0;
    if_dr_n=0;
    if_state=IFS_DOWN;
    if_demand_time=0;

    if_Neighborcnt=0;
    if_Neighborlist=0;
    if_fullcnt=0;
    recv_update=false;
    flood=false;
}
/* 接口的析构函数。
 * 宣布一个接口关闭，并且释放与之相关的邻居
 */
Interface::~Interface()
{
    Interface **ipp;
    Interface *ip;
    Neighbor *np;
    NbrList nbr_list(this);
    //宣布接口关闭
    ifc_fsm(IFE_DOWN);
    //删除邻居
    while((np=nbr_list.getnext()))
        delete np;
    //从ospf结构中的接口表删除该接口
    for(ipp=&ospf->ifcs; (ip=*ipp)!=0; ipp=&ip->next)
    {
        if(ip==this)
        {
            *ipp=next;
            break;
        }

    }
    //关闭物理接口
    if(if_physic!=-1)
        ospf->phy_detach(if_physic,if_addr);

}

int Interface::type()
{
    return (if_type);
}
//开始发送hello
void Interface::start_hellos()
{
    send_hello();
    if_hellot.start(if_hellointerval*Timer::SECOND);
}
//重新发送hello
void Interface::restart_hellos()
{
    if_hellot.restart();
}
//停止发送hello
void Interface::stop_hellos()
{
    if_hellot.stop();
}
//是否要选举DR
bool Interface::elects_dr()
{
    return false;
}
//对于DR类型接口，需要选举DR
bool DRIfc::elects_dr()
{
    return true;
}

//从该接口发送分组
void Interface::if_send(Pkt *pdesc,InAddr addr)
{
    InPkt *pkt;

    if(!pdesc->iphead)
        return;
    finish_pkt(pdesc,addr);
    pkt=pdesc->iphead;
    if(pkt->i_src!=0)
        sys->sendpkt(pkt,if_physic);
    ospf->freepkt(pdesc);
}


//生成校验码，目前是无验证模式
void Interface::gen_msg(Pkt *pdesc)
{
    SpfPkt *spfpkt;
    int length;

    spfpkt=pdesc->spfpkt;
    length=ntoh16(spfpkt->plen);

    spfpkt->chksum=0;
    spfpkt->autype=hton16(0);
    memset(spfpkt->un.aubytes,0,8);

    switch(if_autype)
    {
    case AUT_NONE:
        if(!pdesc->chksummed)
            spfpkt->chksum=~incksum((uns16 *)spfpkt,length);
        else
            spfpkt->chksum=~incksum((uns16 *)spfpkt,sizeof(SpfPkt),pdesc->body_chksum);
        memcpy(spfpkt->un.aubytes, if_passwd, 8);
    default:
        break;
    }
}



//设置邻居ID或IP地址
void Interface::set_IdAddr(Neighbor *np,rtid_t id,InAddr)
{
    np->n_id=id;
}

//设置端口
//opt表示是删除还是添加
void OSPF::ConfigInterface(struct InterfaceConfig *m,int opt)
{
    Interface *ip;
    bool nbr_change=false;
    bool new_lsa=false;
    bool new_net_lsa=false;

    ip = find_ifc(m->address, m->phyint);

    if (opt == DELETE_ITEM)
    {
        if (ip)
            delete ip;
        return;
    }

    if (ip && ip->type() != m->IfType)
    {
        delete ip;
        ip = 0;
    }

    if (!ip)
    {
        switch (m->IfType)
        {
        case IFT_BROADCAST:
            ip = new BroadcastIfc(m->address, m->phyint);
            break;
        default:
            return;
        }
        ip->if_mask = m->mask;
        ip->if_net = ip->if_addr & ip->if_mask;
        //加入到ospf接口表
        ip->next = ospf->ifcs;
        ospf->ifcs = ip;
    }

    //更改hello发送时间
    if (m->hello_int != ip->if_hellointerval || ip->if_pollinterval != m->poll_int)
    {
        ip->if_hellointerval = m->hello_int;
        ip->if_pollinterval = m->poll_int;
        ip->restart_hellos();
    }
    // 设置参数
    ip->mtu = m->mtu;		// 接口分组尺寸
    ip->if_xdelay = m->xmt_dly;	// 重发延迟
    ip->if_rxmt = m->rxmt_int;	// 重发间隔
    ip->if_deadinterval = m->dead_int;	// 邻居失效时间
    ip->if_autype = m->auth_type; // 验证类型

    // 设置端口掩码
    if (ip->if_mask != m->mask)
    {
        ip->if_mask = m->mask;
        ip->if_net = ip->if_addr & ip->if_mask;
        new_lsa = true;
        new_net_lsa = true;
    }
    // 设置端口序号
    if (ip->if_IfIndex != m->IfIndex)
    {
        ip->if_IfIndex = m->IfIndex;
        new_lsa = true;
    }
    // DR优先级
    if (ip->if_drpri != m->dr_pri)
    {
        ip->if_drpri = m->dr_pri;
        ip->ifa_allNeighbors_event(NBE_EVAL);
        nbr_change = true;
    }
    // 端口花费
    if (m->if_cost == 0)
        m->if_cost = 1;
    if (ip->if_cost != m->if_cost)
    {
        ip->if_cost = m->if_cost;
        new_lsa = true;
    }

    //开启端口
    if (sys->phy_operational(ip->if_physic))
        ip->ifc_fsm(IFE_UP);

    sys->add_interface_direct(ip->net(),ip->mask(),ip->if_addr,false);

    if (nbr_change)
        ip->ifc_fsm(IFE_NCHG);
    if (new_lsa)
        ospf->rl_orig(false);//生成新的router-lsa
    if (new_net_lsa)
        ip->nl_orig(false);//生成新的network-lsa
    ip->updated = true;
}

//重启端口
void Interface::restart()
{
    ifc_fsm(IFE_DOWN);
    if(sys->phy_operational(if_physic))
        ifc_fsm(IFE_UP);
}

//对端口所有邻居发送事件驱动
void Interface::ifa_allNeighbors_event(int event)
{
    NbrList iter(this);
    Neighbor *nbr;
    while((nbr=iter.getnext()))
        nbr->nbr_fsm(event);
}

//验证分组校验码
int Interface::verify(Pkt *pdesc, Neighbor *np)
{
    SpfPkt *spfpkt;
    byte *spfend;

    spfpkt = pdesc->spfpkt;
    if (ntoh16(spfpkt->autype) != if_autype)
        return(false);
    spfend = ((byte *) spfpkt) + ntoh16(spfpkt->plen);

    switch (if_autype)
    {
    case AUT_NONE:	// 无验证模式
        if (incksum((uns16 *) spfpkt, ntoh16(spfpkt->plen)) != 0)
            return(false);
        break;
    default:
        return(false);
    }

    pdesc->end = spfend;
    return(true);
}

//根据邻居地址寻找接口邻居
Neighbor *Interface::find_Neighbor(InAddr addr, rtid_t)

{
    NbrList iter(this);
    Neighbor *np;

    while ((np = iter.getnext()))
    {
        if (np->addr() == addr)
            break;
    }

    return(np);
}


//根据邻居地址找到接口
Interface *OSPF::find_nbr_ifc(InAddr nbr_addr)

{
    IfcList iter(this);
    Interface *ip;

    while ((ip = iter.get_next()))
    {
        if ((nbr_addr & ip->mask()) == ip->net())
            return(ip);
    }

    return(0);
}
