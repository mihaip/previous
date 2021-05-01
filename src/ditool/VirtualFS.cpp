#include <string>
#include <sstream>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>

#include "VirtualFS.h"
#include "compat.h"

using namespace std;

#ifdef _XDRSTREAM_H_
FileAttrs::FileAttrs(XDRInput* xin) : rdev(INVALID) {
    xin->Read(&mode);
    xin->Read(&uid);
    xin->Read(&gid);
    xin->Read(&size);
    xin->Read(&atime_sec);
    xin->Read(&atime_usec);
    xin->Read(&mtime_sec);
    xin->Read(&mtime_usec);
    
}
#endif

FileAttrs::FileAttrs(const struct stat& stat) :
mode      (stat.st_mode),
uid       (stat.st_uid),
gid       (stat.st_gid),
size      (static_cast<uint32_t>(stat.st_size)),
atime_sec (static_cast<uint32_t>(stat.st_atimespec.tv_sec)),
atime_usec(static_cast<uint32_t>(stat.st_atimespec.tv_nsec / 1000)),
mtime_sec (static_cast<uint32_t>(stat.st_mtimespec.tv_sec)),
mtime_usec(static_cast<uint32_t>(stat.st_mtimespec.tv_nsec / 1000)),
rdev      (stat.st_rdev)
{}

FileAttrs::FileAttrs(const FileAttrs& attrs) :
mode      (FATTR_INVALID),
uid       (FATTR_INVALID),
gid       (FATTR_INVALID),
size      (FATTR_INVALID),
atime_sec (FATTR_INVALID),
atime_usec(FATTR_INVALID),
mtime_sec (FATTR_INVALID),
mtime_usec(FATTR_INVALID),
rdev      (FATTR_INVALID)
{update(attrs);}

void FileAttrs::update(const FileAttrs& attrs) {
#define UPDATE_ATTR(a) a = valid(attrs. a) ? attrs. a : a
    UPDATE_ATTR(mode);
    UPDATE_ATTR(uid);
    UPDATE_ATTR(gid);
    UPDATE_ATTR(size);
    UPDATE_ATTR(atime_sec);
    UPDATE_ATTR(atime_usec);
    UPDATE_ATTR(mtime_sec);
    UPDATE_ATTR(mtime_usec);
    UPDATE_ATTR(rdev);
}

//----- VFS path

PathCommon::PathCommon(const std::string& sep, const std::string& path)
: vector(split(sep, path))
, sep(sep)
, path(path)
{}

PathCommon::PathCommon(const std::string& sep, const char* path)
: vector(split(sep, path))
, sep(sep)
, path(path)
{}

std::string PathCommon::to_string() const {
    std::string newPath(empty() ? sep : "");
    for(const auto& comp : *this) {
        newPath += sep;
        newPath += comp;
    }
    return is_absolute() ? newPath : newPath.substr(1);
}

vector<string> PathCommon::split(const std::string& sep, const std::string& path) {
    vector<std::string> result;
    std::string relPath = sep == "/"
    ? path.substr(!(path.empty())    && path[0] == '/' ? 1 : 0)
    : path.substr(!(path.size() > 1) && path[1] == ':' ? 2 : 0);
        
    stringstream ss(relPath);
    std::string comp;

    while (getline (ss, comp, sep[0]))
        result.push_back (comp);
    
    return result;
}

void PathCommon::append(const PathCommon& path) {
    for(const auto& comp : path)
        push_back(comp);
    this->path = to_string();
}

bool PathCommon::exists() {
    struct stat buffer;
    return (stat (c_str(), &buffer) == 0);
}

ostream& operator<<(ostream& os, const PathCommon& path) {
    os << path.string();
    return os;
}

