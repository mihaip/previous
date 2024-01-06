#include <emscripten.h>

#include "file_js.h"

FILEJS *FileJS_Open(const char *path, const char *mode) {
    int disk_id = EM_ASM_INT({
        return workerApi.disks.open(UTF8ToString($0));
    }, path + 1); // Skip over leading slash
    if (disk_id == -1) {
        return NULL;
    }

    FILEJS *fp = (FILEJS *)malloc(sizeof(FILEJS));
    fp->disk_id = disk_id;
    return fp;
}

FILEJS *FileJS_Close(FILEJS *fp) {
    EM_ASM_({ workerApi.disks.close($0); }, fp->disk_id);
    free(fp);
    return NULL;
}

off_t FileJS_Length(const char *path) {
    return EM_ASM_INT({
        return workerApi.disks.size(UTF8ToString($0));
    }, path + 1); // Skip over leading slash
}

bool FileJS_Write(uint8_t *data, uint32_t size, uint64_t offset, FILEJS *fp) {
    int write_size = EM_ASM_INT({
        return workerApi.disks.write($0, $1, $2, $3);
    }, fp->disk_id, data, (int)offset, (int)size);
    return write_size == size;
}

bool FileJS_Read(uint8_t *data, uint32_t size, uint64_t offset, FILEJS *fp) {
    int read_size = EM_ASM_INT({
        return workerApi.disks.read($0, $1, $2, $3);
    }, fp->disk_id, data, (int)offset, (int)size);
    return read_size == size;
}
