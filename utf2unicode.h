#ifndef UTF2UNICODE_H
#define UTF2UNICODE_H
#include <wchar.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

wchar_t* utf8_to_unicode(const uint8_t* utf8, int length, wchar_t* wcs, int* size);

#ifdef __cplusplus
}
#endif


#endif