VFSPath VFSPath::canonicalize(void) const {
    unique_ptr<char[]> buffer(new char[length()+1]);
    memcpy(buffer.get(), c_str(), length()+1);
    
    char* vfsPath = buffer.get();
    char* slashslashptr;
    char* dotdotptr;
    char* slashdotptr;
    char* slashptr;

    /* step 1 : replace '//' sequences by a single '/' */
    slashslashptr = vfsPath;
    while(1) {
        slashslashptr = strstr(slashslashptr,"//");
        if(NULL == slashslashptr)
            break;
        /* remove extra '/' */
        memmove(slashslashptr,slashslashptr+1,strlen(slashslashptr+1)+1);
    }

    /* step 2 : replace '/./' sequences by a single '/' */
    slashdotptr = vfsPath;
    while(1) {
        slashdotptr = strstr(slashdotptr,"/./");
        if(NULL == slashdotptr)
            break;
        /* strip the extra '/.' */
        memmove(slashdotptr,slashdotptr+2,strlen(slashdotptr+2)+1);
    }

    /* step 3 : replace '/<name>/../' sequences by a single '/' */

    while(1) {
        dotdotptr = strstr(vfsPath,"/../");
        if(NULL == dotdotptr)
            break;
        if(dotdotptr == vfsPath) {
            /* special case : '/../' at the beginning of the path are replaced
               by a single '/' */
            memmove(vfsPath, vfsPath+3,strlen(vfsPath+3)+1);
            continue;
        }

        /* null-terminate the string before the '/../', so that strrchr will
           start looking right before it */
        *dotdotptr = '\0';
        slashptr = strrchr(vfsPath,'/');
        if(NULL == slashptr) {
            /* this happens if this function was called with a relative path.
               don't do that.  */
            assert("can't find leading '/' before '/../ sequence\n");
            break;
        }
        memmove(slashptr,dotdotptr+3,strlen(dotdotptr+3)+1);
    }

    /* step 4 : remove a trailing '/..' */

    dotdotptr = strstr(vfsPath,"/..");
    if(dotdotptr == vfsPath)
        /* if the full path is simply '/..', replace it by '/' */
        vfsPath[1] = '\0';
    else if(NULL != dotdotptr && '\0' == dotdotptr[3]) {
        *dotdotptr = '\0';
        slashptr = strrchr(vfsPath,'/');
        if(NULL != slashptr) {
            /* make sure the last slash isn't the root */
            if (slashptr == vfsPath)
                vfsPath[1] = '\0';
            else
                *slashptr = '\0';
        }
    }

    /* step 5 : remove a traling '/.' */

    slashdotptr = strstr(vfsPath,"/.");
    if (slashdotptr != NULL && slashdotptr[2] == '\0') {
        if(slashdotptr == vfsPath)
            // if the full path is simply '/.', replace it by '/' */
            vfsPath[1] = '\0';
        else
            *slashdotptr = '\0';
    }
    
    VFSPath result(vfsPath);
    assert(result.string().find("/./") == string::npos);
    assert(result.string().find("/../") == string::npos);
    return result;
}

string VFSPath::filename() const {
    return empty() ? std::string() : at(size()-1);
}

VFSPath VFSPath::parent_path() const {
    if(empty()) return *this;
    auto result(*this);
    result.pop_back();
    return VFSPath(result);
}

bool VFSPath::is_absolute() const {
    return !(path.empty()) && path[0] == '/';
}

VFSPath& VFSPath::operator /= (const VFSPath& path) {
    append(path);
    return *this;
}

VFSPath VFSPath::operator / (const VFSPath& path) const {
    auto result(*this);
    result /= path;
    return result;
}

VFSPath VFSPath::relative(const VFSPath& path, const VFSPath& basePath) {
    return VFSPath(path.string().find(basePath.string(), 0) == 0 ? path.string().substr(basePath.length()) : path);
}

bool HostPath::is_absolute() const {
#ifdef _WIN32
    return !(path.size() > 1) && path[1] == ':' && ::toupper(paht[0]) >= 'A' && && ::toupper(paht[0]) <= 'Z' ;
#else
    return !(path.empty()) && path[0] == '/';
#endif
}

HostPath& HostPath::operator /= (const HostPath& path) {
    append(path);
    return *this;
}

HostPath HostPath::operator / (const HostPath& path) const {
    auto result(*this);
    result /= path;
    return result;
}

//----- file attributes

FileAttrs::FileAttrs(const string& serialized) {
    sscanf(serialized.c_str(), "0%o:%d:%d:%d", &mode, &uid, &gid, &rdev);
}

string FileAttrs::serialize() const {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "0%o:%d:%d:%d", mode, uid, gid, rdev);
    return string(buffer);
}

bool FileAttrs::valid(uint32_t statval) {return statval != 0xFFFFFFFF;}

//----- VFS

VirtualFS::VirtualFS(const HostPath& basePath, const VFSPath& basePathAlias)
: basePathAlias(basePathAlias)
, basePath(basePath)
{
}

VirtualFS::~VirtualFS() {
}

HostPath VirtualFS::getBasePath() {return basePath;}

VFSPath VirtualFS::getBasePathAlias() {return basePathAlias;}

VFSPath VirtualFS::removeAlias(const VFSPath& absoluteVFSpath) {
    VFSPath result("/");
    result /=  VFSPath::relative(absoluteVFSpath, basePathAlias);
    assert(result.is_absolute());
    return result;
}

