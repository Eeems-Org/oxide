struct _IO_FILE;
typedef struct _IO_FILE FILE;

extern "C" {
__attribute__((visibility("default"))) FILE*
fopen(const char* path, const char* mode);

__attribute__((visibility("default"))) FILE*
fopen64(const char* path, const char* mode);
}
