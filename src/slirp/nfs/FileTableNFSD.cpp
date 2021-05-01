//
//  FileTableNFSD.cpp
//  Previous
//
//  Created by Simon Schubiger on 04.03.19.
//

#include "FileTableNFSD.h"
#include "devices.h"
#include "compat.h"
#include <unistd.h>

using namespace std;

FileTableNFSD::FileTableNFSD(const HostPath& basePath, const VFSPath& basePathAlias) : VirtualFS(basePath, basePathAlias), mutex(host_mutex_create()) {
    for(int d = 0; DEVICES[d][0]; d++) {
        const char* perm  = DEVICES[d][0];
        uint32_t    major = atoi(DEVICES[d][1]);
        uint32_t    minor = atoi(DEVICES[d][2]);
        const char* name  = DEVICES[d][3];
        uint32_t    rdev  = major << 8 | minor;
        if(     perm[0] == 'b') blockDevices[name]     = rdev;
        else if(perm[0] == 'c') characterDevices[name] = rdev;
    }
}

FileTableNFSD::~FileTableNFSD(void) {
    host_mutex_destroy(mutex);
}

bool FileTableNFSD::isBlockDevice(const string& fname) {
    return blockDevices.find(fname) != blockDevices.end();
}

bool FileTableNFSD::isCharDevice(const string& fname) {
    return characterDevices.find(fname) != characterDevices.end();
}

bool FileTableNFSD::isDevice(const VFSPath& absoluteVFSpath, string& fname) {
    auto  directory(absoluteVFSpath.parent_path().string());
    fname          = absoluteVFSpath.filename();
    
    const size_t len = directory.size();
    return
    len >= 4 &&
    directory[len-4] == '/' &&
    directory[len-3] == 'd' &&
    directory[len-2] == 'e' &&
    directory[len-1] == 'v' &&
    (isBlockDevice(fname) || isCharDevice(fname));
}

int FileTableNFSD::stat(const VFSPath& absoluteVFSpath, struct stat& fstat) {
    NFSDLock lock(mutex);
    
    int result = VirtualFS::stat(absoluteVFSpath, fstat);
    
    string fname;
    if(fstat.st_rdev == (unsigned long) FATTR_INVALID) {
        if(isDevice(absoluteVFSpath, fname)) {
            map<string,uint32_t>::iterator iter = blockDevices.find(fname);
            if(iter != blockDevices.end()) {
                fstat.st_mode &= ~S_IFMT;
                fstat.st_mode |= S_IFBLK;
                fstat.st_rdev = iter->second;
            }
            iter = characterDevices.find(fname);
            if(iter != characterDevices.end()) {
                fstat.st_mode &= ~S_IFMT;
                fstat.st_mode |= S_IFCHR;
                fstat.st_rdev = iter->second;
            }
        }
    }
    
    return result;
}
bool FileTableNFSD::getCanonicalPath(uint64_t fhandle, std::string& result) {
    NFSDLock lock(mutex);
    auto iter(handle2path.find(fhandle));
    if(iter != handle2path.end()) {
        result = iter->second;
        return true;
    }
    return false;
}
void FileTableNFSD::move(const VFSPath& absoluteVFSpathFrom, const VFSPath& absoluteVFSpathTo) {
    NFSDLock lock(mutex);
    return VirtualFS::move(absoluteVFSpathFrom, absoluteVFSpathTo);
}
void  FileTableNFSD::remove(const VFSPath& absoluteVFSpath) {
    NFSDLock lock(mutex);
    return VirtualFS::remove(absoluteVFSpath);
}
uint64_t FileTableNFSD::getFileHandle(const VFSPath& absoluteVFSpath) {
    NFSDLock lock(mutex);
    auto result(VirtualFS::getFileHandle(absoluteVFSpath));
    handle2path[result] = absoluteVFSpath.canonicalize().string();
    return result;
}
void FileTableNFSD::setFileAttrs(const VFSPath& absoluteVFSpath, const FileAttrs& fstat) {
    NFSDLock lock(mutex);
    return VirtualFS::setFileAttrs(absoluteVFSpath, fstat);
}

FileAttrs FileTableNFSD::getFileAttrs(const VFSPath& absoluteVFSpath) {
    NFSDLock lock(mutex);
    return VirtualFS::getFileAttrs(absoluteVFSpath);
}
