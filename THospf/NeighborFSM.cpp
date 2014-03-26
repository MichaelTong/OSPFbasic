#include "include.h"
#include "Interface.h"
#include "NeighborFSM.h"
#include "InterfaceFSM.h"
#include "system.h"

//邻居状态机的状态转移表
FSMmachine NbrFSM[]=
{
    { NBS_ACTIVE,       NBE_HELLORCVD,	        NBA_RESTART_INACTT,	    0},
    { NBS_BIDIR,        NBE_2WAYRCVD,	        0,		                0},
    { NBS_INIT,	        NBE_1WAYRCVD,	        0,		                0},
    { NBS_DOWN,	        NBE_HELLORCVD,	        NBA_START_INACTT,	    NBS_INIT},
    { NBS_DOWN,	        NBE_START,	            NBA_START,	            NBS_ATTEMPT},
    { NBS_ATTEMPT,      NBE_HELLORCVD,	        NBA_RESTART_INACTT,	    NBS_INIT},
    { NBS_INIT,	        NBE_2WAYRCVD,	        NBA_EVAL1,	            0},
    { NBS_INIT,	        NBE_DDRCVD,	            NBA_EVAL2,	            0},
    { NBS_2WAY,	        NBE_DDRCVD,	            NBA_EVAL2,	            0},
    { NBS_EXSTART,	    NBE_NEGDONE,	        NBA_SNAPSHOT,	        NBS_EXCHANGE},
    { NBS_EXCHANGE,	    NBE_EXCHANGEDONE,	    NBA_EXCHANGEDONE,	    0},
    { NBS_LOADING,	    NBE_LOADINGDONE,	    0,		            NBS_FULL},
    { NBS_2WAY,	        NBE_EVAL,	            NBA_EVAL1,	            0},
    { NBS_ADJFORM,      NBE_EVAL,	            NBA_REEVAL,	            0},
    { NBS_PRELIM,       NBE_EVAL,	            NBA_HELLOCHK,	        0},
    { NBS_ADJFORM,      NBE_ADJTMO,	            NBA_RESTART_DD,	        NBS_2WAY},
    { NBS_FLOOD,        NBE_SEQNOMISM,	        NBA_RESTART_DD,	        NBS_2WAY},
    { NBS_FLOOD,        NBE_BADLSREQ,	        NBA_RESTART_DD,	        NBS_2WAY},
    { NBS_ANY,	        NBE_KILLNBR,	        NBA_DELETE,	            NBS_DOWN},
    { NBS_ANY,	        NBE_LLDOWN,	            NBA_DELETE,	            NBS_DOWN},
    { NBS_ANY,	        NBE_INACTIVE,	        NBA_DELETE,	            NBS_DOWN},
    { NBS_BIDIR,        NBE_1WAYRCVD,	        NBA_CLR_LISTS,	        NBS_INIT},
    { 0, 	            0,		                -1,		                0},
};


char *nbrstates(int state)
{
    switch(state)
    {
    case NBS_DOWN:
        return("Down");
    case NBS_ATTEMPT:
        return("Attempting");
    case NBS_INIT:
        return("1-Way");
    case NBS_2WAY:
        return("2-Way");
    case NBS_EXSTART:
        return("ExchangeStart");
    case NBS_EXCHANGE:
        return("Exchange");
    case NBS_LOADING:
        return("Loading");
    case NBS_FULL:
        return("Full");
    default:
        break;
    }

    return("Unknown");
}

char *nbrevents(int event)
{
    switch(event)
    {
    case NBE_HELLORCVD:
        return("HelloReceived");
    case NBE_START:
        return("Start");
    case NBE_2WAYRCVD:
        return("2WayHello");
    case NBE_NEGDONE:
        return("NegotiationDone");
    case NBE_EXCHANGEDONE:
        return("ExchangeDone");
    case NBE_BADLSREQ:
        return("BadLSReq");
    case NBE_LOADINGDONE:
        return("LoadingDone");
    case NBE_EVAL:
        return("EvaluateAdjacency");
    case NBE_DDRCVD:
        return("DDReceived");
    case NBE_SEQNOMISM:
        return("BadDDSequenceNo");
    case NBE_1WAYRCVD:
        return("1WayHello");
    case NBE_KILLNBR:
        return("Kill");
    case NBE_INACTIVE:
        return("HelloTimeout");
    case NBE_LLDOWN:
        return("LLDown");
    case NBE_ADJTMO:
        return("AdjacencyTimeout");
    default:
        break;
    }

    return("Unknown");
}

