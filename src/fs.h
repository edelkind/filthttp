#ifndef _FTD_FS_H
#define _FTD_FS_H

struct fsfd {
    int fd;
    char *dest;    /* filename opened for writing  */
    char *target;  /* end target for rename (or 0) */
    unsigned status;
};

#define ST_INCOMPLETE 0x1
#define ST_CREATED    0x2  /* XXX: implement per RFC */

void fs_ready(void);
void fs_parseout(lx_s *dest_dir, lx_s *dest_file, lx_s *src_location);
void fs_chdir_write(char *path);
void fs_open_file_write(struct fsfd *h, char *name, unsigned namelen);
void fs_close_file_write(struct fsfd *h);
void fs_slurp_gd2h(struct fsfd *h, lx_gd *gd, unsigned len);
void fs_delete(char *path);

#define HT_DIR_BASE "/.htftd"
#define HT_DIR_LOCK HT_DIR_BASE "/locks"

#define HT_FILE_THRTL HT_DIR_BASE "/throttle"

#endif /* _FTD_FS_H */
