#include <lx_string.h>
#include <minimisc.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "datatypes.h"
#include "http_errors.h"
#include "header.h"
#include "conf.h"
#include "req.h"
#include "resp.h"
#include "die.h"

void req_recv (lx_s *req)
{
    ub1_t match;

    if (lx_getln(req, gd_in, &match, MAXLINELEN)) {
        die_html(HTERR_BADREQ|ERRNO, 0,
                "error on request read", err_header_parse);
    }

    if (match & MATCH_TOOLONG) {
        die_html(HTERR_BADREQ, 0,
                "request line too long", err_header_parse);
    }

    if (req->len <= 2) {
        die_html(HTERR_BADREQ, 0,
                "missing request", err_header_parse);
    }

    lx_chop(req, 1);
    if (req->s[req->len-1] != '\r') {
        die_html(HTERR_BADREQ, 0,
                "request not RFC-compliant", err_header_parse);
    }
    if (req->s[req->len-1] == '\r') lx_chop(req, 1);
}

/***************************************************************************
  hexval assumes that the character passed is hexidecimal; else, an
  undefined value will be returned.

  returns the integer value for the character passed.
 ***************************************************************************/

static inline int hexval(ub1_t c)
{
    /* ASCII translation table, with '0' at index 0
     * 'A'-'F' must be upper-case
     */
    static ub1_t hexval_table[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 0, 0, 0, 0, 0, 0,  /* unused: 0x3a - 0x40 */
        0xa, 0xb, 0xc, 0xd, 0xe, 0xf
    };

    return hexval_table[toupper(c)-'0'];
}

/***************************************************************************
  Pluck one url-encoded byte off [*srcp] and place it on [dest].
  If the character is the start of a percent-encoded byte, the 2-char value
  of this byte is plucked off as well, and [*srcp] and [*srclenp] are
  advanced appropriately.

  returns 0 on success;
  returns 1 if there was an error decoding the next character;
  dies on out-of-memory error.
 ***************************************************************************/

static inline int pluck_urlencoded(lx_s *dest, char **srcp, ub4_t *srclenp)
{
    /* bitmask for unreserved characters as related to the ASCII character
     * set.  Note that '/' is reserved, but is added anyway, because we're
     * including it in our URIs. */

    /* reserved characters:
     * 0x2c(,)
     * 0x2d(-)
     * 0x2e(.)
     * 0x2f(/)
     * 0x30(0) - 0x39(9)
     * 0x41(A) - 0x5a(Z)
     * 0x5f(_)
     * 0x61(a) = 0x7a(z)
     * 0x7e(~)
     */
    static ub1_t unreserved[] = {
        0x00, 0x00,  /* 0x00-0x0f */
        0x00, 0x00,  /* 0x10-0x1f */
        0x00, 0x0f,  /* 0x20-0x2f; 0x2c(08)+0x2d(04)+0x2e(02)+0x2f(01) */
        0xff, 0xc0,  /* 0x30-0x3f; 0x38(80)+0x39(40) */
        0x7f, 0xff,  /* 0x40-0x4f; all except 0x40(80) */
        0xff, 0xe1,  /* 0x50-0x5f; 0x58(80)+0x59(40)+0x5a(20) +0x5f(01) */
        0x7f, 0xff,  /* 0x60-0x6f; all except 0x60(80) */
        0xff, 0xe2,  /* 0x70-0x7f; 0x78(80)+0x79(40)+0x7a(20) +0x7e(02) */
        0x00, 0x00,  /* 0x80-0x8f */
        0x00, 0x00,  /* 0x90-0x9f */
        0x00, 0x00,  /* 0xa0-0xaf */
        0x00, 0x00,  /* 0xb0-0xbf */
        0x00, 0x00,  /* 0xc0-0xcf */
        0x00, 0x00,  /* 0xd0-0xdf */
        0x00, 0x00,  /* 0xe0-0xef */
        0x00, 0x00,  /* 0xf0-0xff */
    };

    char *src = *srcp;
    ub4_t srclen = *srclenp;
    ub1_t c = (ub1_t)*src, cdest;

    if (c == '%') {
        if (--srclen < 2) return 1;
        c = *++src;

        if (!isxdigit(c)) return 1;
        cdest = hexval(c) << 4;

        c = *++src; --srclen;
        if (!isxdigit(c)) return 1;
        cdest |= hexval(c);

        *srcp = src;
        *srclenp = srclen;
        c = cdest;
    } else {
        if (!(unreserved[c/8] & (0x80 >> c % 8)))
            return 1;
    }

    if (lx_cadd(dest, c)) die_nomem();
    return 0;
}

