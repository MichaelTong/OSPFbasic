#include "include.h"
#include "phyint.h"
#include "InterfaceFSM.h"
#include "system.h"

PhyInt::PhyInt(int phyint)
    : TblItem((uns32)phyint,0)
{

}

PhyInt::~PhyInt()
{

}
//添加启用的物理端口到记录中
void OSPF::phy_attach(int phyint)
{
    PhyInt *phyp;
    if (!(phyp = (PhyInt *)phyints.find((uns32) phyint, 0)))
    {
        phyp = new PhyInt((uns32) phyint);
        phyints.add(phyp);
        sys->phy_open(phyint);
    }

    phyp->ref();
}
//删除端口记录
void OSPF::phy_detach(int phyint, InAddr if_addr)

{
    PhyInt *phyp;

    if ((phyp = (PhyInt *)phyints.find((uns32) phyint, 0)))
    {
        phyp->deref();
        if (phyp->not_referenced())
        {
            sys->phy_close(phyint);
            phyints.remove(phyp);
            delete phyp;
        }
    }
}

//加入多播组
void OSPF::multi_join(int phyint,InAddr group)
{
    TblItem *member;
    if(!(member=ospf_member.find((uns32)phyint,group)))
    {
        member= new TblItem((uns32)phyint,group);
        sys->join(group,phyint);
    }
    member->ref();
}
//离开多播组
void OSPF::multi_leave(int phyint, InAddr group)
{
    TblItem *member;

    if ((member = ospf_member.find((uns32) phyint, group)))
    {
        member->deref();
        if (member->not_referenced())
        {
            sys->leave(group, phyint);
            ospf_member.remove(member);
            member->chkref();
        }
    }
}
