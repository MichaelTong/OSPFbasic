const byte OSPFv2 = 2;

const age_t LSRefreshTime = 1800;
const age_t MinLSInterval = 5;
const age_t MaxAge = 3600;
const age_t CheckAge = 300;
const age_t MaxAgeDiff = 900;
const age_t DoNotAge = 0x8000;
const age_t MinArrival = 1;

const uns32 LSInfinity = 0xffffffL;
const uns32 DefaultDest = 0L;
const uns32 DefaultMask	= 0L;
const uns32 HostMask = 0xffffffffL;
const uns32 UnknownAddr	= 0L;

const seq_t InvalidLSSeq = 0x80000000L;
const seq_t InitLSSeq = 0x80000001L;
const seq_t MaxLSSeq = 0x7fffffffL;
const seq_t LSInvalid = 0x80000000L;

const aid_t BACKBONE = 0;
const InAddr AllSPFRouters = 0xe0000005; // 224.0.0.5
const InAddr AllDRouters = 0xe0000006;  // 224.0.0.6
