/*
 * ROFS - The read-only filesystem for FUSE.
 * Copyright 2005,2006,2008 Matthew Keller. kellermg@potsdam.edu and others.
 * v2008.09.24
 *
 * Mount any filesytem, or folder tree read-only, anywhere else.
 * No warranties. No guarantees. No lawyers.
 *
 * I read (and borrowed) a lot of other FUSE code to write this.
 * Similarities possibly exist- Wholesale reuse as well of other GPL code.
 * Special mention to Rémi Flament and his loggedfs.
 *
 * Consider this code GPLv2.
 *
 * Compile: gcc -o rofs -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse rofs.c
 * Mount: rofs readwrite_filesystem mount_point
 *
 */


#define FUSE_USE_VERSION 26
#define TEXT "text"

static const char* rofsVersion = "2008.09.24";

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <ansilove.h>
#include <fuse.h>
#include <magic.h>


int is_text(const char *actual_file)
{
    const char *magic_full;
    magic_t magic_cookie;

    magic_cookie = magic_open(MAGIC_MIME);

    if (magic_cookie == NULL) {
        printf("unable to initialize magic library\n");
        return -1;
    }

    if (magic_load(magic_cookie, NULL) != 0) {
        printf("cannot load magic database - %s\n", magic_error(magic_cookie));
        magic_close(magic_cookie);
        return -1;
    }

    magic_full = magic_file(magic_cookie, actual_file);

    if (strncmp(magic_full, TEXT, strlen(TEXT)) == 0) {
      magic_close(magic_cookie);
      return 1;
    } else {
      magic_close(magic_cookie);
      return 0;
    }
}


// Global to store our read-write path
char *rw_path;

// Translate an rofs path into it's underlying filesystem path
static char* translate_path(const char* path)
{
    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(rw_path)+1));

    strcpy(rPath,rw_path);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);

    return rPath;
}

static char *get_pngname(const char *filename) {
    const char *ext;
    char *png_name;

    ext = strrchr(filename, '.');

    if (!ext) {
      ext = &filename[strlen(filename)];
    }

    png_name = malloc(sizeof(char) * (ext - filename + 4));

    strncpy(png_name, filename, (ext - filename));
    png_name[ext - filename] = '\0';
    strcat(png_name, ".png");

    return png_name;
}

static char* get_filepath(const char *dirpath, const char* path)
{
    char *rPath= malloc(sizeof(char) * (strlen(path) + strlen(dirpath) + 2));

    strcpy(rPath, dirpath);

    if (rPath[strlen(rPath)-1] != '/') {
        strcat(rPath, "/");
    }

    strcat(rPath, path);

    return rPath;
}


/******************************
*
* Callbacks for FUSE
*
*
*
******************************/

static int txt2png_getattr(const char *path, struct stat *st_data)
{
    int res;
    char *upath=translate_path(path);

    res = lstat(upath, st_data);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return 0;
}

static int txt2png_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char *upath=translate_path(path);

    res = readlink(upath, buf, size - 1);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    buf[res] = '\0';
    return 0;
}

static int txt2png_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    int res;

    (void) offset;
    (void) fi;
    char *dirpath = translate_path(path);

    printf("Dirpath: %s\n", dirpath);

    dp = opendir(dirpath);

    if(dp == NULL) {
        res = -errno;
        return res;
    }

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

	char *filepath = get_filepath(dirpath, de->d_name);
	printf("Filepath: %s\n", filepath);
	free(filepath);

	if (st.st_mode == S_IFDIR) {
	    if (filler(buf, de->d_name, &st, 0)) {
	        break;
	    }
	} else {
	    char *filepath = get_filepath(dirpath, de->d_name);
	    int is_textfile = is_text(filepath);
	    free(filepath);

	    printf("%s is %d (1: text)\n", de->d_name, is_textfile);

	    if (is_textfile == 1) {
	        char *png_name = get_pngname(de->d_name);

	        if (filler(buf, png_name, NULL, 0)) {
		    free(png_name);
	            break;
	        }
		free(png_name);
	    }
	}
    }
    free(dirpath);

    closedir(dp);
    return 0;
}

static int txt2png_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -EROFS;
}

static int txt2png_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;
}

static int txt2png_unlink(const char *path)
{
    (void)path;
    return -EROFS;
}

