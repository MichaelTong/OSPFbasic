#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"
//收到DD
void Neighbor::recv_dd(struct Pkt *pdesc)
{
    DDPkt *ddpkt;
    byte new_opts;
    int init;
    int more;
    int claim;
    int master;
    uns32 seqno;
    LShdr *hdr;
    int is_empty;

    ddpkt = (DDPkt *) pdesc->spfpkt;
    new_opts = ddpkt->dd_opts;
    init = ((ddpkt->dd_imms & DD_INIT) != 0);
    more = ((ddpkt->dd_imms & DD_MORE) != 0);
    claim = ((ddpkt->dd_imms & DD_MASTER) == 0);//对方声称的主从关系
    master = (ospf->my_id() > n_id);//id大的为主
    seqno = ntoh32(ddpkt->dd_seqno);

    hdr=(LShdr *)(ddpkt+1);
    is_empty=(pdesc->end==(byte*)hdr);

    if((ntoh16(ddpkt->dd_mtu)>n_ifc->mtu))
    {
        printf("BADMTU\n");
    }

    switch (n_state)
    {
    case NBS_DOWN:
    case NBS_ATTEMPT:
    default:
        return;

    case NBS_INIT:
    case NBS_2WAY:
        nbr_fsm(NBE_DDRCVD);
        //开始数据库交换
        if(n_state!=NBS_EXSTART)
            return;

    case NBS_EXSTART:
        if(claim!=master)//对方声称的主从关系与本机不同
            return;
        //主从关系协商一致
        if (master && (!init) && (seqno == n_ddseq))
        //本机是主，收到的DD分组init为0，序列号相同，表示协商完毕
        {
            n_opts = new_opts;
            nbr_fsm(NBE_NEGDONE);
            break;
        }
        if((!master)&&init&&more&&is_empty)
        //本机是从，收到的DD分组init和more为1，表示写上完毕
        {
            n_opts=new_opts;
            nbr_fsm(NBE_NEGDONE);
            break;
        }
        //跳入数据库交换状态
        return ;
    case NBS_EXCHANGE:
        if (claim != master || init || n_opts != new_opts)//DD错误
        {
            nbr_fsm(NBE_SEQNOMISM);
            return;
        }
        else if (master)//本机是主
        {
            if (seqno == n_ddseq)//收到的序列号与发出的一致
                break;
            if (master && seqno == (n_ddseq - 1))//还未发来最新的回复
                return;
        }
        else//本机是从
        {
            if (seqno == (n_ddseq + 1))//主增加了序列号
                break;
            if (seqno == n_ddseq)//说明主没有收到DD分组，重传DD
            {
                retrans_dd();
                return;
            }
        }

        nbr_fsm(NBE_SEQNOMISM);
        return;
    case NBS_LOADING:
    case NBS_FULL://本机认为已经完全邻接
        if (claim != master || init || n_opts != new_opts)//DD错误
        {
            nbr_fsm(NBE_SEQNOMISM);
            return;
        }
        else if (master && seqno == (n_ddseq - 1))
            //本机是主，对方发送正确的DD在,直接返回
            return;
        else if ((!master) && seqno == n_ddseq)
        //本机是从，收到的序列号没有改变，说明对方没有收到DD，重传
        {
            retrans_dd();
            return;
        }

        nbr_fsm(NBE_SEQNOMISM);
        return;
    }
    //收到了有效的DD分组，
    //释放准备发的DD
    //处理收到的DD里面带有的LSA信息
    //组成LS Req，并发送
    negotiate(n_opts);
    n_imms=ddpkt->dd_imms;
    dd_free();
    ddt.stop();
    progt.restart();
    process_dd(hdr,pdesc->end);
    send_req();
    //如果是主，增加序列号
    if(master)
    {
        n_ddseq++;
        if(database_sent&&(!more))
        {
            nbr_fsm(NBE_EXCHANGEDONE);
            return;
        }
    }
    //如果是从，拷贝序列号
    else
        n_ddseq=seqno;
    //没有请求，发送下一个DD
    if(reqlist.is_empty())
        send_dd();
}
//发送DD
void Neighbor::send_dd()
{
    int master;
    Interface *ip;
    DDPkt *ddpkt;
    LShdr *hdr;
    LSA *lsap;
    LsaListIterator iter(&ddlist);

    master = (ospf->my_id() > n_id);
    ip = n_ifc;
    //由ospf分配最基本的分组
    if (ospf->getpkt(&n_ddpkt, SPT_DD, ip->mtu) == 0)
        return;
    //填入字段
    ddpkt = (DDPkt *) (n_ddpkt.spfpkt);
    ddpkt->dd_mtu = hton16(ip->mtu);
    ddpkt->dd_seqno = hton32(n_ddseq);
    ddpkt->dd_opts |= SPO_EXT;

    n_ddpkt.dptr = (byte *) (ddpkt + 1);
    if (n_state == NBS_EXSTART)
    //如果是数据库交换开始阶段
    //imms设置为DD_INIT | DD_MORE | DD_MASTER
    //直接发送分组，并设定重传时间
    {
        n_ddpkt.hold = true;
        ddpkt->dd_imms = DD_INIT | DD_MORE | DD_MASTER;
        database_sent = false;
        ip->Neighbor_send(&n_ddpkt, this);
        ddt.start(ip->if_rxmt*Timer::SECOND, false);
        return;
    }

    if (master)
        ddt.start(ip->if_rxmt*Timer::SECOND, false);

    progt.restart();
    n_ddpkt.hold = true;
    ddpkt->dd_imms = 0;
    //加入LSA信息
    while ((lsap = iter.get_next()))
    {
        hdr = (LShdr *) n_ddpkt.dptr;
        if (!lsap->valid())
        {
            iter.remove_current();
            continue;
        }
        else if (lsap->lsa_age() == MaxAge)
        //LSA已经达到最大老化
        {
            iter.remove_current();
            add_to_retranslist(lsap);
            hdr = ospf->BuildLSA(lsap);
            (void) add_to_update(hdr);
            continue;
        }
        else if (n_ddpkt.dptr + sizeof(LShdr) > n_ddpkt.end)
        //已经达到最大DD长度
            break;
        else
        {
            iter.remove_current();
            *hdr = *lsap;
            n_ddpkt.dptr += sizeof(LShdr);
        }
    }
    //设置响应的参数
    if (master)
        ddpkt->dd_imms |= DD_MASTER;
    if (!ddlist.is_empty())
    //仍有LSA没有传完
        ddpkt->dd_imms |= DD_MORE;
    else
        database_sent = true;

    ip->Neighbor_send(&n_ddpkt, this);
    ip->Neighbor_send(&n_update, this);

    if ((!master) && database_sent && (n_imms & DD_MORE) == 0)
    {
        nbr_fsm(NBE_EXCHANGEDONE);
        holdt.start(ip->if_deadinterval*Timer::SECOND);
    }
}
//从DD中解析出LSA，并填入数据库，组件LS Req
void Neighbor::process_dd(LShdr *hdr,byte *end)
{
    Interface *ip;

    ip = n_ifc;

    for (; (byte *) hdr < end; hdr++)
    {
        int lstype;
        lsid_t lsid;
        rtid_t orig;
        LSA *lsap;

        lstype = hdr->ls_type;
        lsid = ntoh32(hdr->ls_id);
        orig = ntoh32(hdr->ls_org);
        lsap = ospf->FindLSA(ip, lstype, lsid, orig);

        if (lsap && lsap->cmp_instance(hdr) <= 0)
            continue;

        if (reqlist.is_empty())
            lsrt.start(ip->if_rxmt*Timer::SECOND, false);
        lsap = new LSA(0, hdr, 0);
        reqlist.addEntry(lsap);
    }
}

//重发DD
void Neighbor::retrans_dd()
{
    if(n_ddpkt.iphead)
    {
        n_ifc->Neighbor_send(&n_ddpkt,this);
    }
    else if(n_state==NBS_FULL)
        nbr_fsm(NBE_SEQNOMISM);
}

void DDTimer::action()
{
    np->retrans_dd();
}

void Neighbor::dd_free()
{
    n_ddpkt.hold=false;
    ospf->freepkt(&n_ddpkt);
}

void HoldTimer::action()
{
    np->dd_free();
}

