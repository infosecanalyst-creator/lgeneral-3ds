/* Per-platform config (replaces autoconf's config.h) */
#ifdef __3DS__
#include "../platform/3ds/config.h"
#else
#include "../platform/desktop/config.h"
#endif
