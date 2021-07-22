#include <lx_string.h>
#include <get_opts.h>
#include <minimisc.h>

#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include "http_errors.h"
#include "options.h"
#include "die.h"
#include "fs.h"

#if 0 /* XXX nouser */
static char *restrict_dirs[] = {
    HT_DIR_BASE,
    HT_DIR_LOCK,
    0,
};

static char *restrict_files[] = {
    HT_FILE_THROTTLE,
    0,
};

blobqueue *riq; /* restricted inode queue */
#endif /* XXX nouser */

#if 0 /* XXX */
static inline void fs_mkdir_sys(char *path, struct stat *statbuf)
{
    if (mkdir(path, 0700)) {
        die_html(HTERR_SERVERR|ERRNO, path,
                "fs_mkdir_sys->mkdir", err_sys_generic);
    }
    if (stat(path, statbuf)) {
        die_html(HTERR_SERVERR|ERRNO, path,
                "fs_mkdir_sys->stat", err_sys_generic);
    }
}
#endif

#if 0 /* XXX nouser */
static inline void fs_sys_mustwrite(char *path)
{
    if (opt_syswrite.v_opt.opt_toggle)
        return;

    die_html(HTERR_SERVERR, path,
            "please create system files for this user",
            err_sys_generic);
}

static inline struct stat *fs_sys_chkdir(char *path)
{
    struct stat *stp = malloc(sizeof struct stat);
    if (!stp) die_nomem();

    if (lstat(path, stp)) {
        if (errno != ENOENT) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkdir->lstat", err_sys_generic);
        }

        fs_sys_mustwrite(path);

        if (mkdir(path, 0700)) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkdir->mkdir", err_sys_generic);
        }
        if (stat(path, stp)) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkdir->lstat", err_sys_generic);
        }
    }

    if (!S_ISDIR(stp->st_mode)) {
        die_html(HTERR_SERVERR, path,
                "exists, but is not a directory", err_sys_generic);
    }

    return stp;
}

static inline struct stat *fs_chkthrottle(char *path)
{
    struct stat *stp = malloc(sizeof struct stat);
    int fd;

    if (!stp) die_nomem();

    if (lstat(path, stp)) {
        if (errno != ENOENT) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkdir->lstat", err_sys_generic);
        }

        fs_sys_mustwrite(path);

        /* XXX: decide on mode/flags */
        /* XXX: open() follows symlinks; O_NOFOLLOW is not portable. */
        fd = open(path, O_RDWRIO_CREAT, 0600);
        if (fd < 0) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkthrottle->open", err_sys_generic);
        }

        if (fstat(fd, stp)) {
            die_html(HTERR_SERVERR|ERRNO, path,
                    "fs_sys_chkdir->fstat", err_sys_generic);
        }
    if (!S_ISREG(stp->st_mode)) {
        die_html(HTERR_SERVERR, path,
                "exists, but is not a regular file", err_sys_generic);
    }
}
#endif /* XXX nouser */

/***************************************************************************
  fs_ready() must be called first, as it both creates the essential
  directories and populates the restricted inode set.
 ***************************************************************************/

void fs_ready(void) {
#if 0 /* XXX nouser */
    struct stat *sb;

    riq = blobqueue_new();

    sb = fs_sys_chkdir(HT_DIR_BASE);

    if (blobqueue_push(riq, BLOB_LAST, (void *)sb, free))
        die_nomem();

    sb = fs_sys_chkdir(HT_DIR_LOCK);

    if (blobqueue_push(riq, BLOB_LAST, (void *)sb, free))
        die_nomem();

    sb = fs_sys_chkthrottle(HT_FILE_THRTL);

    if (blobqueue_push(riq, BLOB_LAST, (void *)sb, free))
        die_nomem();
#endif /* XXX nouser */

}

static inline void
fs_mkdir(char *path)
{
    if (mkdir(path, 0777)) {
        if (errno == EEXIST) return;
        die_html(HTERR_FORBID|ERRNO, path,
                "mkdir", err_perm_denied);
    }
    log_debug(DEBUG_INFO, path, "created subdirectory");
}

static inline void
fs_mkpath(char *path)
{
    char *p, *last = path;

    for (;;) {
        p = index(last, '/');
        if (p == last) {
            last++; p++;
            continue;
        }

        if (p) *p = 0;

        fs_mkdir(path);
        if (!p) break;

        *p = '/';
        last = p;
    }
}

void
fs_chdir_write(char *path)
{
    if (OPT_MKPATH)
        fs_mkpath(path);

    if (chdir(path)) {
        die_html(HTERR_FORBID|ERRNO, path,
                "chdir", err_perm_denied);
    }
}

