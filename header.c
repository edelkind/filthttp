#include <lx_string.h>
#include <minimisc.h>
#include <get_opts.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>

#include "datatypes.h"
#include "http_errors.h"
#include "options.h"
#include "base64.h"
#include "header.h"
#include "conf.h"
#include "auth.h"
#include "req.h"
#include "die.h"

int header_only = 0;
/***************************************************************************
  parse a full header line into a blobset.
  header is added in lower case form for case-insensitivity.
  Must be in (NAME: VALUE) format.

  use header_flush() wrapper instead.

returns:
  0 on success
  1 on error
 ***************************************************************************/

static inline int header_parse (blobset *header, lx_s *s)
{
    char *name = s->s;
    ub2_t namelen;
    long idx;
    lx_s *data = lx_new(); /* XXX: add lx_new and lx_destroy */

    if (!data) die_nomem();

    idx = lx_strindex(s, ':', 1);
    if (idx == -1 || !idx) return 1;

    namelen = (ub2_t)idx++;
    if (s->len - idx++ < 2) return 1;
    if (lx_striset(data, s->s + idx, s->len - idx)) die_nomem();

    lx_chomp_ws(data);
    if (lx_chompf_ws(data)) die_nomem();

    lx_lowers(name, namelen);
    if (blob_register(header, (ub1_t *)name, namelen, data, lx_destroy))
        die_nomem();

    return 0;
}

/***************************************************************************
  add the line to the blobset.
  see header_parse().
 ***************************************************************************/

static inline void header_flush(blobset *h, lx_s *this) {
    if (!this->s) return;
    if (header_parse(h, this)) {
        die_html(HTERR_BADREQ, 0,
                "error tokenizing header", err_header_parse);
    }
    lx_free(this);
}

/***************************************************************************
  append a header-continuation line to a header line.
  all leading and trailing whitespace will be removed.
 ***************************************************************************/

static inline void header_line_append(lx_s *line, lx_s *plus) {
    lx_chomp_ws(plus);
    if (lx_chompf_ws(plus)) die_nomem();

    if (!plus->len) {
        die_html(HTERR_BADREQ, 0,
                "bad header appendage", err_header_parse);
    }
    if (lx_strcat(line, plus)) die_nomem();
}


/***************************************************************************
  Read in all HTTP headers, adding each to blobset [header], with a key of
  the header name and a value of the header value.  All leading and trailing
  whitespace is removed from the value.

  Dies with the appropriate code on error.
 ***************************************************************************/

void header_recv (blobset *header)
{
    ub1_t match;
    ub2_t nheaders = 0;
    lx_s s       = {0};
    lx_s thishdr = {0};

    do {
        if (lx_getln(&s, gd_in, &match, MAXLINELEN))
            die_html(HTERR_BADREQ|ERRNO, 0,
                    "error on header read", err_header_parse);

        if (match & MATCH_TOOLONG) {
            die_html(HTERR_BADREQ, 0,
                    "header line too long", err_header_parse);
        }

        if (s.len < 2) break; /* fall through to error */

        lx_chop(&s, 1);
        if (!s.len || s.s[s.len-1] != '\r') {
            die_html(HTERR_BADREQ, 0,
                    "header line breaks rfc compliance", err_header_parse);
        }

        lx_chop(&s, 1);
        if (!s.len) {
            header_flush(header, &thishdr);
            lx_free(&s);
            return;
        }

        if (++nheaders > MAXHEADERS) {
            die_html(HTERR_BADREQ, 0,
                    "too many header lines", err_header_parse);
        }

        if (*s.s != '\t' && *s.s != ' ')
            header_flush(header, &thishdr);

        header_line_append(&thishdr, &s);

    } while (match);

    die_html(HTERR_BADREQ, 0,
            "premature end of headers", err_header_parse);

}

static inline int graph_check(char *s, int len) {
    while (len--) {
        if (!isgraph((int)*s++))
            return 1;
    }

    return 0;
}

static inline int print_check(char *s, int len) {
    while (len--) {
        if (!isprint((int)*s++))
            return 1;
    }

    return 0;
}

