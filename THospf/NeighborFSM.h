enum
{
    NBS_DOWN = 0x01,	// �ھӶϿ�
    NBS_ATTEMPT = 0x02,	// ���Է���Hello(NBMA)
    NBS_INIT = 0x04,	// ����ͨ��
    NBS_2WAY = 0x08,	// ˫��ͨ��
    NBS_EXSTART = 0x10,	// Э������
    NBS_EXCHANGE = 0x20,	// ��ʼ����DD
    NBS_LOADING = 0x40,	// DD������ϣ�����ֻ��LS����
    NBS_FULL = 0x80,	// ��ȫ�ڽ�

    NBS_ACTIVE = 0xFE,	// ���Ͽ�֮�������״̬
    NBS_FLOOD = NBS_EXCHANGE | NBS_LOADING | NBS_FULL, //����
    NBS_ADJFORM = NBS_EXSTART | NBS_FLOOD, //�ڽ��γ�
    NBS_BIDIR = NBS_2WAY | NBS_ADJFORM, //˫��
    NBS_PRELIM = NBS_DOWN | NBS_ATTEMPT | NBS_INIT, //˫�����ӽ���֮ǰ��״̬
    NBS_ANY = 0xFF,	// All states
};

enum
{
    NBE_HELLORCVD = 1,	// ���յ�Hello����
    NBE_START,		// ��ʼ����Hello
    NBE_2WAYRCVD,		// ˫��
    NBE_NEGDONE,	// Э������
    NBE_EXCHANGEDONE,	// ��������ݿ⽻��
    NBE_BADLSREQ,	// �յ���LS request�д�
    NBE_LOADINGDONE,		// ��ȡ���
    NBE_EVAL,		// �����Ƿ�Ҫ�γ��ڽӣ��뱾OSPF��������ͬʱ�������ݿ⽻�����й�
    NBE_DDRCVD,		// �յ���Ч��DD
    NBE_SEQNOMISM,	// DD��Ŵ���
    NBE_1WAYRCVD,		// ����
    NBE_KILLNBR,	// �Ͽ��ھ�
    NBE_INACTIVE,	// һ��ʱ����û���յ�Hello
    NBE_LLDOWN,		// ��·�Ͽ�
    NBE_ADJTMO,		// �γ��ڽӳ�ʱ
};

enum
{
    NBA_START   = 1,    // ��ʼ���ھ�����
    NBA_RESTART_INACTT, // �����ھӲ����ʱ��
    NBA_START_INACTT,   // ��ʼ�ھӲ����ʱ��
    NBA_EVAL1,      // ������ʼ����DD
    NBA_EVAL2,      // �յ��Է�DD��������ʼ����DD
    NBA_SNAPSHOT,   // �������ݿ����
    NBA_EXCHANGEDONE,   // ���ݿ⽻����Ϻ����һϵ�ж���
    NBA_REEVAL,     // ���������Ƿ�Ӧ�����ھ��γ��ڽ�
    NBA_RESTART_DD, // ���¿�ʼ���ݿⷢ��
    NBA_DELETE,     // ɾ���ھ�
    NBA_CLR_LISTS,  // ������ݿ�
    NBA_HELLOCHK,
};


