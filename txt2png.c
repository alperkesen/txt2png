#define FUSE_USE_VERSION 26
#define TEXT "text"
#define ANSI "application/octet-stream"
#define PNG  ".png"
#define MAXSIZE  100000

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
#include <ansilove.h>


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

    if (strncmp(magic_full, TEXT, strlen(TEXT)) == 0 || strncmp(magic_full, ANSI, strlen(ANSI)) == 0) {
      magic_close(magic_cookie);
      return 1;
    } else {
      magic_close(magic_cookie);
      return 0;
    }
}


char *rw_path;

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
    strcat(png_name, PNG);

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

static char *split_dir(const char *path) {
    char *last_occ;
    char *dirpath;

    last_occ = strrchr(path, '/');
    dirpath = malloc(sizeof(char) * (last_occ - path + 2));

    strncpy(dirpath, path, (last_occ - path + 1));
    dirpath[last_occ - path + 1] = '\0';

    return dirpath;
}


static char *split_filename(const char *path) {
    char *last_occ;
    char *filename;

    last_occ = strrchr(path, '/');
    filename = malloc(sizeof(char) * (path + strlen(path) - last_occ + 1));

    strncpy(filename, last_occ + 1, path + strlen(path) - last_occ);
    filename[path + strlen(path) - last_occ + 1] = '\0';

    return filename;
}


static int txt2png_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    char *upath;

    if (strncmp(path + strlen(path) - strlen(PNG), PNG, strlen(PNG)) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
	stbuf->st_size = MAXSIZE;
    } else {
        upath = translate_path(path);
        res = lstat(upath, stbuf);
	free(upath);

	if (res == -1) {
	    return -errno;
	}
    }

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


static int txt2png_open(const char *path, struct fuse_file_info *finfo)
{
    int res;
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
        return -EROFS;
    }

    DIR *dp;
    struct dirent *de;
    char *dirname = split_dir(path);
    char *dirpath = translate_path(dirname);
    char *filepath;
    free(dirname);

    dp = opendir(dirpath);

    if (dp == NULL) {
        res = -errno;
        return res;
    }

    char *fn = split_filename(path);

    while((de = readdir(dp)) != NULL) {
        if (strncmp(de->d_name, fn, strlen(fn) - strlen(PNG)) == 0) {
	    filepath = get_filepath(dirpath, de->d_name);
	    break;
        }
    }

    free(dirpath);
    closedir(dp);

    if (filepath == NULL) {
        return -errno;
    }

    res = open(filepath, flags);
    free(filepath);

    if (res == -1) {
        return -errno;
    }

    close(res);

    return 0;
}


static int txt2png_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    struct ansilove_ctx ctx;
    struct ansilove_options options;

    int fd;
    int res;
    (void)finfo;

    DIR *dp;
    struct dirent *de;
    char *dirname = split_dir(path);
    char *dirpath = translate_path(dirname);
    char *filepath;
    free(dirname);

    dp = opendir(dirpath);

    if (dp == NULL) {
        res = -errno;
        return res;
    }

    char *fn = split_filename(path);

    while ((de = readdir(dp)) != NULL) {
        if (strncmp(de->d_name, fn, strlen(fn) - strlen(PNG)) == 0) {
	    filepath = get_filepath(dirpath, de->d_name);
	    break;
        }
    }

    free(dirpath);
    closedir(dp);

    if (filepath == NULL) {
        return -errno;
    }

    ansilove_init(&ctx, &options);

    ansilove_loadfile(&ctx, filepath);
    ansilove_ansi(&ctx, &options);

    char *upath = translate_path(path);

    ansilove_savefile(&ctx, NULL);

    free(upath);

    if (offset < ctx.png.length) {
      if (offset + size > ctx.png.length) {
        size = ctx.png.length - offset;
      }
      memcpy(buf, ctx.png.buffer + offset, size);
    } else {
      size = 0;
    }

    ansilove_clean(&ctx);

    return size;
}


struct fuse_operations txt2png_oper = {
    .getattr     = txt2png_getattr,
    .readdir     = txt2png_readdir,
    .open        = txt2png_open,
    .read        = txt2png_read,
};


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
    default:
        fprintf(stderr, "see `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}

static struct fuse_opt txt2png_opts[] = {
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
        exit(1);
    }

    if (rw_path == 0)
    {
        fprintf(stderr, "Missing source and destination path\n");
        exit(1);
    }

    fuse_main(args.argc, args.argv, &txt2png_oper, NULL);

    return 0;
}