/***************************************************************************
  authenticate a connection using basic auth, gathered from the HTTP
  headers.

  The pwent structure is populated with the current user's information.

  To succeed, all of the following must be true:
    - authentication header
      - is present
      - is in the proper form for "basic auth"
        - includes "Basic ..."
        - encoded string is at least 4 characters, as the b64 minimum
        - string decodes properly
    - auth string is of the form <user>:<passphrase>
    - neither <user> nor <passphrase> is empty
    - checks pass to protect against potential subordinate lib/svc bugs
      - <username> is no longer than MAX_PWNAM_LEN
      - <passphrase> is no longer than MAX_PW_LEN (XXX: implement)
      - <user> consists only of printable, nonwhitespace characters
      - <passphrase> consists only of printable characters
    - user exists
    - the supplied password matches the user's password
    - user is not blacklisted (explicitly or implicitly by group)
    - user is whitelisted (explicitly or implicitly by group)

  Upon error, the connection is terminated with an "Unauthorized" html error
  code, with no extraneous information.  Detailed information, however, is
  included in the system log.

  returns nothing.  If it returns at all, authentication was successful.
 ***************************************************************************/

static struct validation val;

void header_init(void)
{
    /* preinitialized to 0 */
    if (OPT_BLACKLIST_USR) {
        val.blacklst  = OPT_BLACKLIST_USR->sl;
        val.nblacklst = OPT_BLACKLIST_USR->n;
    }
    if (OPT_WHITELIST_USR) {
        val.whitelst  = OPT_WHITELIST_USR->sl;
        val.nwhitelst = OPT_WHITELIST_USR->n;
    }
    if (OPT_BLACKLIST_GRP) {
        val.blackgrp  = OPT_BLACKLIST_GRP->sl;
        val.nblackgrp = OPT_BLACKLIST_GRP->n;
    }
    if (OPT_WHITELIST_GRP) {
        val.whitegrp  = OPT_WHITELIST_GRP->sl;
        val.nwhitegrp = OPT_WHITELIST_GRP->n;
    }
}

void header_auth (blobset *headers, struct passwd **p_pwent)
{
    char *user;
    lx_s authinfo = {0};
    int i;

    lx_s *value = blob_get(headers, "authorization", 13);
    if (!value) {
        die_html(HTERR_UNAUTH, 0,
                "no authentication header", err_bad_auth);
    }

    /* Basic <b64-encoded user:pass> */

    /* minimum 10: "Basic 1234..." */
    if (value->len < 10 || lx_strnlpcmp("basic ", value->s, 6)) {
        die_html(HTERR_UNAUTH, 0,
                "badly formed authentication header", err_bad_auth);
    }

    if (lx_striset(&authinfo, value->s + 6, value->len - 6)) die_nomem();

    i = (int) base64_decode(authinfo.s, authinfo.s, authinfo.len);
    if (i < 0) {
        die_html(HTERR_UNAUTH, 0,
                "bad authentication header", err_bad_auth);
    }

    authinfo.len = i;
    if (lx_strff(&authinfo, &user, ':', 1)) {
        die_html(HTERR_UNAUTH, 0,
                "bad authentication header", err_bad_auth);
    }
    if (!*user) {
        die_html(HTERR_UNAUTH, 0,
                "empty username", err_bad_auth);
    }

    if (!authinfo.len) {
        die_html(HTERR_UNAUTH, 0,
                "empty passphrase", err_bad_auth);
    }

    i -= (authinfo.len+1);
    if (i > MAX_PWNAM_LEN) {
        die_html(HTERR_UNAUTH, 0,
                "username too long", err_bad_auth);
    }

    if (graph_check(user, i)) {
        die_html(HTERR_UNAUTH, 0,
                "bad characters in username", err_bad_auth);
    }

    if (print_check(authinfo.s, authinfo.len)) {
        die_html(HTERR_UNAUTH, 0,
                "bad characters in passphrase", err_bad_auth);
    }

    if (lx_check0(&authinfo)) die_nomem();

    /* authentication first */
    switch(auth_methods->verify(p_pwent, user, authinfo.s)) {
        case AUTH_OKAY:
            break;
        case AUTH_NO_SUCH_USER:
            die_html(HTERR_UNAUTH, user,
                    "no such user", err_bad_auth);
        default: /* AUTH_BAD_PASSWORD */
            log_debug(DEBUG_PRIVATE, authinfo.s, "failed password");
            die_html(HTERR_UNAUTH, 0,
                    "incorrect password", err_bad_auth);
    }

    /* authorization second */
    switch(auth_methods->validate(*p_pwent, &val)) {
        case AUTH_ALLOW_USER:
            log_debug(DEBUG_ACCESS, user, "authentication succeeded by user");
            break;
        case AUTH_ALLOW_GROUP:
            log_debug(DEBUG_ACCESS, user, "authentication succeeded by group");
            break;
        case AUTH_FORBID_USER:
            die_html(HTERR_UNAUTH, user,
                    "blacklisted user", err_bad_auth);
        case AUTH_FORBID_GROUP:
            die_html(HTERR_UNAUTH, user,
                    "member of blacklisted group", err_bad_auth);
        default: /* default */
            die_html(HTERR_UNAUTH, user,
                    "no rules match; user denied by default", err_bad_auth);

    }

}


