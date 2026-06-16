struct _IO_FILE;
typedef struct _IO_FILE FILE;

extern "C" {
// Don't define the function for this
FILE*
fdopen(int fd, const char* mode) noexcept;

__attribute__((visibility("default"))) FILE*
fopen(const char* path, const char* mode);

__attribute__((visibility("default"))) FILE*
fopen64(const char* path, const char* mode);
}
