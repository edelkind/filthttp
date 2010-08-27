/* include after <minimisc.h> */
#ifndef _FTD_HTTP_ERRORS_H
#define _FTD_HTTP_ERRORS_H

extern struct http_errors {
    ub2_t status;
    char *message;
} hterrors[];
#define HTERR_OK       0
#define HTERR_OK_CREAT 1
#define HTERR_OK_ACCPT 2
#define HTERR_OK_NOCNT 3
#define HTERR_BADREQ   4
#define HTERR_UNAUTH   5
#define HTERR_FORBID   6
#define HTERR_NOTFOUND 7
#define HTERR_TIMEOUT  8
#define HTERR_NEEDLEN  9
#define HTERR_SERVERR  10
#define HTERR_NOTIMPL  11

extern char *err_header_parse; /* error parsing header */
extern char *err_req_parse;    /* error parsing request */
extern char *err_bad_auth;     /* authentication error */
extern char *err_dir_index;    /* error building directory index */
extern char *err_perm_denied;  /* permission denied */
extern char *err_not_found;    /* file not found */
extern char *err_sys_generic;  /* generic system error */

#endif /* _FTD_HTTP_ERRORS_H */