//邻居状态机
void Neighbor::nbr_fsm(int event)
{
    int action;
    int	n_ostate;	//之前的状态

    n_ostate = n_state;
    action = ospf->run_fsm(&NbrFSM[0], n_state, event);
    printf(">>>Neighbor Event: %s\tState Change: %s ---> %s\tAction: ",nbrevents(event),nbrstates(n_ostate),nbrstates(n_state));
    switch (action)
    {
    case 0:
        printf("No action\n\n");
        break;
    case NBA_START:
        printf("Start, sending hello\n\n");
        send_hello();//开始发送hello
        hellot.start(n_ifc->if_hellointerval*Timer::SECOND);
        inactt.start(n_ifc->if_deadinterval*Timer::SECOND);
        break;
    case NBA_RESTART_INACTT://收到了Hello，重置不活动计时器
        printf("Restart Inactivity Timer\n\n");
        inactt.restart(n_ifc->if_deadinterval*Timer::SECOND);
        break;
    case NBA_START_INACTT://发现邻居，开始不活动计时器
        printf("Start Inactivity Timer\n\n");
        inactt.start(n_ifc->if_deadinterval*Timer::SECOND);
        break;
    case NBA_EVAL1://对于本机开始发送DD，评估是否开始发动DD
        printf("EVAL1\n\n");
        nba_eval1();
        break;
    case NBA_EVAL2://收到对方DD，评估是否要回复DD
        printf("EVAL2\n\n");
        nba_eval2();
        break;
    case NBA_SNAPSHOT://保存数据库信息
        printf("SNAPSHOT\n\n");
        nba_snapshot();
        break;
    case NBA_EXCHANGEDONE:
        printf("Exchange done\n\n");
        nba_exchangedone();//完成数据库交换
        break;
    case NBA_REEVAL:
        printf("Reeval\n\n");
        nba_reeval();//重新评估是否要发送DD
        break;
    case NBA_RESTART_DD://重新发送DD
        printf("Restart DD\n\n");
        nba_clr_lists();//清空LSA重发表等
        AddPendAdj();
        break;
    case NBA_DELETE:
        printf("Delete neighbor\n\n");
        nba_delete();//删除邻居
        break;
    case NBA_CLR_LISTS:
        printf("Clear\n\n");
        nba_clr_lists();//清空LSA重发表等
        break;
    case NBA_HELLOCHK:
        printf("Check Hello\n\n");
        if(n_ifc->type()!=IFT_NBMA&&n_ifc->type()!=IFT_P2MP)
            break;
        if(!n_ifc->adjacency_wanted(this)&&n_state==NBS_ATTEMPT)
        {
            n_state=NBS_DOWN;
            nba_delete();
            break;
        }
        if(n_ifc->adjacency_wanted(this)&&n_state==NBS_DOWN)
            n_state=NBS_ATTEMPT;
        break;
    case -1:
    default:
        printf("Error\n\n");
        return;
    }

    if(n_ostate==n_state)
        return;

    if (n_state == NBS_FULL)
    {
        ospf->n_dbx_nbrs--;//需要交换数据库的邻居数
        progt.stop();
        exit_dbxchg();
        ospf->rl_orig(false);//形成邻接关系了，就需要生成router LSA
    }
    //开始进行数据库交换
    else if (n_ostate <= NBS_EXSTART && n_state > NBS_EXSTART)
        ospf->n_dbx_nbrs++;
    else if (n_ostate == NBS_FULL)//从FULL变到其他状态
    {
        n_ifc->if_fullcnt--;
        ospf->rl_orig(false);
    }
    else if (n_state <= NBS_2WAY && n_ostate >= NBS_EXSTART)
    {
        exit_dbxchg();
        if (n_ostate > NBS_EXSTART)
            ospf->n_dbx_nbrs--;
    }

    // 如果是DR，重新生成network lsa
    if (n_ifc->state() == IFS_DR)
        n_ifc->nl_orig(false);

    if ((n_state >= NBS_2WAY && n_ostate < NBS_2WAY) ||
            (n_state < NBS_2WAY && n_ostate >= NBS_2WAY))
        n_ifc->ifc_fsm(IFE_NCHG);//邻居可能发生变化，驱动接口状态机

}

//完成数据库交换
void Neighbor::exit_dbxchg()
{
    if (remote_init)
        ospf->remote_inits--;
    else
        ospf->local_inits--;
    remote_init = false;
}

