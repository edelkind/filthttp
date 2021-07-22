#define _XOPEN_SOURCE

#include <crypt.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <lx_string.h>

#include "auth.h"
#include "die.h"

struct auth_method_set sys_auth_method_set = {
    sys_auth_verify,
    sys_auth_validate_user,
};


/***************************************************************************
  authenticate a user, given a null-terminated username and password.
  Populates *p_pwent with the user's pwent structure.

  This function is non-reentrant, and further calls to getpwent(3) may well
  overwrite the memory to which *p_pwent will point.  That's okay in this case,
  because there will be no other calls to getpwent(3) in this process.

Returns:
  AUTH_OKAY on success
  AUTH_NO_SUCH_USER if user does not exist
  AUTH_BAD_PASSWORD if user exists, but the password does not match.
 ***************************************************************************/

int sys_auth_verify(struct passwd **p_pwent, char *user, char *pass)
{
    struct passwd *pwent;
    char *cpasswd;

    pwent = getpwnam(user);
    if (!pwent)
        return AUTH_NO_SUCH_USER;

    cpasswd = crypt(pass, pwent->pw_passwd);

    if (strcmp(pass, pwent->pw_passwd))
        return AUTH_BAD_PASSWORD;


    *p_pwent = pwent;
    return AUTH_OKAY;
}

/***************************************************************************
  find the null-terminated string [nam] in the list of null-terminated
  strings [sl], which has [sn] elements.

  returns 0 if found; 1 otherwise.
 ***************************************************************************/

static inline int seek_stringlist(char **sl, int sn, char *nam)
{
    for (;sn;sn--) {
        log_debug(DEBUG_NOISE, *sl, "comparing user data");
        if (!memcmp(*sl++, nam, strlen(nam)+1)) {   /* include '\0' */
            return 0;
        }
    }

    return 1;
}


/***************************************************************************
  [gl] is a list of null-terminated groups to look up on the system.  Each
  group will contain a list of usernames, through which [nam] will be
  sought.

  returns 0 if found; 1 otherwise.
 ***************************************************************************/

static inline int seek_grouplist(char **gl, int gn, char *nam)
{
    struct group *grp;
    char **pp;

    for (;gn;gn--) {
        grp = getgrnam(*gl++);
        if (!grp) {
            log_debug(DEBUG_INFO, *(gl-1), "no such group (ignoring)");
            continue;
        }

        pp = grp->gr_mem;
        while (*pp) pp++;

        log_debug(DEBUG_NOISE, nam, "seeking group");

        if (!seek_stringlist(grp->gr_mem,
                    (pp - (char**)grp->gr_mem), nam))
            return 0;
    }

    return 1;
}


/***************************************************************************
  Ensures that a user is allowed access before accepting login.

  [pwent] is the passwd structure associated with the user.  [val] is the
  populated validation structure.

  Priority is as follows:
    1. user is explicitly blacklisted (deny)
    2. user is explicitly whitelisted (allow)
    3. user is a member of a group that is blacklisted (deny)
    4. user is a member of a group that is whitelisted (allow)
    5. default action (deny)

  Return values:
    AUTH_ALLOW_USER   - user is explicitly allowed
    AUTH_FORBID_USER  - user is explicitly denied
    AUTH_ALLOW_GROUP  - user is implicitly allowed by group
    AUTH_FORBID_GROUP - user is implicitly denied by group
    AUTH_DFLT         - user is not referenced explicitly or implicitly.
 ***************************************************************************/

int sys_auth_validate_user(struct passwd *pwent, struct validation *val)
{
    if (!seek_stringlist(val->blacklst, val->nblacklst, pwent->pw_name))
        return AUTH_FORBID_USER;
    if (!seek_stringlist(val->whitelst, val->nwhitelst, pwent->pw_name))
        return AUTH_ALLOW_USER;

    if (!seek_grouplist(val->blackgrp, val->nblackgrp, pwent->pw_name))
        return AUTH_FORBID_GROUP;
    if (!seek_grouplist(val->whitegrp, val->nwhitegrp, pwent->pw_name))
        return AUTH_ALLOW_GROUP;

    return AUTH_DFLT;
}
