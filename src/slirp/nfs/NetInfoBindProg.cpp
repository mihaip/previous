//
//  NetInfoBindProg.cpp
//  Slirp
//
//  Created by Simon Schubiger on 9/1/21.
//

#include "nfsd.h"
#include "NetInfoBindProg.h"
#include "configuration.h"
#include "UFS.h"

#include <sstream>
#include <cstring>

using namespace std;

extern void add_rpc_program(CRPCProg *pRPCProg, uint16_t port = 0);

class NIProps
{
private:
    map<string, string> m_map;
public:
    NIProps() {}
    
    NIProps(const string& key, const string& val) {
        m_map[key] = val;
    }
    NIProps& operator()(const string& key, const string& val) {
        m_map[key] = val;
        return *this;
    }
    operator map<string, string>() {
        return m_map;
    }
    
    string& operator[] (const string& key) {
        return m_map[key];
    }
};

static string ip_addr_str(uint32_t addr, size_t count) {
    stringstream ss;
    switch(count) {
        case 3:
            ss << (0xFF&(addr >> 24)) << "." << (0xFF&(addr >> 16)) << "." <<  (0xFF&(addr >> 8));
            break;
        case 4:
            ss << (0xFF&(addr >> 24)) << "." << (0xFF&(addr >> 16)) << "." <<  (0xFF&(addr >> 8)) << "." << (0xFF&(addr));
            break;
    }
    return ss.str();
}

extern "C" int nfsd_read(const char* path, size_t fileOffset, void* dst, size_t dstSize);

int CNetInfoBindProg::tryRead(const std::string& path, void* dst, size_t dstSize) {
    int count = nfsd_read(path.c_str(), 0, dst, dstSize);
    if(count > 0) {
        log("read '%s' from '%s%s'", path.c_str(), ConfigureParams.Ethernet.szNFSroot, path.c_str());
        return count;
    }
    
    for(int i = 0; i < 6; i++) {
        DiskImage im(ConfigureParams.SCSI.target[i].szImageName);
        if(im.valid()) {
            for(size_t part = 0; part < im.parts.size(); part++) {
                if(im.parts[part].isUFS()) {
                    UFS ufs(im.parts[part]);
                    icommon inode;
                    if(ufs.findInode(inode, path, true)) {
                        dstSize = ufs.fileSize(inode);
                        if(ufs.readFile(inode, 0, dstSize, static_cast<uint8_t*>(dst)) == ERR_NO) {
                            log("read '%s' from '%s:%d'", path.c_str(), ConfigureParams.SCSI.target[i].szImageName, part);
                            return dstSize;
                        }
                        else
                            break;
                    }
                }
            }
        }
    }
    
    return 0;
}

static vector<string> tokenize(const char *data, const char *sep) {
    vector<string> tokens;
    const char *p;
    int i, j, len;
    char buf[4096];
    int scanning;

    if (!(data)) return tokens;
    if (!(sep)) {
        tokens.push_back((char *)data);
        return tokens;
    }

    len = strlen(sep);

    p = data;

    while (p[0] != '\0') {
        /* skip leading white space */
        while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

        /* check for end of line */
        if (p[0] == '\0') break;

        /* copy data */
        i = 0;
        scanning = 1;
        for (j = 0; (j < len) && (scanning == 1); j++)
            if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;

        while (scanning == 1) {
            buf[i++] = p[0];
            p++;
            for (j = 0; (j < len) && (scanning == 1); j++)
                if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
        }
    
        /* back over trailing whitespace */
        i--;
        if (i > -1) /* did we actually copy anything? */
            while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
        buf[++i] = '\0';
    
        tokens.push_back(buf);

        /* check for end of line */
        if (p[0] == '\0') break;

        /* skip separator */
        scanning = 1;
        for (j = 0; (j < len) && (scanning == 1); j++) {
            if (p[0] == sep[j]) {
                p++;
                scanning = 0;
            }
        }

        if ((scanning == 0) && p[0] == '\0') {
            /* line ended at a separator - add a null member */
            tokens.push_back("");
            return tokens;
        }
    }
    return tokens;
}

