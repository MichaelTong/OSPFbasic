#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"

//发送LS Request
void Neighbor::send_req()
{
    Interface *ip;
    Pkt pkt;
    ReqPkt *rqpkt;
    LSRef *lsref;
    LsaListIterator iter(&reqlist);
    int count;

    if (reqlist.is_empty())
        return;

    ip = n_ifc;
    pkt.iphead = 0;
    if (ospf->getpkt(&pkt, SPT_LSREQ, ip->mtu) == 0)
        return;

    rqpkt = (ReqPkt *) (pkt.spfpkt);
    pkt.dptr = (byte *) (rqpkt + 1);
    count = 0;

    for (lsref = (LSRef *) pkt.dptr; ; )
    {
        LSA *lsap;
        if (!(lsap = iter.get_next()))
            break;
        if (pkt.dptr + sizeof(LSRef) > pkt.end)
            break;
        lsref->ls_type = hton32(lsap->ls_type());
        lsref->ls_id = hton32(lsap->ls_id());
        lsref->ls_org = hton32(lsap->adv_rtr());
        pkt.dptr += sizeof(LSRef);
        lsref++;
        count++;
    }

    req_goal = reqlist.count() - count;
    ip->Neighbor_send(&pkt, this);
}

//LS Requeset计时器
void LSRTimer::action()
{
    np->send_req();
}

//收到LS Requuest
void Neighbor::recv_req(Pkt *pdesc)
{
    ReqPkt *rqpkt;
    LSRef *lsref;
    Interface *ip;

    if (n_state < NBS_EXCHANGE)
        return;

    ip = n_ifc;

    rqpkt = (ReqPkt *) pdesc->spfpkt;
    lsref = (LSRef *) (rqpkt + 1);
    for (; (byte *) (lsref + 1) <= pdesc->end; lsref++)
    {
        lsid_t ls_id;
        rtid_t ls_org;
        LSA *lsap;
        LShdr *hdr;
        byte lstype;

        lstype = ntoh32(lsref->ls_type);
        ls_id = ntoh32(lsref->ls_id);
        ls_org = ntoh32(lsref->ls_org);
        if (!(lsap = ospf->FindLSA(ip, lstype, ls_id, ls_org)))
        {
            nbr_fsm(NBE_BADLSREQ);
            return;
        }
        hdr = ospf->BuildLSA(lsap);
        (void) add_to_update(hdr);
    }

    ip->Neighbor_send(&n_update, this);
}