int VirtualFS::stat(const VFSPath& absoluteVFSpath, struct stat& fstat) {
    auto path   = removeAlias(absoluteVFSpath);
    auto result = vfsStat(path, fstat);
    auto attrs  = getFileAttrs(path);
    
    if(FileAttrs::valid(attrs.mode)) {
        uint32_t mode = fstat.st_mode; // copy format & permissions from actual file in the file system
        mode &= ~(S_IWUSR  | S_IRUSR | S_ISVTX);
        mode |= attrs.mode & (S_IWUSR | S_IRUSR | S_ISVTX); // copy user R/W permissions and directory restrcted delete from attributes
        if(S_ISREG(fstat.st_mode) && fstat.st_size == 0 && (attrs.mode & S_IFMT)) {
            // mode heursitics: if file is empty we map it to the various special formats (CHAR, BLOCK, FIFO, etc.) from stored attributes
            mode &= ~S_IFMT;                // clear format
            mode |= (attrs.mode & S_IFMT); // copy format from attributes
        }
        fstat.st_mode = mode;
    }
    fstat.st_uid  = FileAttrs::valid(attrs.uid)  ? attrs.uid  : fstat.st_uid;
    fstat.st_gid  = FileAttrs::valid(attrs.gid)  ? attrs.gid  : fstat.st_gid;
    fstat.st_rdev = FileAttrs::valid(attrs.rdev) ? attrs.rdev : fstat.st_rdev;
    
    return result;
}

static uint64_t rotl(uint64_t x, uint64_t n) {return (x<<n) | (x>>(64LL-n));}

int VirtualFS::remove(const char* fpath, const struct stat* /*sb*/, int /*typeflag*/, struct FTW* /*ftwbuf*/) {
    fchmodat(AT_FDCWD, fpath, ACCESSPERMS, AT_SYMLINK_NOFOLLOW);
    ::remove(fpath);
    return 0;
}

static uint64_t make_file_handle(const struct stat& fstat) {
    uint64_t result = fstat.st_dev;
    result = rotl(result, 32) ^ fstat.st_ino;
    if(result == 0) result = ~result;
    return result;
}

uint64_t VirtualFS::getFileHandle(const VFSPath& absoluteVFSPath) {
    auto path = removeAlias(absoluteVFSPath);
    
    uint64_t result(0);
    struct stat fstat;
    if(vfsStat(path, fstat) == 0) {
        result = make_file_handle(fstat);
    } else {
        printf("No file handle for %s\n", absoluteVFSPath.c_str());
    }
    return result;
}

void VirtualFS::move(const VFSPath& pathFrom, const VFSPath&) {
    remove(pathFrom);
}

void VirtualFS::remove(const VFSPath& /*absoluteVFSPath*/) {
}

const string NFSD_ATTRS(".nfsd_fattrs");

void VirtualFS::setFileAttrs(const VFSPath& absoluteVFSpath, const FileAttrs& fstat) {
    auto path   = removeAlias(absoluteVFSpath);
    auto fname(absoluteVFSpath.filename());

    assert("." != fname && ".." != fname);
            
    auto serialized = fstat.serialize();
    auto hostPath   = toHostPath(absoluteVFSpath);
    if(::setxattr(hostPath.c_str(), NFSD_ATTRS.c_str(), serialized.c_str(), serialized.length(), 0, XATTR_NOFOLLOW) != 0)
        printf("setxattr(%s) failed\n", hostPath.c_str());
}

FileAttrs VirtualFS::getFileAttrs(const VFSPath& absoluteVFSpath) {
    char buffer[128];
    auto hostPath = toHostPath(absoluteVFSpath);
    if(::getxattr(hostPath.c_str(), NFSD_ATTRS.c_str(), buffer, sizeof(buffer), 0, XATTR_NOFOLLOW) > 0)
        return FileAttrs(buffer);
    else {
        struct stat fstat;
        ::lstat(hostPath.c_str(), &fstat);
        return FileAttrs(fstat);
    }
}

uint32_t VirtualFS::fileId(uint64_t ino) {
    uint32_t result = static_cast<uint32_t>(ino);
    return (result ^ (ino >> 32LL)) & 0x7FFFFFFF;
}

void VirtualFS::touch(const VFSPath& absoluteVFSpath) {
    VFSFile file(*this, absoluteVFSpath, "wb");
}

//----- file io

VFSFile::VFSFile(VirtualFS& ft, const VFSPath& path, const std::string& mode)
: ft(ft)
, path(path)
, restoreStat(false)
, file(::fopen(ft.toHostPath(path).c_str(), mode.c_str())) {
    if(!(file) && errno == EACCES) {
        restoreStat = true;
        ft.vfsStat(path, fstat);
        ft.vfsChmod(path, fstat.st_mode);
        file = ::fopen(ft.toHostPath(path).c_str(), mode.c_str());
    }
}

VFSFile::~VFSFile(void) {
    if(restoreStat) {
        ft.vfsChmod(path, fstat.st_mode);
        struct timeval times[2];
        times[0].tv_sec  = fstat.st_atimespec.tv_sec;
        times[0].tv_usec = static_cast<int32_t>(fstat.st_atimespec.tv_nsec / 1000);
        times[1].tv_sec  = fstat.st_mtimespec.tv_sec;
        times[1].tv_usec = static_cast<int32_t>(fstat.st_mtimespec.tv_nsec / 1000);
        ft.vfsUtimes(path, times);
    }
    if(file) fclose(file);
}