static int txt2png_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int txt2png_symlink(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int txt2png_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int txt2png_link(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int txt2png_chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;

}

static int txt2png_chown(const char *path, uid_t uid, gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -EROFS;
}

static int txt2png_truncate(const char *path, off_t size)
{
    (void)path;
    (void)size;
    return -EROFS;
}

static int txt2png_utime(const char *path, struct utimbuf *buf)
{
    (void)path;
    (void)buf;
    return -EROFS;
}

static int txt2png_open(const char *path, struct fuse_file_info *finfo)
{
    int res;

    /* We allow opens, unless they're tring to write, sneaky
     * people.
     */
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
        return -EROFS;
    }

    char *upath=translate_path(path);

    res = open(upath, flags);

    free(upath);
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

static int txt2png_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    int fd;
    int res;
    (void)finfo;

    char *upath=translate_path(path);

    fd = open(upath, O_RDONLY);
    free(upath);
    if(fd == -1) {
        res = -errno;
        return res;
    }
    res = pread(fd, buf, size, offset);

    if(res == -1) {
        res = -errno;
    }
    close(fd);
    return res;
}

static int txt2png_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return -EROFS;
}

static int txt2png_statfs(const char *path, struct statvfs *st_buf)
{
    int res;
    char *upath=translate_path(path);

    res = statvfs(upath, st_buf);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int txt2png_release(const char *path, struct fuse_file_info *finfo)
{
    (void) path;
    (void) finfo;
    return 0;
}

static int txt2png_fsync(const char *path, int crap, struct fuse_file_info *finfo)
{
    (void) path;
    (void) crap;
    (void) finfo;
    return 0;
}

static int txt2png_access(const char *path, int mode)
{
    int res;
    char *upath=translate_path(path);

    /* Don't pretend that we allow writing
     * Chris AtLee <chris@atlee.ca>
     */
    if (mode & W_OK)
        return -EROFS;

    res = access(upath, mode);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return res;
}

/*
 * Set the value of an extended attribute
 */
static int txt2png_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -EROFS;
}

/*
 * Get the value of an extended attribute.
 */
static int txt2png_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = lgetxattr(upath, name, value, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;
}

/*
 * List the supported extended attributes.
 */
static int txt2png_listxattr(const char *path, char *list, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = llistxattr(upath, list, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;

}

/*
 * Remove an extended attribute.
 */
static int txt2png_removexattr(const char *path, const char *name)
{
    (void)path;
    (void)name;
    return -EROFS;

}

struct fuse_operations txt2png_oper = {
    .getattr     = txt2png_getattr,
    .readlink    = txt2png_readlink,
    .readdir     = txt2png_readdir,
    .mknod       = txt2png_mknod,
    .mkdir       = txt2png_mkdir,
    .symlink     = txt2png_symlink,
    .unlink      = txt2png_unlink,
    .rmdir       = txt2png_rmdir,
    .rename      = txt2png_rename,
    .link        = txt2png_link,
    .chmod       = txt2png_chmod,
    .chown       = txt2png_chown,
    .truncate    = txt2png_truncate,
    .utime       = txt2png_utime,
    .open        = txt2png_open,
    .read        = txt2png_read,
    .write       = txt2png_write,
    .statfs      = txt2png_statfs,
    .release     = txt2png_release,
    .fsync       = txt2png_fsync,
    .access      = txt2png_access,

    /* Extended attributes support for userland interaction */
    .setxattr    = txt2png_setxattr,
    .getxattr    = txt2png_getxattr,
    .listxattr   = txt2png_listxattr,
    .removexattr = txt2png_removexattr
};
enum {
    KEY_HELP,
    KEY_VERSION,
};

static void usage(const char* progname)
{
    fprintf(stdout,
            "usage: %s readwritepath mountpoint [options]\n"
            "\n"
            "   Mounts readwritepath as a read-only mount at mountpoint\n"
            "\n"
            "general options:\n"
            "   -o opt,[opt...]     mount options\n"
            "   -h  --help          print help\n"
            "   -V  --version       print version\n"
            "\n", progname);
}

static int txt2png_parse_opt(void *data, const char *arg, int key,
			     struct fuse_args *outargs)
{
    (void) data;

    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
        if (rw_path == 0)
        {
            rw_path = strdup(arg);
            return 0;
        }
        else
        {
            return 1;
        }
    case FUSE_OPT_KEY_OPT:
        return 1;
    case KEY_HELP:
        usage(outargs->argv[0]);
        exit(0);
    case KEY_VERSION:
        fprintf(stdout, "TXT2PNG version %s\n", rofsVersion);
        exit(0);
    default:
        fprintf(stderr, "see `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}

static struct fuse_opt txt2png_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res;

    res = fuse_opt_parse(&args, &rw_path, txt2png_opts, txt2png_parse_opt);
    if (res != 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }
    if (rw_path == 0)
    {
        fprintf(stderr, "Missing readwritepath\n");
        fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
        exit(1);
    }

    fuse_main(args.argc, args.argv, &txt2png_oper, NULL);

    return 0;
}
