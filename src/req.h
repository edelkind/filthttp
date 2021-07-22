/* requires: <lx_string.h> */
#ifndef _FTD_REQ_H
#define _FTD_REQ_H

struct reqinfo {
    lx_s cmd;
    lx_s location;
    unsigned short major;
    unsigned short minor;
};

void req_recv (lx_s *req);
void req_parse(struct reqinfo *req, lx_s *reqline);
void req_serv(struct reqinfo *req);


/* stages */
#define SENT_PREFIX  0x1
#define SENT_HEADER  0x2

extern unsigned stage;

#endif /* _FTD_REQ_H */
