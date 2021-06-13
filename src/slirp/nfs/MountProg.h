#ifndef _MOUNTPROG_H_
#define _MOUNTPROG_H_

#include <string>
#include <map>

#include "RPCProg.h"

#define MOUNT_NUM_MAX 100

enum pathFormats {
    FORMAT_PATH = 1,
    FORMAT_PATHALIAS = 2
};

class CMountProg : public CRPCProg
{
public:
	CMountProg();
	~CMountProg();
protected:
	int m_nMountNum;
    
    char  m_exportPath[MAXPATHLEN];
	char* m_clientAddr[MOUNT_NUM_MAX];

	int   ProcedureMNT(void);
	int   ProcedureUMNT(void);
    int   ProcedureUMNTALL(void);
    int   ProcedureEXPORT(void);
};

#endif
