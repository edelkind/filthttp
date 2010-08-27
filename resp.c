#include <lx_string.h>
#include <minimisc.h>
#include <get_opts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "datatypes.h"
#include "http_errors.h"
#include "options.h"
#include "header.h"
#include "conf.h"
#include "req.h"
#include "resp.h"
#include "die.h"
#include "fs.h"

/***************************************************************************
  - blobset or blobqueue?
 ***************************************************************************/

void
resp_sendprefix(struct reqinfo *req, ub2_t index)
{
    lx_s s = {0};
    ub2_t hterrno = hterrors[index].status;
    char *hterr   = hterrors[index].message;

#if 0
    for (;hterrors[hterridx][0]; hterridx++) {
        if (hterrors[hterridx][0] != errcode)
            continue;
    }

    /* if not found, this will be the zero at the end position */
    hterr = hterrors[hterridx][1];
#endif

    if (lx_striset(&s, "HTTP/", 5) ||
        lx_straddulong(&s, req->major, 10) ||
        lx_cadd(&s, '.') ||
        lx_straddulong(&s, req->minor, 10) ||
        lx_cadd(&s, ' ') ||
        lx_straddulong(&s, hterrno, 10) ||
        lx_cadd(&s, ' ') ||
        lx_stradd(&s, hterr) ||
        lx_striadd(&s, "\r\n", 2)) die_nomem();

    if (lx_gdstrput(gd_out, &s)) die_outerr();

    lx_free(&s);

    stage |= SENT_PREFIX;
}

void
resp_sendheaders(struct reqinfo *req, blobset *headers)
{
    ub1_t slice;
    blobentry *entry;

    /* XXX: hack to suppress req usage warning (should be optimized out) */
    do { } while (0 && req);

    for (slice = 0; slice < headers->elem; slice++) {
        entry = headers->hash[slice];
        while (entry) {
            if (lx_gdputsn(gd_out, (char *)entry->name, entry->namesize))
                die_outerr();
            if (lx_gdputsn(gd_out, ": ", 2)) die_outerr();
            if (entry->data) {
                if (lx_gdstrput(gd_out, entry->data)) die_outerr();
            }
            if (lx_gdputsn(gd_out, "\r\n", 2)) die_outerr();

            entry = entry->next;
        }
    }
    if (lx_gdputsn(gd_out, "\r\n", 2)) die_outerr();

    stage |= SENT_HEADER;
}

static inline int
openfile(struct stat *sb, char *path, int flags, int mode, char *logerrmsg)
{
    int fd;

    fd = open(path, flags, mode);
    if (fd < 0) {
        if (errno == ENOENT) {
            die_html(HTERR_NOTFOUND|ERRNO, path,
                    logerrmsg, err_not_found);
        }
        die_html(HTERR_FORBID|ERRNO, path,
                logerrmsg, err_perm_denied);
    }

    if (fstat(fd, sb)) {
        close(fd);
        /* XXX: 500 instead? */
        die_html(HTERR_FORBID|ERRNO, path,
                "opened file, but fstat() failed",
                err_perm_denied);
    }

    return fd;

}

/***************************************************************************
  headers:
  Content-Type (XXX: make customizable)
    
 ***************************************************************************/

void
resp_sendfile(struct reqinfo *req)
{
    int fd;
    struct stat sb;
    lx_s body = {0},
         data = {0};

    if (lx_check0(&req->location)) die_nomem();
    fd = open(req->location.s, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            die_html(HTERR_NOTFOUND|ERRNO, req->location.s,
                    "couldn't open file for reading", err_not_found);
        }
        die_html(HTERR_FORBID|ERRNO, req->location.s,
                "couldn't open file for reading",
                err_perm_denied);
    }

    if (fstat(fd, &sb)) {
        close(fd);
        /* XXX: 500 instead? */
        die_html(HTERR_FORBID|ERRNO, req->location.s,
                "opened file, but fstat() failed",
                err_perm_denied);
    }

    /* directory? send index (if config option is set) */
    if (S_ISDIR(sb.st_mode) && OPT_INDICES) {
        index_generate(fd, &sb, req, &body);
        close(fd);

        if (lx_strset(&data, "text/html")) die_nomem();
        header_setstr(blob_header_send, "Content-Type", data.s, data.len);

        data.len = 0;
        if (lx_straddulong(&data, body.len, 10)) die_nomem();
        header_setstr(blob_header_send, "Content-Length", data.s, data.len);

        resp_sendprefix(req, HTERR_OK);
        resp_sendheaders(req, blob_header_send);

        if (!global_flags.header_only) {
            if (lx_gdstrput(gd_out, &body)) die_outerr();
        }

        lx_free(&data);
        lx_free(&body);
        return;
    }

    if (!OPT_READABL) {
        close(fd);
        die_html(HTERR_FORBID, 0,
                "Read access forbidden",
                err_perm_denied);
    }

    if (!S_ISREG(sb.st_mode)) {
        close(fd);
        die_html(HTERR_FORBID, req->location.s,
                "not a regular file (may be a dir with 'indexes' unset)",
                err_perm_denied);
    }

    if (lx_strset(&data, "application/octet-stream")) die_nomem();
    header_setstr(blob_header_send, "Content-Type", data.s, data.len);

    data.len = 0;
    if (lx_straddulong(&data, sb.st_size, 10)) die_nomem();
    header_setstr(blob_header_send, "Content-Length", data.s, data.len);

    data.len = 0;
    header_mkdate(&data, sb.st_mtime);
    header_setstr(blob_header_send, "Last-Modified", data.s, data.len);

    resp_sendprefix(req, HTERR_OK);
    resp_sendheaders(req, blob_header_send);

    lx_free(&data);

    if (!global_flags.header_only) {
#define READBUFSIZ 8192
        char buf[READBUFSIZ];
        int r;

        for (;;) {
            r = read(fd, buf, READBUFSIZ);
            if (r < 0) {
                close(fd);
                die_html(HTERR_SERVERR|ERRNO, req->location.s,
                        "file opened, but read failed",
                        err_sys_generic);
            }
            if (sb.st_size < r) {
                log_warning(0, req->location.s,
                        "file longer than file size; truncating.");
                r = sb.st_size;  /* now < READBUFSIZ by definition */
            }

            if (lx_gdputsn(gd_out, buf, r)) die_outerr();
            sb.st_size -= r;

            if (r < READBUFSIZ) break;
        }
        if (sb.st_size) {
            log_warning(0, req->location.s, "premature EOF");
            /* XXX: fill with zeros? */
        }
    }
    close(fd);
}

