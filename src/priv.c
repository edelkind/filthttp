#include <minimisc.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include "http_errors.h"
#include "die.h"


/***************************************************************************
  Drops privileges.

  - cd to user home directory
  - chroot(".")
  - set gid to user's gid
  - initialize supplementary groups
  - set uid to user's uid
 ***************************************************************************/

void priv_drop (struct passwd *pwent) {
    if (chdir(pwent->pw_dir)) {
        die_html(HTERR_UNAUTH|ERRNO, pwent->pw_dir,
                "chdir()", err_bad_auth);
    }

    if (chroot(".")) {
        die_html(HTERR_UNAUTH|ERRNO, ".",
                "chroot()", err_bad_auth);
    }

    if (setgid(pwent->pw_gid)) {
        die_html(HTERR_UNAUTH|ERRNO, pwent->pw_name,
                "setgid()", err_bad_auth);
    }

    if (initgroups(pwent->pw_name, pwent->pw_gid)) {
        die_html(HTERR_UNAUTH|ERRNO, pwent->pw_name,
                "initgroups()", err_bad_auth);
    }

    if (setuid(pwent->pw_uid)) {
        die_html(HTERR_UNAUTH|ERRNO, pwent->pw_name,
                "setuid()", err_bad_auth);
    }
}
