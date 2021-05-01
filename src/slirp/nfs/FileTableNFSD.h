//
//  FileTableNFSD.hpp
//  Previous
//
//  Created by Simon Schubiger on 04.03.19.
//

#ifndef FileTableNFSD_hpp
#define FileTableNFSD_hpp

#include "../../ditool/VirtualFS.h"
#include "XDRStream.h"
#include "host.h"

class FileTableNFSD : public VirtualFS {
    mutex_t*                        mutex;
    
    std::map<std::string, uint32_t> blockDevices;
    std::map<std::string, uint32_t> characterDevices;
    std::map<uint64_t, std::string> handle2path;
    
    bool        isBlockDevice(const std::string& fname);
    bool        isCharDevice (const std::string& fname);
    bool        isDevice     (const VFSPath& absoluteVFSpath, std::string& fname);
public:
    FileTableNFSD(const HostPath& basePath, const VFSPath& basePathAlias);
    virtual ~FileTableNFSD(void);
    
    int         stat            (const VFSPath& absoluteVFSpath, struct stat& stat) override;
    void        move            (const VFSPath& absoluteVFSpathFrom, const VFSPath& absoluteVFSpathTo) override;
    void        remove          (const VFSPath& absoluteVFSpath) override;
    uint64_t    getFileHandle   (const VFSPath& absoluteVFSpath) override;
    void        setFileAttrs    (const VFSPath& absoluteVFSpath, const FileAttrs& fstat) override;
    FileAttrs   getFileAttrs    (const VFSPath& absoluteVFSpath) override;
    
    bool        getCanonicalPath(uint64_t handle, std::string& result);
};

#endif /* FileTableNFSD_hpp */
