#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// Override some File_* functions with ones that read disk files from the JS
// side. file_js.h is meant to be #included after file.h, so it can override
// the functions via the C preprocessor.
typedef struct {
    int disk_id;
} FILEJS;

extern FILEJS *FileJS_Open(const char *path, const char *mode);
extern FILEJS *FileJS_Close(FILEJS *fp);
extern off_t FileJS_Length(const char *path);
extern bool FileJS_Write(uint8_t *data, uint32_t size, uint64_t offset, FILEJS *fp);
extern bool FileJS_Read(uint8_t *data, uint32_t size, uint64_t offset, FILEJS *fp);

#define FILE FILEJS
#define File_Open FileJS_Open
#define File_Close FileJS_Close
#define File_Length FileJS_Length
#define File_Write FileJS_Write
#define File_Read FileJS_Read
