#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#define MAX_SZ 256
#define MAX_LEN 4096

struct session {
    char input[MAX_LEN];
    char output[MAX_LEN];
    bool input_ready; // For null inputs
    pthread_mutex_t lock;
};

struct session sessions[MAX_SZ];

// path must be /{%id}/{input,output}
// {%id} is the return value, {input,output} is in variable file
static int get_session_id(const char *path, char *file) {
    if (path[0] != '/' || strlen(path) < 2) 
        return -ENOENT;
    char *cut = strchr(path + 1, '/');
    int id = atoi(path + 1);
    if (id < 0 || id >= MAX_SZ)
        return -ENOENT;
    if (file) {
        if (cut)
            strcpy(file, cut + 1);
        else
            file[0] = '\0';
    }
    return id;
}

void gpt_reply(const char *prompt, char *reply) {
    sprintf(reply, "Q: %sA: Yes!\n", prompt);
}

static int gptfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    char file[16];
    get_session_id(path, file);
    if (file[0] == '\0') {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(file, "input") == 0 || strcmp(file, "output") == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = MAX_LEN;
    } else return -ENOENT;
    return 0;
}

static int gptfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off,
                         struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    if (strcmp(path, "/") == 0) {
        for (int i = 0; i < MAX_SZ; i++) {
            if (sessions[i].input_ready || sessions[i].output[0] != '\0') {
                char name[16];
                sprintf(name, "session%d", i);
                filler(buf, name, NULL, 0, 0);
            }
        }
        return 0;
    }
    get_session_id(path, NULL); // check if id in bound
    filler(buf, "input", NULL, 0, 0);
    filler(buf, "output", NULL, 0, 0);
    return 0;
}

static int gptfs_mkdir(const char *path, mode_t mode) {
    int id = get_session_id(path, NULL); // check if id in bound
    sessions[id].input[0] = '\0';
    sessions[id].output[0] = '\0';
    sessions[id].input_ready = false;
    pthread_mutex_init(&sessions[id].lock, NULL);
    return 0;
}

static int gptfs_open(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int gptfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    char file[16];
    int id = get_session_id(path, file);
    pthread_mutex_lock(&sessions[id].lock);
    const char *src = NULL;
    if (strcmp(file, "input") == 0) 
        src = sessions[id].input;
    else if (strcmp(file, "output") == 0) 
        src = sessions[id].output;
    else return -ENOENT;
    size_t len = strlen(src);
    if (offset < len) {
        if (offset + size > len) size = len - offset;
        memcpy(buf, src + offset, size);
    } else size = 0;
    pthread_mutex_unlock(&sessions[id].lock);
    return size;
}

static int gptfs_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    char file[16];
    int id = get_session_id(path, file);
    if (strcmp(file, "input") != 0)
        return -ENOENT;
    pthread_mutex_lock(&sessions[id].lock);
    size_t len = size > MAX_LEN - 1 ? MAX_LEN - 1 : size;
    memcpy(sessions[id].input, buf, len);
    sessions[id].input[len] = 0;
    gpt_reply(sessions[id].input, sessions[id].output);
    sessions[id].input_ready = 1;
    pthread_mutex_unlock(&sessions[id].lock);
    return size;
}

static const struct fuse_operations gptfs_oper = {
    .getattr = gptfs_getattr,
    .readdir = gptfs_readdir,
    .mkdir = gptfs_mkdir,
    .open = gptfs_open,
    .read = gptfs_read,
    .write = gptfs_write,
};

int main(int argc, char *argv[]) {
    memset(sessions, 0, sizeof(sessions));
    return fuse_main(argc, argv, &gptfs_oper, NULL);
}