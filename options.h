/* include after: <get_opts.h> */
#ifndef FTD_OPTIONS_H
#define FTD_OPTIONS_H

extern opt
    opt_d,
    opt_a,
    opt_I,
    opt_p,
    opt_m,
    opt_U,
    opt_u,
    opt_G,
    opt_g,
    opt_S,
#ifdef USE_PAM
    opt_P,
#endif
    opt_w,
    opt_r,
    opt_R;


#define OPT_INDICES opt_I.v_opt.opt_toggle
#define OPT_DEBUG   opt_d.v_opt.opt_int
#define OPT_ATOMIC  opt_a.v_opt.opt_toggle
#define OPT_REALM   opt_R.v_opt.opt_string
#define OPT_MODE    opt_m.v_opt.opt_int
#define OPT_MKPATH  opt_p.v_opt.opt_toggle
#define OPT_WRITABL opt_w.v_opt.opt_toggle
#define OPT_READABL opt_r.v_opt.opt_toggle

#define OPT_BLACKLIST_USR opt_U.v_opt.opt_stringlist
#define OPT_WHITELIST_USR opt_u.v_opt.opt_stringlist
#define OPT_BLACKLIST_GRP opt_G.v_opt.opt_stringlist
#define OPT_WHITELIST_GRP opt_g.v_opt.opt_stringlist

#endif /* FTD_OPTIONS_H */
