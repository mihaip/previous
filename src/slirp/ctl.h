#define CTL_CMD		0
#define CTL_ALIAS	2
#define CTL_GATEWAY CTL_ALIAS
#define NAME_HOST   "previous"
#define CTL_HOST    15
#define CTL_DNS		3
#define NAME_DNS    "nameserver.local"
#define CTL_NFSD    254
#define NAME_NFSD   "shared.local"

#define CTL_NET          0x0A000200 //10.0.2.0
#define CTL_NET_MASK     0xFFFFFF00 //255.255.255.0
#define CTL_CLASS_MASK(x)   (((x & 0x80000000) == 0x00000000) ? 0xFF000000 : \
                             ((x & 0xC0000000) == 0x80000000) ? 0xFFFF0000 : \
                                                                0xFFFFFF00 )
