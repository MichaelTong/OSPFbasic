#ifndef PHYINT_H
#define PHYINT_H


class PhyInt:public TblItem
{
    int ipmult;
    bool op;
    InAddr my_addr;
    Interface *ospf_if;



public:
    PhyInt(int phyint);
    virtual ~PhyInt();

};

#endif // PHYINT_H
