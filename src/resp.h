/* requires: <lx_string.h>, <minimisc.h>, "req.h" */
#ifndef _FTD_RESP_H
#define _FTD_RESP_H

void resp_sendprefix(struct reqinfo *req, ub2_t index);
void resp_sendheaders(struct reqinfo *req, blobset *headers);
void resp_sendfile(struct reqinfo *req);
void resp_recvfile(struct reqinfo *req);
void resp_headfile(struct reqinfo *req);
void resp_rmfile(struct reqinfo *req);

void index_generate(int fd, void *void_st, struct reqinfo *req, lx_s *body);

#endif /* _FTD_RESP_H */