//进行数据库快照
void Neighbor::nba_snapshot()
{

    n_ifc->AddTypesToList(LST_RTR, &ddlist);
    n_ifc->AddTypesToList(LST_NET, &ddlist);

}

//删除邻居
void Neighbor::nba_delete()

{
    md5_seqno = 0;
    nba_clr_lists();
    n_dr = UnknownAddr;
    n_bdr = UnknownAddr;
    request_suppression = false;

    inactt.stop();
    hellot.stop();

    ospf->delete_neighbors = true;
}

//某接口上邻居开始
void Interface::ifa_Neighbor_start(int base_priority)

{
    NbrList lis(this);
    Neighbor *np;

    while ((np = lis.getnext()))
    {
        if (np->priority() >= base_priority)
            np->nbr_fsm(NBE_START);
    }
}

//开始邻接
void Neighbor::start_adjacency()
{
    n_ddseq++;
    progt.start(n_ifc->if_deadinterval*Timer::SECOND);
    send_dd();//发送DD
}

//增加等待形成邻接关系的邻居
void Neighbor::AddPendAdj()
{
    if(n_adj_pend)
        return;
    n_adj_pend=true;
    next_pend=0;
    if(!ospf->g_adj_head)
    {
        ospf->g_adj_head=this;
        ospf->g_adj_tail=this;
    }
    else
    {
        ospf->g_adj_tail->next_pend=this;
        ospf->g_adj_tail=this;
    }
}

//删除等待形成邻接关系的邻居
void Neighbor::DelPendAdj()
{
    Neighbor **ptr;
    Neighbor *prev;
    Neighbor *nbr;

    if(!n_adj_pend)
        return;
    prev=0;
    for(ptr=&ospf->g_adj_head; ; prev=nbr,ptr=&nbr->next_pend)
    {
        if(!(nbr=*ptr))
            return;
        if(nbr==this)
            break;
    }

    *ptr=next_pend;
    if(ospf->g_adj_tail==this)
        ospf->g_adj_tail=prev;
    n_adj_pend=false;
}

//对于本机最先开始发送DD，判断是否要开始发送
void Neighbor::nba_eval1()
{
    n_state = NBS_2WAY;
    if (!n_ifc->adjacency_wanted(this))
        DelPendAdj();
    else if (ospf->local_inits < ospf->max_dds)
    {
        n_state = NBS_EXSTART;
        ospf->local_inits++;
        DelPendAdj();
        start_adjacency();
    }
    else
        AddPendAdj();
}

//对于本机收到对方DD，判断是否要开始发送DD
void Neighbor::nba_eval2()
{
    if (!n_ifc->adjacency_wanted(this))
        n_state = NBS_2WAY;
    else if (ospf->remote_inits < ospf->max_dds)
    {
        remote_init = true;
        n_state = NBS_EXSTART;
        ospf->remote_inits++;
        DelPendAdj();
        start_adjacency();
    }
    else
    {
        n_state = NBS_2WAY;
        AddPendAdj();
    }
}

//是否需要更多邻接
int Interface::adjacency_wanted(Neighbor *)
{
    return true;
}

//对于DR接口，是否需要更多邻接
int DRIfc::adjacency_wanted(Neighbor *np)
{
    if(if_state==IFS_DR)
        return true;
    else if(if_state==IFS_BACKUP)
        return true;
    else if (np->is_dr())
        return true;
    else if(np->is_bdr())
        return true;
    else
        return false;
}

//清空邻居有关表，重新开始评估
void Neighbor::nba_reeval()
{
    if (!n_ifc->adjacency_wanted(this))
    {
        nba_clr_lists();
        n_state = NBS_2WAY;
    }
}

//清除邻居有关的表
void Neighbor::nba_clr_lists()
{
    DelPendAdj();
    n_adj_pend = false;

    dd_free();
    // 清除lsa转发表
    clear_retrans_list();

    // 清除其它的表
    ddlist.clear();
    reqlist.clear();
    database_sent = false;

    holdt.stop();
    ddt.stop();
    lsrt.stop();
    progt.stop();
}


void ProgressTimer::action()
{
    np->nbr_fsm(NBE_ADJTMO);
}

//数据库完成交换，判断是否形成完全邻接关系
void Neighbor::nba_exchangedone()
{
    if(reqlist.is_empty())
        n_state=NBS_FULL;
    else
        n_state=NBS_LOADING;
}


