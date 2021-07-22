#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <lx_string.h>
#include <minimisc.h>
#include <get_opts.h>

#include "http_errors.h"
#include "datatypes.h"
#include "options.h"
#include "header.h"
#include "req.h"
#include "resp.h"
#include "die.h"

struct http_errors hterrors[] = {
/* #define HTERR_OK       0 */
    { 200, "OK" },
/* #define HTERR_OK_CREAT 1 */
    { 201, "Created" },
/* #define HTERR_OK_ACCPT 2 */
    { 202, "Accepted" },
/* #define HTERR_OK_NOCNT 3 */
    { 204, "No Content" },
/* #define HTERR_BADREQ   4 */
    { 400, "Bad Request" },
/* #define HTERR_UNAUTH   5 */
    { 401, "Unauthorized" },
/* #define HTERR_FORBID   6 */
    { 403, "Forbidden" },
/* #define HTERR_NOTFOUND 7 */
    { 404, "Not Found" },
/* #define HTERR_TIMEOUT  8 */
    { 408, "Request Timeout" },
/* #define HTERR_NEEDLEN  9 */
    { 411, "Length Required" },
/* #define HTERR_SERVERR  10 */
    { 500, "Internal Server Error" },
/* #define HTERR_NOTIMPL  11 */
    { 501, "Not Implemented" },
    { 0, 0 }
};

static char *f_fatal   = "fatal",
            *f_warning = "warning",
            *f_debug   = "debug";

void die_nomem(void) {
    die_log(0, 0, "out of memory", 0);
}

void die_outerr(void) {
    die_log(ERRNO, 0, "output error", 0);
}

void die_usage(void) {
    fprintf (stderr, "usage: filthttp [options]\n");
    fflush(stderr);
    _exit (1);
}

static inline void sendfmt(char *s1, char *s2, char *s3, char *s4)
{
    fprintf(stderr, "[%u] %s:", sessionid, s1);
    if (s2) fprintf (stderr, " <%s>", s2); /* XXX: safe4log() */
    fprintf(stderr, " %s", s3);
    if (s4) fprintf (stderr, " (%s)", s4);
    fprintf (stderr, "\n");
    fflush(stderr);
}

/***************************************************************************
  log a message, die with an error code

  [err] is 0 or ERRNO.  If set, the errno string is included.
  [arg_info] is 0 or a string describing the object involved; included if !0
  [log_info] is the log message; must be a valid string.
  [user_comment] is 0 or the string to send to the user.  Sent if !0.
 ***************************************************************************/

void
die_log(int err, char *arg_info, char *log_info, char *user_comment)
{
    if (err & ERRNO) err = errno;

    sendfmt(f_fatal, arg_info, log_info, err ? strerror(err) : 0);
    if (user_comment)
        (void) lx_gdputs(gd_out, user_comment);

    lx_gdflush(gd_out);
    _exit(1);
}

void
log_debug(int level, char *arg_info, char *log_info)
{
    if (!(level & OPT_DEBUG)) return;
    sendfmt(f_debug, arg_info, log_info, 0);
}

void
log_warning(int err, char *arg_info, char *log_info)
{
    if (err & ERRNO) err = errno;
    sendfmt(f_warning, arg_info, log_info, err ? strerror(err) : 0);
}

/***************************************************************************
  a fatal error that results in an html message.

  [log_info] and [arg_info] are used as in die_log();
  [user_comment] is output in html on the resulting error page.

  [hterr] is the http error code, possibly ORed with the ERRNO flag.  If
  set, this flag means to include an errno message.
 ***************************************************************************/

void
die_html(int hterr, char *arg_info,
        char *log_info, char *user_comment)
{
    /* ignore all user-supplied data on error, including user-supplied req.
     * XXX: breaks spec?
     */
    struct reqinfo req = {
        {0}, {0}, 1, 0
    };

    lx_s body = {0},
         data = {0};

    int err = (hterr & ERRNO) ? errno : 0;
    int hterridx = hterr & (~ERRNO);

    sendfmt(f_fatal, arg_info, log_info, err ? strerror(err) : 0);

    if (stage & SENT_PREFIX) {
        /* oh, well.  Prefix already sent; may as well not supply user info
         * now, since we'd just corrupt the data already given.
         */
        goto JUSTDIE;
    }

    header_reinit(&blob_header_send);
    header_set_default(hterridx);

    /* RFC-compliance means that we gather this regardless of whether we
     * send the data.  I guess i care.
     */
    if (lx_stradd(&body,
                "<html>\r\n"
                "  <head>\r\n"
                "    <title>")) die_nomem();
    if (lx_stradd(&body, user_comment)) die_nomem();
    if (lx_stradd(&body, "</title>\r\n"
                "  </head>\r\n"
                "  <body>")) die_nomem();
    if (lx_stradd(&body, user_comment)) die_nomem();
    if (lx_stradd(&body, "</body>\r\n"
                "</html>\r\n")) die_nomem();

    if (lx_strset(&data, "text/html")) die_nomem();
    header_setstr(blob_header_send, "Content-Type", data.s, data.len);

    data.len = 0;
    if (lx_straddulong(&data, body.len, 10)) die_nomem();
    header_setstr(blob_header_send, "Content-Length", data.s, data.len);

    resp_sendprefix(&req, hterridx);
    resp_sendheaders(&req, blob_header_send);

    if (!global_flags.header_only)
        (void) lx_gdstrput(gd_out, &body);

JUSTDIE:
    (void) lx_gdflush(gd_out);

    _exit(1);
}
