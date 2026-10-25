/* config.h for the Nintendo 3DS build of LGeneral (hand-written, replaces autoconf output) */
#ifndef LG3DS_CONFIG_H
#define LG3DS_CONFIG_H

#define PACKAGE "lgeneral"
#define PACKAGE_NAME "lgeneral"
#define VERSION "1.4.4-3ds"

/* no gettext on 3DS: ENABLE_NLS stays undefined so tr() is a no-op */
#define HAVE_SETENV 1

#endif
