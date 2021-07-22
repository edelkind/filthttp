#ifndef _FTD_CONF_H
#define _FTD_CONF_H

/* keep it all under USHRT_MAX, okay? */
#define BLOCKSIZE  8192
#define MAXLINELEN 1024
#define MAXHEADERS 120
#define MAX_PWNAM_LEN 16
#define MAX_PW_LEN 32

/* XXX: replace other instances of "\r\n" with CRLF */
#define CRLF "\r\n"

/* #define USE_GETDENTS 1 */

#endif /* _FTD_CONF_H */
