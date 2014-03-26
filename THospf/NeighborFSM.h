enum
{
    NBS_DOWN = 0x01,	// 邻居断开
    NBS_ATTEMPT = 0x02,	// 尝试发送Hello(NBMA)
    NBS_INIT = 0x04,	// 单向通信
    NBS_2WAY = 0x08,	// 双向通信
    NBS_EXSTART = 0x10,	// 协商主从
    NBS_EXCHANGE = 0x20,	// 开始发送DD
    NBS_LOADING = 0x40,	// DD发送完毕，现在只有LS请求
    NBS_FULL = 0x80,	// 完全邻接

    NBS_ACTIVE = 0xFE,	// 除断开之外的所有状态
    NBS_FLOOD = NBS_EXCHANGE | NBS_LOADING | NBS_FULL, //泛洪
    NBS_ADJFORM = NBS_EXSTART | NBS_FLOOD, //邻接形成
    NBS_BIDIR = NBS_2WAY | NBS_ADJFORM, //双向
    NBS_PRELIM = NBS_DOWN | NBS_ATTEMPT | NBS_INIT, //双向连接建立之前的状态
    NBS_ANY = 0xFF,	// All states
};

enum
{
    NBE_HELLORCVD = 1,	// 接收到Hello分组
    NBE_START,		// 开始发送Hello
    NBE_2WAYRCVD,		// 双向
    NBE_NEGDONE,	// 协商主从
    NBE_EXCHANGEDONE,	// 完成了数据库交换
    NBE_BADLSREQ,	// 收到的LS request有错
    NBE_LOADINGDONE,		// 读取完毕
    NBE_EVAL,		// 评估是否要形成邻接，与本OSPF允许的最多同时进行数据库交换数有关
    NBE_DDRCVD,		// 收到有效的DD
    NBE_SEQNOMISM,	// DD序号错误
    NBE_1WAYRCVD,		// 单向
    NBE_KILLNBR,	// 断开邻居
    NBE_INACTIVE,	// 一段时间内没有收到Hello
    NBE_LLDOWN,		// 链路断开
    NBE_ADJTMO,		// 形成邻接超时
};

enum
{
    NBA_START   = 1,    // 开始与邻居连接
    NBA_RESTART_INACTT, // 重置邻居不活动计时器
    NBA_START_INACTT,   // 开始邻居不活动计时器
    NBA_EVAL1,      // 本机开始发送DD
    NBA_EVAL2,      // 收到对方DD，本机开始发送DD
    NBA_SNAPSHOT,   // 进行数据库快照
    NBA_EXCHANGEDONE,   // 数据库交换完毕后进行一系列动作
    NBA_REEVAL,     // 重新评估是否应该与邻居形成邻接
    NBA_RESTART_DD, // 重新开始数据库发送
    NBA_DELETE,     // 删除邻居
    NBA_CLR_LISTS,  // 清空数据库
    NBA_HELLOCHK,
};