static bool parse_user(char* data, NIProps& props) {
    if (data == NULL) return false;

    vector<string> tokens = tokenize(data, ":");
    if (tokens.size() !=  7) return false;

    props("name",            tokens[0]);
    props("_writers_passwd", tokens[0]);
    props("passwd",          "");
    props("uid",             tokens[2]);
    props("gid",             tokens[3]);
    props("realname",        tokens[4]);
    props("home",            tokens[5]);
    props("shell",           tokens[6]);
    
    return true;
}

static bool parse_group(char* data, NIProps& props) {
    if (data == NULL) return false;

    vector<string> tokens = tokenize(data, ":");
    if (tokens.size() !=  4) return false;

    props("name",            tokens[0]);
    props("passwd",          tokens[1]);
    props("gid",             tokens[2]);
    if(!(tokens[3]).empty())
        props("users",       tokens[3]);
    
    return true;
}

void CNetInfoBindProg::addHost(CNetInfoProg& host, const string& name, const string& systemType) {
    add_rpc_program(&host);
    doRegister(host.mTag, host.getPortUDP(), host.getPortTCP());
    host.mRoot.add("trusted_networks", ip_addr_str(CTL_NET, 3));
    string master(name);
    master += "/local";
    //host.mRoot.add("master", name);
    NetInfoNode* machines   = host.mRoot.add(NIProps("name","machines"));
    machines->add(NIProps("name","broadcasthost")("ip_address",ip_addr_str(0xFFFFFFFF, 4))("serves","../network"));
    machines->add(NIProps("name","localhost")("ip_address",ip_addr_str(0x7F000001, 4))("serves","./local")("netgroups","")("system_type",systemType));
}

CNetInfoBindProg::CNetInfoBindProg()
    : CRPCProg(PROG_NETINFOBIND, 1, "nibindd")
    , m_Local("local")
    , m_Network("network")
{
    #define RPC_PROG_CLASS  CNetInfoBindProg
    
    SET_PROC(1, REGISTER);
    SET_PROC(2, UNREGISTER);
    SET_PROC(3, GETREGISTER);
    SET_PROC(4, LISTREG);
    SET_PROC(5, CREATEMASTER);
    SET_PROC(6, CREATECLONE);
    SET_PROC(7, DESTROYDOMAIN);
    SET_PROC(8, BIND);
    
    string systemType = ConfigureParams.System.nMachineType == NEXT_STATION ? "NeXTstation" : "NeXTcube";
    if(ConfigureParams.System.nMachineType == NEXT_STATION && ConfigureParams.System.bColor)
        systemType += " Color";

    addHost(m_Local, NAME_HOST, systemType);

    add_rpc_program(&m_Network, PORT_NETINFO);
    doRegister(m_Network.mTag, m_Network.getPortUDP(), m_Network.getPortTCP());
    
    m_Network.mRoot.add("master", NAME_NFSD "/network");
    m_Network.mRoot.add("trusted_networks", ip_addr_str(CTL_NET, 3));
    NetInfoNode* machines   = m_Network.mRoot.add(NIProps("name","machines"));
    
    char hostname[_SC_HOST_NAME_MAX];
    hostname[0] = '\0';
    gethostname(hostname, sizeof(hostname));
        
    machines->add(NIProps("name",hostname) ("ip_address",ip_addr_str(CTL_NET|CTL_ALIAS, 4)));
    machines->add(NIProps("name",NAME_HOST)("ip_address",ip_addr_str(CTL_NET|CTL_HOST,  4))("serves",NAME_HOST"/local")("netgroups","")("system_type",systemType));
    machines->add(NIProps("name",NAME_DNS) ("ip_address",ip_addr_str(CTL_NET|CTL_DNS,   4)));
    machines->add(NIProps("name",NAME_NFSD)("ip_address",ip_addr_str(CTL_NET|CTL_NFSD,  4))("serves","./network,../network"));

    NetInfoNode* mounts     = m_Network.mRoot.add(NIProps("name","mounts"));
    mounts->add(NIProps("name",NAME_NFSD":/")("dir","/Net")("opts","rw,net"));

    /*
    NetInfoNode* printers   = m_Network.mRoot.add(NIProps("name","printers"));
    NetInfoNode* fax_modems = m_Network.mRoot.add(NIProps("name","fax_modems"));
    NetInfoNode* aliases    = m_Network.mRoot.add(NIProps("name","aliases"));
    
    const size_t buffer_size = 1024*1024;
    char* buffer = new char[buffer_size];

    NetInfoNode* groups     = m_Network.mRoot.add(NIProps("name","groups"));
    
    int count = tryRead("/etc/group", buffer, buffer_size);
    if(count > 0) {
        buffer[count] = '\0';
        char* line = strtok(buffer, "\n");
        while(line) {
            NIProps props;
            if(parse_group(line, props))
                groups->add(props);
            line  = strtok(NULL, "\n");
        }
    }
    
    groups->addEx(NIProps("name","wheel")   ("gid","0") ("passwd","*")("users","root,me"));
    groups->addEx(NIProps("name","nogroup") ("gid","-2")("passwd","*"));
    groups->addEx(NIProps("name","daemon")  ("gid","1") ("passwd","*")("users","daemon"));
    groups->addEx(NIProps("name","sys")     ("gid","2") ("passwd","*"));
    groups->addEx(NIProps("name","bin")     ("gid","3") ("passwd","*"));
    groups->addEx(NIProps("name","uucp")    ("gid","4") ("passwd","*"));
    groups->addEx(NIProps("name","kmem")    ("gid","5") ("passwd","*"));
    groups->addEx(NIProps("name","news")    ("gid","6") ("passwd","*"));
    groups->addEx(NIProps("name","ingres")  ("gid","7") ("passwd","*"));
    groups->addEx(NIProps("name","tty")     ("gid","8") ("passwd","*"));
    groups->addEx(NIProps("name","operator")("gid","9") ("passwd","*"));
    groups->addEx(NIProps("name","staff")   ("gid","10")("passwd","*")("users","root,me"));
    groups->addEx(NIProps("name","other")   ("gid","20")("passwd","*"));
                      
    NetInfoNode* users     = m_Network.mRoot.add(NIProps("name","users"));
    
    count = tryRead("/etc/passwd", buffer, buffer_size);
    if(count > 0) {
        buffer[count] = '\0';
        char* line = strtok(buffer, "\n");
        while(line) {
            NIProps props;
            if(parse_user(line, props))
                users->add(props)->add(NIProps("name","info")("_writers", props["name"]));
            line  = strtok(NULL, "\n");
        }
    }
    
    NetInfoNode* user_root = users->addEx(NIProps("name","root")("_writers_passwd","root")("gid","1")("home","/")("passwd","")("realname","Operator")("shell","/bin/csh")("uid","0"));
    user_root->addEx(NIProps("name","info")("_writers","root"));
    
    delete[] buffer;
     */
}
                                       
