extern FSMmachine IfcFSM[];

enum
{
    IFS_DOWN = 0x01,		//接口被关闭
    IFS_LOOP = 0x02,        //接口循环
    IFS_WAIT = 0x04,        //接口等待确定哪一个为BDR
    IFS_PP  = 0x08,         //点到点
    IFS_OTHER = 0x10,       //既不是DR也不是BDR
    IFS_BACKUP = 0x20,      //BDR
    IFS_DR  = 0x40,         //DR

    N_IF_STATES	= 7,        //接口状态数

    IFS_MULTI = (IFS_OTHER | IFS_BACKUP | IFS_DR),//多路状态
    IFS_ANY = 0x7F,         //所有状态
};

enum
{
    IFE_UP  = 1,            //端口开启
    IFE_WTIM,               //等待计时器出发
    IFE_BSEEN,              //发现BDR
    IFE_NCHG,               //邻居状态发生变化
    IFE_LOOP,               //接口循环
    IFE_UNLOOP,             //接口解除循环
    IFE_DOWN,               //接口关闭

    N_IF_EVENTS	= IFE_DOWN, //接口事件数
};

enum
{
    IFA_START = 1,          //接口开启
    IFA_ELECT,              //接口选举
    IFA_RESET,              //接口重置
};
