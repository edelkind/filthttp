/* include after: <minimisc.h>, <lx_string.h> */
#ifndef _FTD_HEADER_H
#define _FTD_HEADER_H

#include <string.h>
#include <time.h>
#include <pwd.h>

void header_set(blobset *headers,
        char *name, int namelen,
        char *v,    int vlen);

/* ease-of-use wrapper */
static inline void header_setstr(blobset *headers,
        char *name, char *v, int vlen)
{ header_set(headers, name, strlen(name), v, vlen); }


void header_init(void);
void header_auth(blobset *headers, struct passwd **p_pwent);
void header_recv(blobset *header);
void header_reinit(blobset **hdrsetp);
void header_set(blobset *headers, char *name, int namelen, char *v, int vlen);
void header_set_default(int index);
void header_mkdate(lx_s *dest, time_t secs);

extern blobset *blob_header_recv;
extern blobset *blob_header_send;

extern struct global_options {
    ub1_t header_only;
} global_flags;

#endif /* _FTD_HEADER_H */
