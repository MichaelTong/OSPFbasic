#include "include.h"
#include "system.h"

//根据当前状态和事件，得到响应的动作
int OSPF::run_fsm(FSMmachine *table, int &i_state,int event)
{
    FSMmachine *trans;

    for(trans=table; trans->states; trans++)
    {
        if((trans->states&i_state)==0)
            continue;
        if(trans->event!=event)
            continue;
        if(trans->new_state!=0)
            i_state=trans->new_state;
        return (trans->action);
    }

    return (-1);
}

Pkt::Pkt(int rcvint, InPkt *inpkt)
{
    int iphlen;
    iphlen = (inpkt->i_vhlen & 0xf) << 2;

    iphead = inpkt;
    phyint = rcvint;
    llmult = false;
    hold = false;

    spfpkt = (SpfPkt *) (((byte *) iphead) + iphlen);
    end = (((byte *) iphead) + ntoh16(iphead->i_len));
    bsize = ntoh16(inpkt->i_len) - iphlen;
    dptr = (byte *) spfpkt;
}

Pkt::Pkt()
{
    iphead = 0;
    phyint = -1;
    llmult = false;
    hold = false;
    chksummed = false;
    spfpkt = 0;
    end = 0;
    bsize = 0;
    dptr = 0;
    body_chksum = 0;
}

//校验和
uns16 incksum(uns16 *ptr, int len, uns16 seed)

{
    uns32 xsum=seed;

    for (; len > 1; len -= 2)
    {
        xsum += *ptr++;
        if (xsum & 0x10000)
        {
            xsum += 1;
            xsum &= 0xffff;
        }
    }

    if (len == 1)
    {
        xsum += hton16(*((byte *)ptr));
        if (xsum & 0x10000)
        {
            xsum += 1;
            xsum &= 0xffff;
        }
    }

    return ((xsum == 0xffff) ? 0 : (uns16) xsum);
}

bool Pkt::partial_checksum()
{
    int datasize;
    byte *bodyptr;

    if (!iphead)
        return(false);
    bodyptr = (byte *) (spfpkt+1);
    datasize = dptr - bodyptr;
    body_chksum = incksum((uns16 *) bodyptr, datasize);
    chksummed = true;
    return(true);
}

//生成基本的IP包
int OSPF::getpkt(Pkt *pkt,int type,uns16 size)
{
    InPkt *ipheader;
    SpfPkt *spfpkt;

    if(!(ipheader=sys->getpkt(size+16)))
        return (0);

    pkt->iphead=ipheader;
    pkt->phyint=-1;
    pkt->llmult=0;
    pkt->spfpkt=(SpfPkt*)(ipheader+1);
    pkt->end=((byte*)ipheader)+size;
    pkt->bsize=size-sizeof(ipheader);
    pkt->dptr=(byte*)(pkt->spfpkt+1);

    ipheader->i_vhlen=IHLVER;
    ipheader->i_tos=PREC_IC;
    ipheader->i_ffo=0;
    ipheader->i_prot=PROT_OSPF;

    spfpkt=pkt->spfpkt;
    spfpkt->vers=OSPFv2;
    spfpkt->type=type;
    spfpkt->srcid=hton32(ospf->my_id());

    return (size);
}

//生成包
void Interface::finish_pkt(Pkt *pkt,InAddr addr)
{
    InPkt *ipheader;
    SpfPkt *spfpkt;
    int size;

    pkt->phyint=if_physic;

    spfpkt=pkt->spfpkt;
    size=pkt->dptr-(byte*)spfpkt;
    spfpkt->plen=hton16(size);
    spfpkt->p_aid=hton32(0);
    gen_msg(pkt);

    ipheader=pkt->iphead;
    size=pkt->dptr-(byte*)ipheader;
    ipheader->i_len=hton16(size);
    ipheader->i_id = 0;
    ipheader->i_ttl = 1;
    ipheader->i_src = (if_addr != 0 ? hton32(if_addr) : hton32(ospf->my_addr()));
    ipheader->i_dest = hton32(addr);
    ipheader->i_chksum = 0;
    ipheader->i_chksum = ~incksum((uns16 *) ipheader, sizeof(*ipheader));
}

//为包分配空间
InPkt *SysCalls::getpkt(uns16 len)

{
    return ((InPkt *) new char[len]);
}

SysCalls::~SysCalls()
{

}

//释放IP包
void SysCalls::freepkt(InPkt *pkt)
{
    delete []((char*)pkt);
}


//释放包
void OSPF::freepkt(Pkt *pkt)

{
    if (!pkt->iphead)
        return;
    if (pkt->hold)
        return;

    sys->freepkt(pkt->iphead);
    pkt->iphead = 0;
    pkt->spfpkt = 0;
    pkt->end = 0;
    pkt->bsize = 0;
    pkt->dptr = 0;

    pkt->chksummed = false;
}
