#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"

//接口发送Hello计时器触发
void HelloTimer_Ifc::action()
{
    ifc->send_hello();
}
//向某个邻居发送Hello计时器触发
void HelloTimer::action()
{
    np->send_hello();
}
//不活动计时器出发
void InactTimer::action()
{
    np->nbr_fsm(NBE_INACTIVE);
}

//广播发送Hello
void Interface::send_hello(bool empty)
{
    NbrList lis(this);
    Neighbor *nbr;
    uns16 size;
    Pkt pkt;
    HelloPkt *hellopkt;
    rtid_t *hellonbr;

    if(passive)//接口如果是被动，则不发送
        return;

    size= sizeof(HelloPkt);
    //对于已经发现的邻居，都要加入到Hello中
    while((nbr=lis.getnext()))
    {
        if(nbr->state()>=NBS_INIT)
            size+=sizeof(rtid_t);
    }
    if(build_hello(&pkt,size)==0)
        return;

    lis.reset();
    hellopkt=(HelloPkt *)(pkt.spfpkt);
    hellonbr=(rtid_t *)(hellopkt+1);
    //向Hello加入发现的邻居的RouterID
    while(!empty&&(nbr=lis.getnext()))
    {
        if(nbr->state()>=NBS_INIT)
        {
            *hellonbr++=hton32(nbr->id());
            pkt.dptr+=sizeof(rtid_t);
        }
    }
    //广播发送
    if_send(&pkt,AllSPFRouters);
}


int Interface::build_hello(Pkt *pkt,uns16 size)
{
    HelloPkt *hellopkt;

    size+=sizeof(InPkt);
    if(ospf->getpkt(pkt,SPT_HELLO,size)==0)
        return (0);

    hellopkt=(HelloPkt*)(pkt->spfpkt);
    hellopkt->Hello_mask=hton32(if_mask);
    hellopkt->Hello_hint=hton16(if_hellointerval);
    hellopkt->Hello_opts=0;
    hellopkt->Hello_opts|=SPO_EXT;
    hellopkt->Hello_pri=if_drpri;
    hellopkt->Hello_dint=hton32(if_deadinterval);
    hellopkt->Hello_dr=((type()!=IFT_PP)?hton32(if_dr):hton32(mtu));
    hellopkt->Hello_bdr=hton32(if_bdr);

    pkt->dptr=(byte*)(hellopkt+1);

    return size;
}

//非广播端口，只发送到一个相邻路由器
void Neighbor::send_hello()
{
    uns16 size;
    Pkt pkt;
    HelloPkt *hellopkt;
    rtid_t *hellonbr;

    size=sizeof(HelloPkt);
    if(n_state>=NBS_INIT)
        size+=sizeof(rtid_t);
    if(n_ifc->build_hello(&pkt,size)==0)
        return;

    hellopkt=(HelloPkt*)(pkt.spfpkt);
    hellonbr=(rtid_t *)(hellopkt+1);
    if(n_state>=NBS_INIT)
    {
        *hellonbr++=hton32(n_id);
        pkt.dptr+=sizeof(rtid_t);
    }
    n_ifc->Neighbor_send(&pkt,this);
}

void Interface::send_hello_response(Neighbor *)
{

}
void Interface::recv_hello(Pkt *pdesc)

{
    InPkt *inpkt;
    HelloPkt *hlopkt;
    rtid_t old_id;
    rtid_t nbr_id;
    InAddr nbr_addr;
    bool was_declaring_dr;
    bool was_declaring_bdr;
    byte old_pri;
    rtid_t *idp;
    bool nbr_change;
    bool backup_seen;
    bool first_hello;
    Neighbor *np;

    if (if_state == IFS_DOWN || passive)
        return;

    inpkt = pdesc->iphead;
    nbr_addr = ntoh32(inpkt->i_src);
    hlopkt = (HelloPkt *) pdesc->spfpkt;
    nbr_id = ntoh32(hlopkt->header.srcid);

    // 验证参数
    if (ntoh16(hlopkt->Hello_hint) != if_hellointerval)
        return;
    if (ntoh32(hlopkt->Hello_dint) != if_deadinterval)
        return;
    if (ntoh32(hlopkt->Hello_mask) != if_mask)
        return;

    // 找到对应的邻居，如果没有，则创建邻居
    if (!(np = find_Neighbor(nbr_addr, nbr_id)))
        np = new Neighbor(this, nbr_id, nbr_addr);

    old_id = np->n_id;
    set_IdAddr(np, nbr_id, nbr_addr);
    //保存邻居的参数
    was_declaring_dr = np->declared_dr();
    was_declaring_bdr = np->declared_bdr();
    old_pri = np->prior;
    np->n_dr = ntoh32(hlopkt->Hello_dr);
    np->n_bdr = ntoh32(hlopkt->Hello_bdr);
    np->prior = hlopkt->Hello_pri;

    // 驱动状态机
    first_hello = (np->n_state == NBS_DOWN);//从邻居收到的第一个Hello？
    np->nbr_fsm(NBE_HELLORCVD);    // 驱动状态机

    for (idp = (rtid_t *) (hlopkt+1); ; idp++)
    {
        if ((byte *) idp >= pdesc->end)//对方发过来的Hello还没有active Neighbor，自己不在其中
        {
            np->nbr_fsm(NBE_1WAYRCVD);//通过事件1WAY破坏邻接关系
            if (first_hello)//收到的第一个Hello，则驱动状态机
            {
                np->nbr_fsm(NBE_EVAL);
            }
            else
                send_hello_response(np);
            return;
        }
        else if (ntoh32(*idp) == ospf->my_id())//自己在Hello体内，则2WAY
            break;
    }
    np->nbr_fsm(NBE_2WAYRCVD);
    np->negotiate(hlopkt->Hello_opts);

    // 检查是否DR变化
    nbr_change = false;
    backup_seen = false;
    //邻居的参数发生了变化？
    if (old_pri != np->prior)
    {
        np->nbr_fsm(NBE_EVAL);
        nbr_change = true;
    }
    else if (old_id != np->n_id)
        nbr_change = true;
    else if (was_declaring_dr != np->declared_dr())
        nbr_change = true;
    else if (was_declaring_bdr != np->declared_bdr())
        nbr_change = true;
    if (if_state == IFS_WAIT)
    {
        if (np->declared_dr() && ntoh32(hlopkt->Hello_bdr) == 0)//缺少BDR
            backup_seen = true;
        if (np->declared_bdr())//存在BDR
            backup_seen = true;
    }

    // 响应hello
    send_hello_response(np);

    if (backup_seen)//驱动接口状态机，进行选举
        ifc_fsm(IFE_BSEEN);
    else if (nbr_change)
        ifc_fsm(IFE_NCHG);
    // 生成新的network lsa
    if (old_id != np->n_id && np->ifc()->state() == IFS_DR)
        np->ifc()->nl_orig(false);
}
