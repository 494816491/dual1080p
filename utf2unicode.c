#include "utf2unicode.h"
#include <string.h>
#include <stdlib.h>
//#include "hi_mem.h"

// Unicode
//==============================================================================
/*
Bits | First     | Last       | Bytes | Byte 1   | Byte 2   | Byte 3   | Byte 4   | Byte 5   | Byte 6   
-----|-----------|------------|-------|----------|----------|----------|----------|----------|----------
  7  | U+0000    | U+007F     | 1     | 0xxxxxxx |          |          |          |          |          
 11  | U+0080    | U+07FF     | 2     | 110xxxxx | 10xxxxxx |          |          |          |          
 16  | U+0800    | U+FFFF     | 3     | 1110xxxx | 10xxxxxx | 10xxxxxx |          |          |          
 21  | U+10000   | U+1FFFFF   | 4     | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |          |          
 26  | U+200000  | U+3FFFFFF  | 5     | 111110xx | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |          
 31  | U+4000000 | U+7FFFFFFF | 6     | 1111110x | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx 
*/

static int UTF8C_UTF16C(const uint8_t* utf8, int length, uint16_t* utf16, int size)
{
    uint32_t v = 0;
    int retval = 1;
    switch (length) {
    case 1:
        *utf16 = *utf8 & 0x7f;
        break;
    case 2:
        v = utf8[0] & 0x1f;
        v <<= 6;
        v |= utf8[1] & 0x3f;
        *utf16 = v;
        break;
    case 3:
        v   = utf8[0] & 0x0f;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        *utf16 = v;
        break;
    case 4:
        v   = utf8[0] & 0x07;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        v  -= 0x10000;
        *utf16++ = 0xD800 | ((v >> 10) & 0x3ff);
        *utf16++ = 0xDC00 | (v & 0x3ff);
        retval = 2;
        break;
    case 5:
        v   = utf8[0] & 0x03;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        v <<= 6;
        v  |= utf8[4] & 0x3f;
        v  -= 0x10000;
        *utf16++ = 0xD800 | ((v >> 10) & 0x3ff);
        *utf16++ = 0xDC00 | (v & 0x3ff);
        retval = 2;
        break;
    case 6:
        v   = utf8[0] & 0x01;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        v <<= 6;
        v  |= utf8[4] & 0x3f;
        v <<= 6;
        v  |= utf8[5] & 0x3f;
        v  -= 0x10000;
        *utf16++ = 0xD800 + ((v >> 10) & 0x3ff);
        *utf16++ = 0xDC00 + (v & 0x3ff);
        retval = 2;
        break;
    default:
        retval = 0;
        break;
    }
    return retval;
}


static int UTF8C_UTF32C(const uint8_t* utf8, int length, uint32_t* utf32, int size)
{
    uint32_t v = 0;
    int retval = 1;
    switch (length) {
    case 1:
        *utf32 = *utf8 & 0x7f;
        break;
    case 2:
        v = utf8[0] & 0x1f;
        v <<= 6;
        v |= utf8[1] & 0x3f;
        *utf32 = v;
        break;
    case 3:
        v   = utf8[0] & 0x0f;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        *utf32 = v;
        break;
    case 4:
        v   = utf8[0] & 0x07;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        *utf32 = v;
        break;
    case 5:
        v   = utf8[0] & 0x03;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        v <<= 6;
        v  |= utf8[4] & 0x3f;
        //v  -= 0x10000;
        *utf32 = v;
        break;
    case 6:
        v   = utf8[0] & 0x01;
        v <<= 6;
        v  |= utf8[1] & 0x3f;
        v <<= 6;
        v  |= utf8[2] & 0x3f;
        v <<= 6;
        v  |= utf8[3] & 0x3f;
        v <<= 6;
        v  |= utf8[4] & 0x3f;
        v <<= 6;
        v  |= utf8[5] & 0x3f;
        //v  -= 0x10000;
        *utf32 = v;
        break;
    default:
        retval = 0;
        break;
    }
    return retval;
}


static int UTF16C_UTF8C(const uint16_t* utf16, int length, uint8_t* utf8, int size)
{
    uint32_t v = 0;
    int retval = 0;
    if (length == 2) {
        v   = (utf16[0] - 0xD800) & 0x3ff;
        v <<= 10;
        v  |= (utf16[1] - 0xDC00) & 0x3ff;
        v  += 10000;
    }
    else {
        v = *utf16;
    }
    if (v <= 0x7F) {
        *utf8 = v;
        retval = 1;
    }
    else if (v <= 0x7FF) {
        utf8[0] = 0xC0 | ((v >> 6) & 0x1F);
        utf8[1] = 0x80 | (v & 0x3F);
        retval = 2;
    }
    else if (v <= 0xFFFF) {
        utf8[0] = 0xE0 | ((v >> 12) & 0x0F);
        utf8[1] = 0x80 | ((v >> 6) & 0x3F);
        utf8[2] = 0x80 | (v & 0x3F);
        retval = 3;
    }
    else if (v <= 0x1FFFF) {
        utf8[0] = 0xF0 | ((v >> 18) & 0x07);
        utf8[1] = 0x80 | ((v >> 12) & 0x3F);
        utf8[2] = 0x80 | ((v >> 6) & 0x3F);
        utf8[3] = 0x80 | (v & 0x3F);
        retval = 4;
    }
    else if (v <= 0x3FFFFFF) {
        utf8[0] = 0xF8 | ((v >> 24) & 0x03);
        utf8[1] = 0x80 | ((v >> 18) & 0x3F);
        utf8[2] = 0x80 | ((v >> 12) & 0x3F);
        utf8[3] = 0x80 | ((v >> 6) & 0x3F);
        utf8[4] = 0x80 | (v & 0x3F);
        retval = 5;
    }
    else if (v <= 0x7FFFFFFF) {
        utf8[0] = 0xFC | ((v >> 30) & 0x01);
        utf8[1] = 0x80 | ((v >> 24) & 0x3F);
        utf8[2] = 0x80 | ((v >> 18) & 0x3F);
        utf8[3] = 0x80 | ((v >> 12) & 0x3F);
        utf8[4] = 0x80 | ((v >> 6) & 0x3F);
        utf8[5] = 0x80 | (v & 0x3F);
        retval = 6;
    }
    return retval;
}

