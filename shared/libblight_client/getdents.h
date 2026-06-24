#include <sys/types.h>

struct __dirstream;
typedef struct __dirstream DIR;
struct dirent;
struct dirent64;

extern "C" {
__attribute__((visibility("default"))) int
getdents(unsigned int fd, struct dirent* dirp, unsigned int count);
__attribute__((visibility("default"))) ssize_t
getdents64(int fd, void* dirp, size_t count);
__attribute__((visibility("default"))) struct dirent*
readdir(DIR* dirp);
__attribute__((visibility("default"))) struct dirent64*
readdir64(DIR* dirp);
}