/***************************************************************************
  supply a formatted date of [secs] since 1970.01.01, placing it into [dest]

  [dest] is added to, not reset.
 ***************************************************************************/

static char *days[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
};

static char *months[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
};

#define MAYBE0(n) if(n < 10) lx_cadd(dest, '0')
void
header_mkdate(lx_s *dest, time_t secs)
{
    struct tm *t = gmtime(&secs);
    if (!t) die_nomem();

    if (lx_setalloc(dest, 29)) die_nomem();
    /* the remaining are now guaranteed to succeed */

    lx_striadd(dest, days[t->tm_wday], 3);
    lx_striadd(dest, ", ", 2);

    MAYBE0(t->tm_mday);
    lx_straddulong(dest, t->tm_mday, 10);
    lx_cadd(dest, ' ');

    lx_striadd(dest, months[t->tm_mon], 3);
    lx_cadd(dest, ' ');

    if (t->tm_year >= 8099) t->tm_year = 8099; /* eh */
    lx_straddulong(dest, t->tm_year + 1900, 10);
    lx_cadd(dest, ' ');

    MAYBE0(t->tm_hour);
    lx_straddulong(dest, t->tm_hour, 10);
    lx_cadd(dest, ':');

    MAYBE0(t->tm_min);
    lx_straddulong(dest, t->tm_min, 10);
    lx_cadd(dest, ':');

    MAYBE0(t->tm_sec);
    lx_straddulong(dest, t->tm_sec, 10);

    lx_striadd(dest, " GMT", 4);

}


/***************************************************************************
  dump the header blob (if it has already been initialized), then initialize
  the header blob anew.
 ***************************************************************************/

void
header_reinit(blobset **hdrsetp)
{
    blobset *hdrset = *hdrsetp;
    if (hdrset) blob_destroy(hdrset);
    hdrset = blob_new(10, 0);
    if (!hdrset) die_nomem();
    *hdrsetp = hdrset;
}

/***************************************************************************
  add a header with name of [name] (length [namelen])
  and value [v] (length [vlen]) to the header set.

  returns nothing.
  dies appropriately on out-of-memory error.
 ***************************************************************************/

void
header_set(blobset *headers,
        char *name, int namelen,
        char *v,    int vlen)
{
    lx_s *s = lx_new();

    if (!s || lx_striset(s, v, vlen)) die_nomem();

    if (blob_register(headers, (unsigned char *)name, namelen, s, lx_destroy))
        die_nomem();
}


/***************************************************************************
  default headers
    - those suitable for all connections
    - plus those used for certain specific response types

  The date should technically be added to all responses, but it is limited
  to authenticated connections here to avoid leaking unnecessary system
  information.  This web service is only useful to authenticated users
  anyway.
 ***************************************************************************/

void
header_set_default(int index)
{
    lx_s data = {0};

    /* XXX: add command-line interface to set arbitrary default headers */

    switch (index) {
        case HTERR_UNAUTH:
            if (lx_strset(&data, "Basic realm=\"") ||
                lx_stradd(&data, OPT_REALM) ||
                lx_cadd(&data, '"')) die_nomem();

            header_setstr(blob_header_send, "WWW-Authenticate",
                    data.s, data.len);
            break;
    }

    if (authenticated) {
        data.len = 0;
        header_mkdate(&data, time(0));
        header_setstr(blob_header_send, "Date",
                data.s, data.len);
    }

    lx_free(&data);
}

