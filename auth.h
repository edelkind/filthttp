/* requires: <lx_string.h> */

#ifndef _FTD_AUTH_H
#define _FTD_AUTH_H

struct validation {
    char **blacklst; int nblacklst;
    char **whitelst; int nwhitelst;
    char **blackgrp; int nblackgrp;
    char **whitegrp; int nwhitegrp;
};

struct auth_method_set {
    int (*verify)(struct passwd **, char *, char *);
    int (*validate)(struct passwd *, struct validation *);
    /*
    int (*verify)();
    int (*validate)();
    */
};


extern struct auth_method_set *auth_methods;
extern int authenticated;

/* system authentication (will be default) */
struct auth_method_set sys_auth_method_set;
extern int sys_auth_verify(struct passwd **, char *, char *);
extern int sys_auth_validate_user(struct passwd *, struct validation *);

#define AUTH_OKAY         0x0
#define AUTH_NO_SUCH_USER 0x1
#define AUTH_BAD_PASSWORD 0x2
#define AUTH_DENY_USER    0x3
#define AUTH_SYSTEM_ERR   0x4

#define AUTH_DFLT         0x0
#define AUTH_FORBID_USER  0x1
#define AUTH_FORBID_GROUP 0x2
#define AUTH_ALLOW_USER   0x3
#define AUTH_ALLOW_GROUP  0x4

#endif /* _FTD_AUTH_H */
