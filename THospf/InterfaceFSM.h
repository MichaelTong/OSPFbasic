extern FSMmachine IfcFSM[];

enum
{
    IFS_DOWN = 0x01,		//�ӿڱ��ر�
    IFS_LOOP = 0x02,        //�ӿ�ѭ��
    IFS_WAIT = 0x04,        //�ӿڵȴ�ȷ����һ��ΪBDR
    IFS_PP  = 0x08,         //�㵽��
    IFS_OTHER = 0x10,       //�Ȳ���DRҲ����BDR
    IFS_BACKUP = 0x20,      //BDR
    IFS_DR  = 0x40,         //DR

    N_IF_STATES	= 7,        //�ӿ�״̬��

    IFS_MULTI = (IFS_OTHER | IFS_BACKUP | IFS_DR),//��·״̬
    IFS_ANY = 0x7F,         //����״̬
};

enum
{
    IFE_UP  = 1,            //�˿ڿ���
    IFE_WTIM,               //�ȴ���ʱ������
    IFE_BSEEN,              //����BDR
    IFE_NCHG,               //�ھ�״̬�����仯
    IFE_LOOP,               //�ӿ�ѭ��
    IFE_UNLOOP,             //�ӿڽ��ѭ��
    IFE_DOWN,               //�ӿڹر�

    N_IF_EVENTS	= IFE_DOWN, //�ӿ��¼���
};

enum
{
    IFA_START = 1,          //�ӿڿ���
    IFA_ELECT,              //�ӿ�ѡ��
    IFA_RESET,              //�ӿ�����
};
