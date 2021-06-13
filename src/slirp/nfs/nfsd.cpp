#include <stdio.h>
#include <unistd.h>
#include <vector>

#include "nfsd.h"
#include "RPCServer.h"
#include "TCPServerSocket.h"
#include "UDPServerSocket.h"
#include "PortmapProg.h"
#include "NFSProg.h"
#include "MountProg.h"
#include "BootparamProg.h"
#include "configuration.h"
#include "SocketListener.h"
#include "VDNS.h"
#include "FileTableNFSD.h"

static bool         g_bLogOn = true;
static CPortmapProg g_PortmapProg;
static CRPCServer   g_RPCServer;

nfsd_NAT nfsd_ports = {{0,0,0,0},{0,0,0,0}};

static std::vector<UDPServerSocket*> SERVER_UDP;
static std::vector<TCPServerSocket*> SERVER_TCP;

FileTableNFSD* nfsd_fts[] = {NULL}; // to be extended for multiple exports

static bool initialized = false;

static void add_program(CRPCProg *pRPCProg, uint16_t port = 0) {
    UDPServerSocket* udp = new UDPServerSocket(&g_RPCServer);
    TCPServerSocket* tcp = new TCPServerSocket(&g_RPCServer);
    
    g_RPCServer.Set(pRPCProg->GetProgNum(), pRPCProg);

    if (tcp->Open(pRPCProg->GetProgNum(), port) && udp->Open(pRPCProg->GetProgNum(), port)) {
        printf("[NFSD] %s started\n", pRPCProg->GetName());
        pRPCProg->Init(tcp->GetPort(), udp->GetPort());
        g_PortmapProg.Add(pRPCProg);
        SERVER_TCP.push_back(tcp);
        SERVER_UDP.push_back(udp);
    } else {
        printf("[NFSD] %s start failed.\n", pRPCProg->GetName());
    }
}

static void printAbout(void) {
    printf("[NFSD] Network File System server\n");
    printf("[NFSD] Copyright (C) 2005 Ming-Yang Kao\n");
    printf("[NFSD] Edited in 2011 by ZeWaren\n");
    printf("[NFSD] Edited in 2013 by Alexander Schneider (Jankowfsky AG)\n");
    printf("[NFSD] Edited in 2014 2015 by Yann Schepens\n");
    printf("[NFSD] Edited in 2016 by Peter Philipp (Cando Image GmbH), Marc Harding\n");
    printf("[NFSD] Mostly rewritten in 2019-2021 by Simon Schubiger for Previous NeXT emulator\n");
}

extern "C" int nfsd_read(const char* path, size_t fileOffset, void* dst, size_t count) {
    if(nfsd_fts[0]) {
        VFSFile file(*nfsd_fts[0], path, "rb");
        if(file.isOpen())
            return file.read(fileOffset, dst, count);
    }
    return -1;
}

static bool getUID_GID(const char* userName, int* uid, int* gid) {
    // try to get uid/gid of user "me" from /etc/passwd and use it as the NFS default user
    size_t buffer_size = 1024*1024;
    char* buffer = (char*)malloc(buffer_size);
    int count = nfsd_read("/etc/passwd", 0, buffer, buffer_size);
    if(count > 0) {
        buffer[count] = '\0';
        char* line = strtok(buffer, "\n");
        while(line) {
            char user[256];
            char passwd[1024];
            if(sscanf(line, "%[^:]::%d:%d", user, uid, gid) < 3)
                sscanf(line, "%[^:]:%[^:]:%d:%d", user, passwd, uid, gid);
            if(strcmp(userName, user) == 0)
                return true;
            line  = strtok(NULL, "\n");
        }
    }
    free(buffer);
    return false;
}

extern "C" void nfsd_start(void) {
    if(access(ConfigureParams.Ethernet.szNFSroot, F_OK | R_OK | W_OK) < 0) {
        printf("[NFSD] can not access directory '%s'. nfsd startup canceled.\n", ConfigureParams.Ethernet.szNFSroot);
        delete nfsd_fts[0];
        nfsd_fts[0] = NULL;
        return;
    }
    
    if(nfsd_fts[0]) {
        if(nfsd_fts[0]->getBasePath() != HostPath(ConfigureParams.Ethernet.szNFSroot)) {
            VFSPath basePath = nfsd_fts[0]->getBasePathAlias();
            delete nfsd_fts[0];
            nfsd_fts[0] = new FileTableNFSD(ConfigureParams.Ethernet.szNFSroot, basePath);
        }
    } else {
        nfsd_fts[0] = new FileTableNFSD(ConfigureParams.Ethernet.szNFSroot, "/");
    }
    if(initialized) return;

    char nfsd_hostname[_SC_HOST_NAME_MAX];
    gethostname(nfsd_hostname, sizeof(nfsd_hostname));
    
    printf("[NFSD] starting local NFS daemon on '%s', exporting '%s'\n", nfsd_hostname, ConfigureParams.Ethernet.szNFSroot);
    printAbout();
    
    static CNFSProg       NFSProg;
    static CMountProg     MountProg;
    static CBootparamProg BootparamProg;

    int uid;
    int gid;
    if(getUID_GID("me", &uid, &gid))
       NFSProg.SetUserID(uid, gid);
    
    g_RPCServer.SetLogOn(g_bLogOn);

    add_program(&g_PortmapProg, PORT_PORTMAP);
    add_program(&NFSProg,       PORT_NFS);
    add_program(&MountProg);
    add_program(&BootparamProg);
    
    static VDNS vdns;
    
    initialized = true;
}

extern "C" int nfsd_match_addr(uint32_t addr) {
    return (addr == (ntohl(special_addr.s_addr) | CTL_NFSD)) ||
           (addr == (ntohl(special_addr.s_addr) | ~(uint32_t)CTL_NET_MASK)) ||
           (addr == (ntohl(special_addr.s_addr) | ~(uint32_t)CTL_CLASS_MASK(CTL_NET))); // NS kernel seems to broadcast on 10.255.255.255
}
