#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <utime.h>

char* concat_paths_unsafe(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    char *result = calloc(len1 + len2 + 2, 1);
    memcpy(result, s1, len1);
    result[len1] = '/';
    memcpy(result + len1 + 1, s2, len2);
    return result;
}

int upd_file(const char* src, const char* dst) {
    char buf[4096];
    int ret = 0;

    struct stat src_st;
    struct stat dst_st;
    int src_stat_ret = stat(src, &src_st);
    int dst_stat_ret = stat(dst, &dst_st);
    if (src_stat_ret != 0) { ret = 1; goto exit; }
    if (dst_stat_ret == 0) {
        if (src_st.st_mtime <= dst_st.st_mtime) {
            chmod(dst, src_st.st_mode);
            /* No need for further processing */
            printf("skipped: %s\n", src);
            return 0;
        }
    }

    int fd_src = open(src, O_RDONLY);
    int fd_dst = open(dst, O_WRONLY | O_CREAT, src_st.st_mode);
    if (fd_src < 0 || fd_dst < 0) { ret = 1; goto exit; }

    struct utimbuf utimbuf = {.actime=src_st.st_atime, .modtime=src_st.st_mtime};
    utime(dst, &utimbuf);

    int remaining_bytes;
    while (remaining_bytes = read(fd_src, buf, 4096), remaining_bytes > 0) {
        int written_bytes = 0;
        while (remaining_bytes > 0) {
            int written_current = write(fd_dst, buf + written_bytes, remaining_bytes);
            if (written_current < 0 && errno != EINTR) { ret = 1; goto exit; }
            if (written_current < 0) { continue; }
            written_bytes += written_current;
            remaining_bytes -= written_current;
        }
    }

    exit: {
        if (errno != 0 && ret != 0) {
            printf("%s: error occured: %s\n", __func__, strerror(errno));
        }
        close(fd_src);
        close(fd_dst);
        return ret;
    }
}

int rm_recursively(const char* path) {
    int ret = 0;
    DIR* dir = opendir(path);
    if (dir == NULL) { ret = 1; goto exit; }

    struct dirent* dirent;
    while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, ".") == 0) { continue; }
        if (strcmp(dirent->d_name, "..") == 0) { continue; }
        char* entry_path = concat_paths_unsafe(path, dirent->d_name);

        struct stat entry_st;
        int src_stat_ret = stat(entry_path, &entry_st);
        if (src_stat_ret == 0 && !S_ISDIR(entry_st.st_mode) && !S_ISREG(entry_st.st_mode)) {
            printf("non-regular or non-directory entry %s encountered, unsupported\n", entry_path);
            continue;
        }

        if (S_ISDIR(entry_st.st_mode)) {
            ret = ret || rm_recursively(entry_path);
        } else if (S_ISREG(entry_st.st_mode)) {
            ret = ret || unlink(entry_path);
        }

        free(entry_path);
    }
    rmdir(path);

    exit: {
        if (errno != 0 && ret != 0) {
            printf("%s: error occured: %s\n", __func__, strerror(errno));
        }
        return ret;
    }
}

int descend(const char* src_path, const char* dst_path) {
    int ret = 0;

    DIR* src_dir = opendir(src_path);
    if (src_dir == NULL) {
        printf("failed to open %s (source): %s\n", src_path, strerror(errno));
        ret = 1;
        goto exit;
    }

    struct stat src_st;
    int src_ret = stat(src_path, &src_st);
    if (src_ret != 0) { ret = 1; goto exit; }
    
    struct stat dst_st = {0};
    if (stat(dst_path, &dst_st) != 0) {
        if (errno != ENOENT) { ret = 1; goto exit; }
        printf("%s does not exist, creating\n", dst_path);
        if (mkdir(dst_path, src_st.st_mode) == -1) {
            printf("failed to create %s: %s\n", dst_path, strerror(errno));
            ret = 1;
            goto exit;
        }
    }

    struct dirent* src_dirent;
    while ((src_dirent = readdir(src_dir)) != NULL) {
        if (strcmp(src_dirent->d_name, ".") == 0) { continue; }
        if (strcmp(src_dirent->d_name, "..") == 0) { continue; }
        char* src_entry_path = concat_paths_unsafe(src_path, src_dirent->d_name);
        char* dst_entry_path = concat_paths_unsafe(dst_path, src_dirent->d_name);

        printf("processing: %s\n", src_entry_path);

        struct stat src_entry_st;
        struct stat dst_entry_st;
        int src_stat_ret = stat(src_entry_path, &src_entry_st);
        int dst_stat_ret = stat(dst_entry_path, &dst_entry_st);
        if (src_stat_ret == 0 && !S_ISDIR(src_entry_st.st_mode) && !S_ISREG(src_entry_st.st_mode)) {
            printf("non-regular or non-directory entry %s encountered, unsupported\n", src_entry_path);
            continue;
        }
        if (dst_stat_ret == 0 && !S_ISDIR(dst_entry_st.st_mode) && !S_ISREG(dst_entry_st.st_mode)) {
            printf("non-regular or non-directory entry %s encountered, unsupported\n", dst_entry_path);
            continue;
        }
        if (S_ISDIR(src_entry_st.st_mode)) {
            if (dst_stat_ret != -1 && !S_ISDIR(dst_entry_st.st_mode)) { unlink(dst_entry_path); }
            chmod(dst_entry_path, src_entry_st.st_mode);
            ret = ret || descend(src_entry_path, dst_entry_path);
        } else if (S_ISREG(src_entry_st.st_mode)) {
            if (dst_stat_ret != -1 && !S_ISREG(dst_entry_st.st_mode)) { rm_recursively(dst_entry_path); }
            ret = ret || upd_file(src_entry_path, dst_entry_path);
        }
        free(src_entry_path);
        free(dst_entry_path);
    }

    exit: {
        closedir(src_dir);
        return ret;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: %s src dest archive_basename\n", argv[0]);
        exit(1);
    }

    int err = descend(argv[1], argv[2]);
    if (err == 0) {
        char buf[4096];
        snprintf(buf, 4096, "tar -c \"%s\" | gzip > \"%s.tar.gz\" && echo \"%s.tar.gz\" successfully created\n", argv[2], argv[3], argv[3]);
        system(buf);
    } else {
        system("echo some failure occured\n");
    }
}