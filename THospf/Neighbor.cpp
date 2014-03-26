#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"

Neighbor::Neighbor(Interface *ip, rtid_t _id,InAddr _addr)
    : inactt(this),hellot(this),holdt(this),
      ddt(this),lsrt(this),lsat(this),
      progt(this)
{
    n_ifc=ip;
    n_id=_id;
    n_addr=_addr;

    n_state=NBS_DOWN;
    md5_seqno=0;
    n_adj_pend=false;
    remote_init=false;
    next_pend=0;
    retrans_cnt=0;
    retrans_window=1;
    req_goal=0;
    n_dr=0;
    n_bdr=0;
    database_sent=false;
    request_suppression=false;
    hellos_suppressed=false;

    next=ip->if_Neighborlist;
    ip->if_Neighborlist=this;
    ip->if_Neighborcnt++;
}

Neighbor::~Neighbor()
{
    nbr_fsm(NBE_KILLNBR);
}

//向邻居发送分组
void Interface::Neighbor_send(Pkt *pdesc,Neighbor *np)
{
    InPkt *pkt;
    if(!pdesc->iphead)
        return;
    finish_pkt(pdesc,np->addr());
    pkt=pdesc->iphead;

    if(pkt->i_src!=0)
    {
        sys->sendpkt(pkt,if_physic,np->addr());
    }
    else
        printf("Neighbor send error no src\n");
    ospf->freepkt(pdesc);
}

//与邻居协商，需要选举dr则直接返回，否则如果状态高于双向，则开启不活动计时器
void Neighbor::negotiate(byte opts)

{
    if (n_ifc->elects_dr())
        return;
    else if (n_state >= NBS_2WAY)
        inactt.start(n_ifc->if_deadinterval*Timer::SECOND);
}

/* 遍历邻居表，删除断开的邻居
 */

void OSPF::delete_down_neighbors()

{
    IfcList iiter(ospf);
    Interface *ip;
    Neighbor *np;
    Neighbor **prev;

    if (!delete_neighbors)
        return;
    delete_neighbors = false;
    while ((ip = iiter.get_next()))
    {
        prev = &ip->if_Neighborlist;
        while ((np = *prev))
        {
            if (np->n_state == NBS_DOWN)
            {
                *prev = np->next;
                delete np;
                ip->if_Neighborcnt--;
            }
            else
                prev = &np->next;
        }
    }
}


