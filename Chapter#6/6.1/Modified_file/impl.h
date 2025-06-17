#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/xattr.h>

int set_xattr(const char *path, const char *name, const char *value){
    if(setxattr(path, name, value, strlen(value), 0) != -1)
        return 1;
    return -1;
};

// Function to get an extended attribute
// If name does not exist, return NULL
// If name exists, return the value of the attribute
char* get_xattr(const char *path, const char *name){
    char *dst = malloc(256);
    if (!dst)
        return NULL;
    if (getxattr(path, name, dst, 256) == -1) {
        free(dst);
        return NULL;
    }
    return dst;
};

// Function to remove an extended attribute
int remove_xattr(const char *path, const char *name){
    if(removexattr(path, name) != -1)
        return 1;
    return -1;
};

void get_inode_info(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat failed");
        return;
    }
    printf("Inode info for %s:\n", path);
    printf("  inode number: %lu\n", st.st_ino);
    printf("  file size: %ld bytes\n", st.st_size);
    printf("  mode: %o\n", st.st_mode & 0777);
    printf("  hard links: %ld\n", st.st_nlink);
    printf("  uid: %d\n", st.st_uid);
    printf("  gid: %d\n", st.st_gid);
    printf("  atime: %ld\n", st.st_atime);
    printf("  mtime: %ld\n", st.st_mtime);
    printf("  ctime: %ld\n", st.st_ctime);
}

void list_xattrs(const char *path) {
    char *xattrs = malloc(1024);
    ssize_t size = llistxattr(path, xattrs, 1024);
    if (size == -1) {
        perror("llistxattr failed");
        free(xattrs);
        return;
    }
    if (size == 0)
        printf("No extended attributes found for %s\n", path);
    else {
        char *p = xattrs;
        while (p < xattrs + size) {
            printf("  %s\n", p);
            p += strlen(p) + 1;
        }
    }
}