uint16_t* utf8_to_utf16(const uint8_t* utf8, int length, uint16_t* utf16, int* size)
{
    int n8 = 0;
    int n16 = 0;
    int u16size = 0;
    int u16len = 0;
    uint8_t c = 0;
    uint16_t* u16 = 0;
    //
    if (length < 0)
        length = strlen((const char*)utf8);
    //
    if (!utf16) {
        u16size = length+1;
        utf16 = malloc(sizeof(uint16_t) * u16size);
    }
    else {
        if (size)
            u16size = *size;
    }
    //
    u16 = utf16;
    if (!u16 || u16size < 1)
        return 0;
    //
    while (length > 0) {
        c = *utf8;
        if (c == 0x00)
            break;
        if ((c & 0xC0) == 0x80) {
            n8 = 1;
            goto next;
        }
        if (c <= 0x7F) {
            n8 = 1;
            n16 = UTF8C_UTF16C(utf8, 1, utf16, u16size);
        }
        else if ((c & 0xE0) == 0xC0) {
            n8 = 2;
            n16 = UTF8C_UTF16C(utf8, 2, utf16, u16size);
        }
        else if ((c & 0xF0) == 0xE0) {
            n8 = 3;
            n16 = UTF8C_UTF16C(utf8, 3, utf16, u16size);
        }
        else if ((c & 0xF8) == 0xF0) {
            n8 = 4;
            n16 = UTF8C_UTF16C(utf8, 4, utf16, u16size);
        }
        else if ((c & 0xFC) == 0xF8) {
            n8 = 5;
            n16 = UTF8C_UTF16C(utf8, 5, utf16, u16size);
        }
        else if ((c & 0xFE) == 0xFC) {
            n8 = 6;
            n16 = UTF8C_UTF16C(utf8, 6, utf16, u16size);
        }
        //
        utf16   += n16;
        u16len  += n16;
        u16size -= n16;
next:
        utf8   += n8;
        length -= n8;
    }
    // Make NULL terminated
    *utf16 = 0;
    //
    if (size)
        *size = u16len;
    //
    return u16;
}


uint32_t* utf8_to_utf32(const uint8_t* utf8, int length, uint32_t* utf32, int* size)
{
    int n8 = 0;
    int n32 = 0;
    int u32len = 0;
    int u32size = 0;
    uint8_t c = 0;
    uint32_t* u32 = 0;
    //
    if (length < 0)
        length = strlen((const char*)utf8);
    //
    if (!utf32) {
        u32size = length+1;
        utf32 = malloc(sizeof(uint32_t) * u32size);
    }
    else {
        if (size)
            u32size = *size;
    }
    //
    u32 = utf32;
    //
    while (length > 0) {
        c = *utf8;
        if (c == 0x00)
            break;
        if ((c & 0xC0) == 0x80) {
            n8 = 1;
            goto next;
        }
        if (c <= 0x7F) {
            n8 = 1;
            n32 = UTF8C_UTF32C(utf8, 1, utf32, u32size);
        }
        else if ((c & 0xE0) == 0xC0) {
            n8 = 2;
            n32 = UTF8C_UTF32C(utf8, 2, utf32, u32size);
        }
        else if ((c & 0xF0) == 0xE0) {
            n8 = 3;
            n32 = UTF8C_UTF32C(utf8, 3, utf32, u32size);
        }
        else if ((c & 0xF8) == 0xF0) {
            n8 = 4;
            n32 = UTF8C_UTF32C(utf8, 4, utf32, u32size);
        }
        else if ((c & 0xFC) == 0xF8) {
            n8 = 5;
            n32 = UTF8C_UTF32C(utf8, 5, utf32, u32size);
        }
        else if ((c & 0xFE) == 0xFC) {
            n8 = 6;
            n32 = UTF8C_UTF32C(utf8, 6, utf32, u32size);
        }
        //
        utf32   += n32;
        u32len  += n32;
        u32size -= n32;
next:
        utf8   += n8;
        length -= n8;
    }
    //
    *utf32 = 0;
    //
    if (size)
        *size = u32len;
    //
    return u32;
}

wchar_t* utf8_to_unicode(const uint8_t* utf8, int length, wchar_t* wcs, int* size)
{
    if (sizeof(wchar_t) == sizeof(uint16_t))
        return (wchar_t*)utf8_to_utf16(utf8, length, (uint16_t*)wcs, size);
    else if (sizeof(wchar_t) == sizeof(uint32_t))
        return (wchar_t*)utf8_to_utf32(utf8, length, (uint32_t*)wcs, size);
    return 0;
}