size_t VFSFile::read(size_t fileOffset, void* dst, size_t count) {
    ::fseek(file, fileOffset, SEEK_SET);
    return ::fread(dst, sizeof(uint8_t), count, file);
}

size_t VFSFile::write(size_t fileOffset, void* src, size_t count) {
    ::fseek(file, fileOffset, SEEK_SET);
    return ::fwrite(src, sizeof(uint8_t), count, file);
}

bool VFSFile::isOpen(void) {
    return file != NULL;
}

HostPath VirtualFS::toHostPath(const VFSPath& absoluteVFSpath) {
    auto path(removeAlias(absoluteVFSpath));
    
    assert(path.is_absolute());
    
    HostPath result(basePath);
    result.append(path.canonicalize());
    return result;
}

static int get_error(int result) {
    return result < 0 ? errno : result;
}

int VirtualFS::vfsChmod(const VFSPath& absoluteVFSpath, mode_t mode) {
    return get_error(::fchmodat(AT_FDCWD, toHostPath(absoluteVFSpath).c_str(), mode | S_IWUSR  | S_IRUSR, AT_SYMLINK_NOFOLLOW));
}

int VirtualFS::vfsAccess(const VFSPath& absoluteVFSpath, int mode) {
    return get_error(::access(toHostPath(absoluteVFSpath).c_str(), mode));
}

DIR* VirtualFS::vfsOpendir(const VFSPath& absoluteVFSpath) {
    return ::opendir(toHostPath(absoluteVFSpath).c_str());
}

int VirtualFS::vfsRemove(const VFSPath& absoluteVFSpath) {
    return get_error(::remove(toHostPath(absoluteVFSpath).c_str()));
}

int VirtualFS::vfsRename(const VFSPath& absoluteVFSpathFrom, const VFSPath& absoluteVFSpathTo) {
    return get_error(::rename(toHostPath(absoluteVFSpathFrom).c_str(), toHostPath(absoluteVFSpathTo).c_str()));
}

int VirtualFS::vfsReadlink(const VFSPath& absoluteVFSpath, VFSPath& result) {
    auto path = toHostPath(absoluteVFSpath);
    
    struct stat sb;
    ssize_t nbytes, bufsiz;
    
    if (lstat(path.c_str(), &sb) == -1)
        return errno;
    
    /* Add one to the link size, so that we can determine whether
     the buffer returned by readlink() was truncated. */
    
    bufsiz = sb.st_size + 1;
    
    /* Some magic symlinks under (for example) /proc and /sys
     report 'st_size' as zero. In that case, take PATH_MAX as
     a "good enough" estimate. */
    
    if (sb.st_size == 0)
        bufsiz = MAXPATHLEN;
    
    char buf[bufsiz];
    
    nbytes = ::readlink(path.c_str(), buf, bufsiz);
    if (nbytes == -1)
        return errno;
    
    buf[nbytes] = '\0';
    result = string(buf);
    
    return 0;
}

int VirtualFS::vfsLink(const VFSPath& absoluteVFSpathFrom, const VFSPath& absoluteVFSpathTo, bool soft) {
    std::string from = soft ? absoluteVFSpathFrom.string() : toHostPath(absoluteVFSpathFrom).string();
    std::string to   = toHostPath(absoluteVFSpathTo).string();
    
    if(soft) return get_error(::symlink(from.c_str(), to.c_str()));
    else     return get_error(::link   (from.c_str(), to.c_str()));
}

int VirtualFS::vfsMkdir(const VFSPath& absoluteVFSpath, mode_t mode) {
    return get_error(::mkdir(toHostPath(absoluteVFSpath).c_str(), mode));
}

int VirtualFS::vfsNftw(const VFSPath& absoluteVFSpath, int (*fn)(const char *, const struct stat *ptr, int flag, struct FTW *), int depth, int flags) {
    return get_error(::nftw(toHostPath(absoluteVFSpath).c_str(), fn, depth, flags));
}

int VirtualFS::vfsStatvfs(const VFSPath& absoluteVFSpath, struct statvfs& fsstat) {
    return get_error(::statvfs(toHostPath(absoluteVFSpath).c_str(), &fsstat));
}

int VirtualFS::vfsStat(const VFSPath& absoluteVFSpath, struct stat& fstat) {
    return get_error(::lstat(toHostPath(absoluteVFSpath).c_str(), &fstat));
}

int VirtualFS::vfsUtimes(const VFSPath& absoluteVFSpath, const struct timeval times[2]) {
    return get_error(::lutimes(toHostPath(absoluteVFSpath).c_str(), times));
}
