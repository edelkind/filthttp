#include <errno.h>
#include <get_opts.h>
#include <lx_string.h>
#include <minimisc.h>
#include <unistd.h>
#include <pwd.h>

#include "datatypes.h"
#include "http_errors.h"
#include "options.h"
#include "header.h"
#include "auth.h"
#include "priv.h"
#include "conf.h"
#include "req.h"
#include "die.h"


add_opt(opt_d, "d", "debug",           OPT_INT, (opt_int_t)0, 0);
add_opt(opt_a, "a", "atomic",          OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_I, "I", "indices",         OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_p, "p", "mkpath",          OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_m, "m", "mode",            OPT_INT, (opt_int_t)0660, 0);
add_opt(opt_U, "U", "blacklist-user",  OPT_STRINGLIST, (opt_stringlist_t)0, 0);
add_opt(opt_u, "u", "whitelist-user",  OPT_STRINGLIST, (opt_stringlist_t)0, 0);
add_opt(opt_G, "G", "blacklist-group", OPT_STRINGLIST, (opt_stringlist_t)0, 0);
add_opt(opt_g, "g", "whitelist-group", OPT_STRINGLIST, (opt_stringlist_t)0, 0);
/* send session id in error message output to ease support (implications) */
add_opt(opt_S, "S", "advertise-sessid", OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_r, "r", "read",            OPT_TOGGLE, (opt_toggle_t)1, 0);
add_opt(opt_w, "w", "write",           OPT_TOGGLE, (opt_toggle_t)1, 0);

#ifdef USE_PAM
static int opt_func_use_pam(opt *optp) {
    auth_methods = &pam_auth_method_set;
    return 0;
}
add_opt(opt_P, "P", "use-pam", OPT_BOOL, (opt_bool_t)0, opt_func_use_pam);
#endif

add_opt(opt_R, "R", "realm", OPT_STRING, (opt_string_t)"access", 0);

put_opts(options, &opt_d, &opt_a, &opt_I, &opt_p, &opt_m,
        &opt_U, &opt_u, &opt_G, &opt_g, &opt_r, &opt_w,
#ifdef USE_PAM
        &opt_P,
#endif
        &opt_R);


lx_gd _gd_in,
      _gd_out;

lx_gd *gd_in  = &_gd_in,
      *gd_out = &_gd_out;

blobset *blob_header_send = 0;
blobset *blob_header_recv = 0;

char *err_header_parse = "There was an error parsing your header information.";
char *err_req_parse    = "There was an error parsing your request.";
char *err_bad_auth     = "Missing or incorrect user authentication.";
char *err_dir_index    = "Error building directory index.";
char *err_perm_denied  = "Permission denied.";
char *err_not_found    = "File not found.";
char *err_sys_generic  = "There was an error processing your request.";

/* default system authentication */
struct auth_method_set *auth_methods = &sys_auth_method_set;

int authenticated = 0;
unsigned stage    = 0;
unsigned sessionid;

#include <stdio.h>
int main(argc, argv)
    int argc;
    char **argv;
{
    static struct reqinfo req; /* 0 initialized */
    lx_s reqline = {0};
    char **args;
    int argn;

    /* non-reentrant usage */
    struct passwd *pwent;

    sessionid = getpid();

    if (lx_gdnew(gd_in,  0, BLOCKSIZE)) die_nomem();
    if (lx_gdnew(gd_out, 1, BLOCKSIZE)) die_nomem();

    argn = get_opts_errormatic(&args, argv+1, argc-1, options);
    if (argn) die_usage();

    header_init();

    req_recv(&reqline);

    header_reinit(&blob_header_recv);

    header_recv(blob_header_recv);

    header_auth(blob_header_recv, &pwent);

    priv_drop(pwent);

    authenticated = 1;

    req_parse(&req, &reqline);

    header_reinit(&blob_header_send);

    req_serv (&req);

    return 0;

}
