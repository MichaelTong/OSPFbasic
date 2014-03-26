#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"

FSMmachine IfcFSM[]=
{
    { IFS_DOWN,	IFE_UP,		IFA_START,	0},
    { IFS_WAIT,	IFE_BSEEN,	IFA_ELECT,	0},
    { IFS_WAIT,	IFE_WTIM,	IFA_ELECT,	0},
    { IFS_WAIT,	IFE_NCHG,	0,		0},
    { IFS_MULTI, IFE_NCHG,	IFA_ELECT,	0},
    { IFS_ANY,	IFE_NCHG,	0,		0},
    { IFS_ANY,	IFE_DOWN,	IFA_RESET,	IFS_DOWN},
    { IFS_ANY,	IFE_LOOP,	IFA_RESET,	IFS_LOOP},
    { IFS_LOOP,	IFE_UNLOOP,	0,		IFS_DOWN},
    { 0, 	0,		-1,		0},
};

char *ifstates(int state)

{
    switch(state)
    {
    case IFS_DOWN:
        return("Down");
    case IFS_LOOP:
        return("Loopback");
    case IFS_WAIT:
        return("Waiting");
    case IFS_PP:
        return("P-P");
    case IFS_OTHER:
        return("DRother");
    case IFS_BACKUP:
        return("Backup");
    case IFS_DR:
        return("DR");
    default:
        break;
    }

    return("Unknown");
}

char *ifevents(int event)

{
    switch(event)
    {
    case IFE_UP:
        return("PhyUp");
    case IFE_WTIM:
        return("WaitTime");
    case IFE_BSEEN:
        return("BackupSeen");
    case IFE_NCHG:
        return("NeighborChange");
    case IFE_LOOP:
        return("LoopInd");
    case IFE_UNLOOP:
        return("UnloopInd");
    case IFE_DOWN:
        return("PhyDown");
    default:
        break;
    }

    return("Unknown");
}


void Interface::ifc_fsm(int event)
{
    int action;
    int if_ostate;
    if_ostate=if_state;
    action=ospf->run_fsm(&IfcFSM[0],if_state,event);
    printf(">>>Interface Event: %s\tState Change: %s ---> %s\tAction: ",ifevents(event),ifstates(if_ostate),ifstates(if_state));
    switch (action)
    {
    case 0:		// 没有相关的动作
        printf("No action\n\n");
        break;
    case IFA_START:	// 接口启动
        printf("Interface Start\n\n");
        ifa_start();
        break;
    case IFA_ELECT:	// 选举DR
        printf("Elect DR\n\n");
        ifa_elect();
        break;
    case IFA_RESET:	// 重置接口
        printf("Interface Start\n\n");
        ifa_reset();
        break;
    case -1:		// 状态机错误
    default:
        printf("Error\n\n");
        return;
    }

    if(if_ostate==if_state)
        return;
    //设置泛洪中使用的地址
    if_flood=(if_state==IFS_OTHER)?AllDRouters:AllSPFRouters;
    //接口关闭，则清除数据库
    if(if_state==IFS_DOWN)
    {
        delete_lsdb();
    }
    //加入多播组，状态是DR或BDR
    if(if_state>IFS_OTHER&&if_ostate<=IFS_OTHER)
        ospf->multi_join(if_physic,AllDRouters);
    //离开多播组
    else if(if_state<=IFS_OTHER&&if_ostate>IFS_OTHER)
        ospf->multi_leave(if_physic,AllDRouters);
    //重新创建LSA
    ospf->rl_orig(false);
    if(if_state==IFS_DR||if_ostate==IFS_DR)
    {
        nl_orig(false);
    }

}

void DRIfc::ifa_start()
{
    start_hellos();
    // 打开物理接口
    ospf->phy_attach(if_physic);
    ospf->multi_join(if_physic,AllSPFRouters);
    if(if_drpri==0)
        if_state=IFS_OTHER;
    else
    {
        //启动计时器，判断是否有邻居断开
        if_waitt.start(if_deadinterval*Timer::SECOND);
        if_state=IFS_WAIT;
    }
}

void WaitTimer::action()
{
    ifc->ifc_fsm(IFE_WTIM);
}