/***************************************************************************
  receive a file from input; direct it to a destination file.

  The file length must be specified by the Content-Length header.
 ***************************************************************************/

void
resp_recvfile(struct reqinfo *req)
{
    struct fsfd h;
    lx_s dest_dir  = {0},
         dest_file = {0},
         data      = {0};

    lx_s *value;
    char *endlen;
    int length;

    if (!OPT_WRITABL) {
        die_html(HTERR_FORBID, 0,
                "Write access forbidden",
                err_perm_denied);
    }

    fs_ready();

    fs_parseout(&dest_dir, &dest_file, &req->location);

    if (lx_check0(&dest_dir) ||
        lx_check0(&dest_file) ||
        lx_check0(&req->location)) die_nomem();

    log_debug(DEBUG_NOISE, dest_dir.s, "destination directory");
    log_debug(DEBUG_NOISE, dest_file.s, "destination file");

    value = blob_get(blob_header_recv, "content-length", 14);
    if (!value) {
        die_html(HTERR_NEEDLEN, 0,
                "POST request without Content-Length",
                "Content-Length header required");
    }

    lx_check0(value);
    length = strtoul(value->s, &endlen, 10);

    if (endlen == value->s || *endlen) {
        die_html(HTERR_NEEDLEN, 0,
                "POST request with invalid Content-Length",
                "Content-Length header required");
    }

    fs_chdir_write(dest_dir.s);

#if 0
    if (!chdir(dest_dir.s)) {
        if (opt_p.v_opt.opt_toggle) {
            fs_mkpath(dest_dir.s);
        } else {
            die_html(HTERR_NOTFOUND|ERRNO, req->location.s,
                    "chdir to target directory",
                    "Invalid target directory");
        }
    }

    fs_okay_write_dir();
#endif

    /* XXX: address if lx_check0 is modified to not increment len! */
    fs_open_file_write(&h, dest_file.s, dest_file.len-1);

    fs_slurp_gd2h(&h, gd_in, length);

    log_debug(DEBUG_NOISE, h.dest, "received file from feed");

    fs_close_file_write(&h);

    if (h.status & ST_INCOMPLETE) {
        die_log(0, dest_file.s, "short read from input stream", 0);
    }

    if (lx_striset(&data, "0", 1)) die_nomem();
    header_setstr(blob_header_send, "Content-Length", data.s, data.len);

    resp_sendprefix(req,
            (h.status & ST_CREATED) ? HTERR_OK_CREAT : HTERR_OK_NOCNT);
    resp_sendheaders(req, blob_header_send);

    lx_free(&dest_dir);
    lx_free(&dest_file);
    lx_free(&data);
}

void
resp_headfile(struct reqinfo *req)
{
    global_flags.header_only = 1;
    resp_sendfile(req);
}

void
resp_rmfile(struct reqinfo *req)
{
    lx_s data = {0};

    if (!OPT_WRITABL) {
        die_html(HTERR_FORBID, 0,
                "Write access forbidden by configuration",
                err_perm_denied);
    }

    if (lx_check0(&req->location)) die_nomem();
    fs_delete(req->location.s);

    if (lx_striset(&data, "0", 1)) die_nomem();
    header_setstr(blob_header_send, "Content-Length", data.s, data.len);

    resp_sendprefix(req, HTERR_OK);
    resp_sendheaders(req, blob_header_send);

    lx_free(&data);
}

