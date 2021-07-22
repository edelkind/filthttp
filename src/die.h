#ifndef _FTD_DIE_H
#define _FTD_DIE_H

#define DEBUG_ACCESS  0x01
#define DEBUG_INFO    0x02
#define DEBUG_NOISE   0x04
#define DEBUG_PRIVATE 0x08

#define ERRNO 0x80000000

void die_nomem(void);
void die_outerr(void);
void die_usage(void);
void die_html(int hterr, char *arg_info, char *log_info, char *user_comment);
void die_log(int err, char *arg_info, char *log_info, char *user_comment);
void log_debug(int level, char *arg_info, char *log_info);
void log_warning(int err, char *arg_info, char *log_info);

#endif /* _FTD_DIE_H */
