#ifndef WLR_VERSION_H
#define WLR_VERSION_H

#define WLR_VERSION_STR "0.21.0-dev"

#define WLR_VERSION_MAJOR 0
#define WLR_VERSION_MINOR 21
#define WLR_VERSION_MICRO 0

#define WLR_VERSION_NUM ((WLR_VERSION_MAJOR << 16) | (WLR_VERSION_MINOR << 8) | WLR_VERSION_MICRO)

int wlr_version_get_major(void);
int wlr_version_get_minor(void);
int wlr_version_get_micro(void);

#endif