void Interface::ifa_reset()
{
    if_dr=UnknownAddr;
    if_bdr=UnknownAddr;


    if_waitt.stop();
    stop_hellos();
    if_ackt.stop();
    ifa_allNeighbors_event(NBE_KILLNBR);
}
//选举DR
void Interface::ifa_elect()
{
    int pass;
    InAddr prev_dr;
    InAddr prev_bdr;

    if_waitt.stop();
    prev_dr=if_dr;
    prev_bdr=if_bdr;
    for(pass=1; pass<=2; pass++)
    {//两次，直到结果不变
        int declared;
        byte c_pri;
        rtid_t c_id;
        Neighbor *np;
        NbrList iter(this);
        Neighbor *bdr_p;

        bdr_p=0;
        if (if_dr != if_addr && if_drpri != 0)
        //如果DR不是自己，则首先设置自己为BDR
        {
            declared = (if_bdr == if_addr);
            if_bdr = if_addr;
            c_pri = if_drpri;
            c_id = ospf->my_id();
        }
        else
        //否则设置BDR为0
        {
            if_bdr = UnknownAddr;
            c_pri = 0;
            declared = false;
            c_id = 0;
        }

        while ((np = iter.getnext()))
        {
        //遍历邻居
            if (np->state() < NBS_2WAY)
                continue;
            else if (np->prior == 0)
                continue;
            else if (np->declared_dr())
                continue;
            else if (declared)
            //本机宣称自己是BDR，而邻居没宣称自己是BDR
            //或者自己的优先级高于该邻居
            //或者邻居ID小于本机ID
            //则跳过该邻居
            {
                if (!np->declared_bdr())
                    continue;
                if (np->prior < c_pri)
                    continue;
                if (np->prior == c_pri && np->n_id < c_id)
                    continue;
            }
            else if (!np->declared_bdr())
            //本机没有宣称自己是BDR（自己是DR或本来就不是BDR）
            //或者自己的优先级高于该邻居
            //或者邻居ID小于本机ID
            //则跳过该邻居
            {
                if (np->prior < c_pri)
                    continue;
                if (np->prior == c_pri && np->n_id < c_id)
                    continue;
            }
            // 当前的邻居被选为BDR
            bdr_p = np;
            if_bdr = np->n_addr;
            declared = np->declared_bdr();//已经有BDR
            c_pri = np->prior;
            c_id = np->n_id;
        }

        // 初始化DR选举
        iter.reset();
        if_dr_n = 0;
        if (if_dr == if_addr && if_drpri != 0)
        {//如果自己是DR
            c_pri = if_drpri;
            c_id = ospf->my_id();
        }
        else
        {
            if_dr = UnknownAddr;
            c_pri = 0;
        }
        // 遍历2WAY的邻居
        while ((np = iter.getnext()))
        {
            if (np->state() < NBS_2WAY)
                continue;
            else if (np->prior == 0)
                continue;
            else if (!np->declared_dr())
                continue;
            else if (np->prior < c_pri)
                continue;
            else if (np->prior == c_pri && np->n_id < c_id)
                continue;
            // 找到一个适合当DR的邻居
            if_dr_n = np;
            if_dr = np->n_addr;
            c_pri = np->prior;
            c_id = np->n_id;
        }

        //如果没有选出DR，则设置BDR为DR
        if (if_dr == UnknownAddr)
        {
            if_dr = if_bdr;
            if_dr_n = bdr_p;
        }
        if (if_dr == if_addr)
            if_state = IFS_DR;
        else if (if_bdr == if_addr)
            if_state = IFS_BACKUP;
        else
            if_state = IFS_OTHER;
        //如果DR或BDR的变化是有关本机的，则再进行一次
        if (if_dr !=prev_dr && (if_dr==if_addr || prev_dr==if_addr))
            continue;
        else if (if_bdr == prev_bdr)
            break;
        else if (if_bdr == if_addr || prev_bdr == if_addr)
            continue;
        else
            break;
    }

    //对于选举结果设置自己的状态
    if (if_dr == if_addr)
        if_state = IFS_DR;
    else if (if_bdr == if_addr)
        if_state = IFS_BACKUP;
    else
        if_state = IFS_OTHER;
    ifa_allNeighbors_event(NBE_EVAL);
    if (if_dr != prev_dr)//DR与之前的不同，重新生成Router lsa
        ospf->rl_orig(false);
}
