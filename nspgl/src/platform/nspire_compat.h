/* force-included for Ndless builds */
#ifndef NSPIRE_COMPAT_H
#define NSPIRE_COMPAT_H
#include <inttypes.h>
#include <stddef.h>
size_t malloc_usable_size(void *p);
#ifndef PRId64
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRIi64 "lli"
#endif
#endif
