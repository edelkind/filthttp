#define _BSD_SOURCE

#include <lx_string.h>
#include <minimisc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "http_errors.h"
#include "req.h"
#include "resp.h"
#include "conf.h"
#include "die.h"

#define DTNAM_DIR   "dir    "
#define DTNAM_FILE  "file   "
#define DTNAM_OTHER "unknown"


static void
entry_prefix(lx_s *body)
{
    /* XXX: worth checking for http metachars?
     * if anyone seriously reports this for XSS, i'm going to shoot myself
     */
    if (lx_stradd(body,
                "<html>"CRLF
                " <head>"CRLF
                "  <title>Directory index</title>"CRLF
                " </head>"CRLF
                CRLF
                " <body>"CRLF
                "<h2>Directory index</h2>"CRLF
                CRLF
                "<pre>"CRLF
                /*"   "DTNAM_DIR"  <a href=\"../\">../</a><br>"CRLF */
                )) die_nomem();

}

static void
entry_suffix(lx_s *body)
{
    if (lx_stradd(body,
                "</pre>"CRLF
                CRLF
                " </body>"CRLF
                "</html>"CRLF)) die_nomem();
}

/***************************************************************************
  add an entry... but also get statistical information about the files for
  display.

  XXX: Perhaps add mtime in the future
 ***************************************************************************/

static void
entry_add(lx_s *body, struct dirent *dent)
{
    char *dt;
    char *dname = dent->d_name;
    struct stat sb;
    lx_s mode = {0};
    lx_s size = {0};
    unsigned perms;

#ifdef HAVE_D_NAMLEN    /* XXX: add to configuration */
    unsigned dnamlen = dent->d_namlen;
#else
    unsigned dnamlen = strlen(dname);
#endif

    if (!strcmp(dname, ".")) return;

    /* XXX: make lstat()? with stat(), symlinks could cause confusion */
    if (stat(dname, &sb)) {
        /* XXX: should probably be debug, really, since symbolic links to
         * invalid filenames will trigger this */
        log_warning(ERRNO, dname, "stat");
        return;
    }

    if (S_ISDIR(sb.st_mode)) {
        dt = DTNAM_DIR;
        /* replace terminating 0 with '/' for our usage */
        dname[dnamlen++] = '/';
    } else if (S_ISREG(sb.st_mode)) {
        dt = DTNAM_FILE;
    } else {
        dt = DTNAM_OTHER;
    }

    /* get ascii representation of numerical mode */
    perms = sb.st_mode & 07777;
    if (lx_setalloc(&mode, 5)) die_nomem();
    if (perms >= 01000)
        lx_cadd(&mode, '0');
    else {
        lx_striadd(&mode, " 0", 2);
        if (perms < 0100) {
            lx_cadd(&mode, '0');
            if (perms < 010) {
                lx_cadd(&mode, '0');
            }
        }
    }
    lx_straddulong(&mode, perms, 8);


    /* get a normalized size value */

#define kmask 0xff
#define kshrink(n, type) \
    ((type) (((type)n & ~(type)kmask) + (kmask+1)) >> 10)
    {
        char type = 'b';
        off_t len = sb.st_size;

        if (lx_setalloc(&size, 5)) die_nomem();

        if (len >= 4096) {
            len  = kshrink(len, off_t);
            type = 'k';
        }
        if (len >= 4096) {
            len  = kshrink(len, off_t);
            type = 'm';
        }
        if (len >= 4096) {
            len  = kshrink(len, off_t);
            type = 'g';
        }
        if (len < 1000) {
            lx_cadd(&size, ' ');
            if (len < 100) {
                lx_cadd(&size, ' ');
                if (len < 10)
                    lx_cadd(&size, ' ');
            }
        }
        if (lx_straddulong(&size, len, 10)) die_nomem();
        if (lx_cadd(&size, type)) die_nomem();
    }

    if (lx_striadd(body, "   ", 3) ||
        lx_stradd(body, dt) ||
        lx_cadd(body, ' ') ||
        lx_strcat(body, &mode) ||
        lx_striadd(body, "  ", 2) ||
        lx_strcat(body, &size) ||
        lx_stradd(body, "  <a href=\"") ||
        lx_striadd(body, dname, dnamlen) ||
        lx_striadd(body, "\">", 2) ||
        lx_striadd(body, dname, dnamlen) ||
        lx_stradd(body, "</a>"CRLF)) die_nomem();

    lx_free(&mode);
    lx_free(&size);
}


/***************************************************************************
  generate an index page and place the contents into [body].
  [fd] is an open descriptor to a directory.

  The entire directory must fit within [body] in order to comply with the
  RFC's Content-Length requirement.  Grrr.

  XXX: Note the above in docs so people can set their heap size limits
  accordingly!

  getdirentries()/getdents() is used because there is no portable facility
  to create DIR* pointers from fds, and i'm damn well not going to reopen
  the descriptor.  I shouldn't have to; it's their fault.
 ***************************************************************************/

static void
process_dents(lx_s *body, struct dirent *dentp, int dbytes)
{
    while (dbytes) {
        if (dentp->d_reclen > dbytes) {
            die_html(HTERR_SERVERR, 0,
                    "dir record is larger than available entry size (fs error?)",
                    err_dir_index);
        }
        if (dentp->d_fileno) entry_add(body, dentp);

        dbytes -= dentp->d_reclen;
        dentp = (struct dirent *)((char *)dentp + dentp->d_reclen);
    }
}

void
index_generate(
        int fd,
        void *void_st,
        struct reqinfo *req,
        lx_s *body)
{
    struct stat *st = (struct stat *)void_st;
    int bsiz = st->st_blksize > 8192 ? st->st_blksize : 8192;
    char dbuf[bsiz];
    int dbytes;

#ifndef USE_GETDENTS
    long base = 0;
#endif

    if (fchdir(fd)) {
        die_html(HTERR_FORBID|ERRNO, req->location.s,
                "error changing directory",
                err_perm_denied);
    }

    entry_prefix(body);

    for (;;) {
#ifndef USE_GETDENTS
        dbytes = getdirentries(fd, dbuf, bsiz, &base);
#else
        dbytes = getdents(fd, dbuf, bsiz);
#endif
        if (dbytes < 0) {
            die_html(HTERR_SERVERR|errno, req->location.s,
                    "error reading directory entries",
                    err_dir_index);
        }
        if (!dbytes) break;

        process_dents(body, (struct dirent *)dbuf, dbytes);
    }

    entry_suffix(body);
}