CNetInfoBindProg::~CNetInfoBindProg() {}
 
void CNetInfoBindProg::doRegister(const std::string& tag, uint32_t udpPort, uint32_t tcpPort) {
    m_Register[tag].tcp_port = tcpPort;
    m_Register[tag].udp_port = udpPort;
    
    log("%s tcp:%d udp:%d", tag.c_str(), tcpPort, udpPort);
}

int CNetInfoBindProg::procedureREGISTER() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureUNREGISTER() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureGETREGISTER() {
    XDRString tag;
    m_in->read(tag);
    
    std::map<std::string, nibind_addrinfo>::iterator it = m_Register.find(tag.c_str());
    
    if(it == m_Register.end()) {
        log("no tag %s", tag.c_str());
        m_out->write(NI_NOTAG);
    } else {
        log("%s tcp:%d udp:%d", tag.c_str(), it->second.tcp_port, it->second.udp_port);
        m_out->write(NI_OK);
        m_out->write(it->second.udp_port);
        m_out->write(it->second.tcp_port);
    }

    return PRC_OK;
}
int CNetInfoBindProg::procedureLISTREG() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureCREATEMASTER() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureCREATECLONE() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureDESTROYDOMAIN() {
    return PRC_NOTIMP;
}
int CNetInfoBindProg::procedureBIND() {
    uint32_t  clientAddr;
    XDRString clientTag;
    XDRString serverTag;
    
    m_in->read(&clientAddr);
    m_in->read(clientTag);
    m_in->read(serverTag);

    m_out->write(NI_OK);

    return PRC_OK;
}
