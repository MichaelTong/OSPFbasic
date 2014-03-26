class IfcList
{
    Interface *next;
    class OSPF *inst;

public:
    inline IfcList(class OSPF *);
    inline void reset();
    inline Interface *get_next();
};

inline IfcList::IfcList(class OSPF *ospf)
{
    next=ospf->ifcs;
    inst=ospf;
}

inline void IfcList::reset()
{
    next = inst->ifcs;
}
inline Interface *IfcList::get_next()
{
    Interface *retval;
    retval = next;
    if (next)
        next = next->next;
    return(retval);
}

class NbrList
{
    Neighbor *next;
    Interface *ifc;
public:
    inline NbrList(Interface *);
    inline void reset();
    inline Neighbor *getnext();
};

inline NbrList::NbrList(Interface *i)
{
    next=i->if_Neighborlist;
    ifc=i;
}

inline void NbrList::reset()
{
    next=ifc->if_Neighborlist;
}

inline Neighbor *NbrList::getnext()
{
    Neighbor *retval;
    retval=next;
    if(next)
        next=next->next;
    return (retval);
}