/***************************************************************************
  GET /some/location HTTP/1.1
  ^command   ^            ^
             +-filename   |
                          +-major.minor

  - filename is URL-encoded
  - filename may contain variable names
  - filename may contain offset markers
  - filename may or may not be preceded by an identifier
    - /path
    - http://hostname/path
 ***************************************************************************/

void req_parse(struct reqinfo *req, lx_s *reqline)
{
    char *rlp = reqline->s;
    ub4_t rll = reqline->len;
    char c;
    ub1_t          ignore = 0,
              skiptoslash = 1;


    /* arg1: command (GET, POST) */
    for (;rll;rll--,rlp++) {
        c = toupper(*rlp);

        if (c == ' ') { rll--,rlp++; break; }
        if ((c >= 'A' && c <= 'Z')) {
            if (lx_cadd(&req->cmd, c)) die_nomem();
        } else {
            die_html(HTERR_BADREQ, 0,
                    "malformed command request", err_req_parse);
        }
    }

    /* arg2: location (http://some/path, /some/path) */
    for (;rll;rll--,rlp++) {
        c = *rlp;

        if (c == ' ') { rll--,rlp++; break; }
        /* XXX: variables may be collected later */
        if (c == '?' || c == '#') ignore = 1;
        if (ignore)   continue;

        if (c == '/' && skiptoslash && --skiptoslash) continue;
        else if (skiptoslash) /* c != '/' */          continue;

        if (pluck_urlencoded(&req->location, &rlp, &rll)) {
            die_html(HTERR_BADREQ, 0,
                    "malformed url-encoded location", err_req_parse);
        }
    }

    /* arg3: protocol (HTTP/1.0, HTTP/1.1) */
    if (rll < 8 || memcmp(rlp, "HTTP/", 5)) {
        die_html(HTERR_BADREQ, 0,
                "bad or missing protocol information", err_req_parse);
    }

    rll -= 5;
    rlp += 5;

    /* does not support protocols with more than one major digit, one minor
     * digit
     */
    if (((c = *rlp) < '0' || *rlp++ > '9'
         || *rlp++ != '.'
         || *rlp < '0' || *rlp > '9')) {
        die_html(HTERR_BADREQ, 0,
                "bad protocol version", err_req_parse);
    }

    req->major = c - '0';
    req->minor = *rlp++ - '0';
    rll -= 3;

    if (rll) {
        /* XXX: perhaps this should only be a warning */
        die_html(HTERR_BADREQ, 0,
                "trailing characters after protocol", err_req_parse);
    }
}

/***************************************************************************
  - input will be url-encoded
  - output...

  Currently supported types:
  - GET  (simple retrieve)
  - POST (simple url-encoded data?)
  - HEAD (test for file existence)
 ***************************************************************************/

void req_serv(struct reqinfo *req)
{
    if (!(req->major == 1 && (req->minor == 0 || req->minor == 1))) {
        die_html(HTERR_BADREQ, 0,
                "unsupported protocol version", err_req_parse);
    }

    header_set_default(HTERR_OK);

    if (!lx_strscmp(&req->cmd, "GET")) {
        log_debug(DEBUG_NOISE, "GET", "servicing request");
        resp_sendfile(req);
    } else if (!lx_strscmp(&req->cmd, "POST")) {
        log_debug(DEBUG_NOISE, "POST", "servicing request");
        resp_recvfile(req);
    } else if (!lx_strscmp(&req->cmd, "PUT")) {
        log_debug(DEBUG_NOISE, "PUT", "servicing request");
        resp_recvfile(req);
    } else if (!lx_strscmp(&req->cmd, "HEAD")) {
        log_debug(DEBUG_NOISE, "HEAD", "servicing request");
        resp_headfile(req);
    } else if (!lx_strscmp(&req->cmd, "DELETE")) {
        log_debug(DEBUG_NOISE, "DELETE", "servicing request");
        resp_rmfile(req);
    } else {
        if (lx_check0(&req->cmd)) die_nomem();
        die_html(HTERR_BADREQ, req->cmd.s,
                "unsupported request type", err_req_parse);
    }

    if (lx_gdflush(gd_out)) die_outerr();

}