void
fs_delete(char *path)
{
    struct stat sb;
    int (*rmfunc)() = unlink;
    char *msg = "unlink";

    if (stat(path, &sb)) {
        if (errno == ENOENT) {
            die_html(HTERR_NOTFOUND|ERRNO, path,
                    "delete", err_not_found);
        }
        die_html(HTERR_FORBID|ERRNO, path,
                "delete", err_perm_denied);
    }

    if (S_ISDIR(sb.st_mode)) {
        rmfunc = rmdir;
        msg    = "rmdir";
    }
    if (rmfunc(path)) {
        if (errno == ENOENT) {
            die_html(HTERR_NOTFOUND|ERRNO, path,
                    msg, err_not_found);
        }
        die_html(HTERR_FORBID|ERRNO, path,
                msg, err_perm_denied);
    }

    log_debug(DEBUG_NOISE, path, msg);
}

/***************************************************************************
  parse [src] path into directory and file, respectively.

  if there is no explicit path included in [src], then [dir] is implicitly
  "/".

  if [src] ends in '/', then [file] will be of zero length; keep this in
  mind when addressing filename feasibility.

  dies on error.
 ***************************************************************************/

void
fs_parseout(lx_s *dir, lx_s *file, lx_s *src)
{
    unsigned l = src->len, plen;
    char *p = &src->s[l-1];

    for (;;p--) {
        if (!l--) {
            if (lx_striset(dir, "/", 1)) die_nomem();
            if (lx_strcopy(file, src)) die_nomem();
            break;
        }

        if (*p != '/') continue;

        plen = src->len - (l+1);

        if (lx_striset(file, ++p, plen)) die_nomem();

        if (!l) l++; /* only one slash; use it for the dirname */
        if (lx_striset(dir, src->s, l)) die_nomem();
        break;
    }
}

/***************************************************************************
  open a file associated with [name] for writing.

  [h->fd], the file descriptor associated with handle [h], is set to the fd
  of the open file.

  If atomic operation is in effect, a temporary filename will be allocated
  and used.  Otherwise, [name] will be opened for writing directly.
 ***************************************************************************/

void
fs_open_file_write(struct fsfd *h, char *name, unsigned namelen)
{
    int fd;

    if (OPT_ATOMIC) {
        char *tmp, *p;

        tmp = malloc(namelen+8);
        if (!tmp) die_nomem();

        memcpy(tmp, name, namelen);
        p = tmp + namelen;
        *p++ = '.';
        *p++ = 'X'; *p++ = 'X'; *p++ = 'X';
        *p++ = 'X'; *p++ = 'X'; *p++ = 'X';
        *p   = 0;

        fd = mkstemp(tmp);
        if (fd < 0) {
            die_html(HTERR_FORBID|ERRNO, tmp,
                    "mkstemp", err_perm_denied);
        }

        h->target = name;
        h->dest   = tmp;
        h->fd     = fd;
        h->status = 0;
    } else {
        fd = open(name, O_WRONLY|O_CREAT, OPT_MODE);
        if (h->fd < 0) {
            die_html(HTERR_FORBID|ERRNO, name,
                    "open O_WRONLY|O_CREAT", err_perm_denied);
        }

        h->target = 0;
        h->dest   = name;
        h->fd     = fd;
        h->status = 0;
    }
}

/***************************************************************************
  close the file handle [h], and its associated file descriptor.

  if h->target is non-null, then atomic operation is in effect, and the file
  must be renamed to the original name.  The previously allocated memory
  used for the temporary filename is then freed.

  if atomic operation is in effect and the entire file is not completed, the
  temporary file is simply deleted.

  XXX: Note that the configurable permissions ANDed with the netmask are not
  currently in effect for atomic writes (see mkstemp(3)).
 ***************************************************************************/

void
fs_close_file_write(struct fsfd *h)
{
    close(h->fd);
    if (h->target) {
        if (h->status & ST_INCOMPLETE) {
            unlink(h->dest);
        } else if (rename(h->dest, h->target)) {
            die_html(HTERR_SERVERR|ERRNO, h->target,
                    "rename", err_sys_generic);
        }
        /* XXX: change mode? */
        free(h->dest);
    }
}

static inline int
writeall(int fd, char *buf, int len)
{
    int n;

    for (;;) {
        n = write(fd, buf, len);
        if (n == len) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        buf += n;
        len -= n;
    }
}

void
fs_slurp_gd2h(struct fsfd *h, lx_gd *gd, unsigned len)
{
    lx_s readbuf = {0};
    int n;

    while(len) {
        n = lx_gdread(&readbuf, gd, (len > gd->a) ? gd->a : len, 0);
        if (!n) {
            h->status |= ST_INCOMPLETE;
            break;
        }

        if (n < 0) {
            die_html(HTERR_SERVERR|ERRNO, h->dest,
                    "while reading input feed", err_sys_generic);
        }

        len -= n;
        if (writeall(h->fd, readbuf.s, readbuf.len)) {
            die_html(HTERR_SERVERR|ERRNO, h->dest,
                    "while writing to putput handle", err_sys_generic);
        }
    }

    if (readbuf.s) lx_free(&readbuf);
}

