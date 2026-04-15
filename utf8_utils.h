/* utf8_utils.h
 *
 * Copyright (C) 2024 Dmitriy Eliseev
 * This file is part of txtFormatter.
 *
 * txtFormatter is licensed under the GNU General Public License, version 3.
 * See the LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.en.html>
 * for details.
 */
#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#include <stddef.h>

/* Check if byte is a valid UTF-8 continuation byte (10xxxxxx). */
static inline int utf8_is_continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

/* Return byte length of a UTF-8 character starting at s.
 * Validates continuation bytes. Returns 0 for NULL/empty,
 * 1-4 for valid sequences, 1 for invalid/overlong/orphan bytes. */
static inline int utf8_char_len(const char* s)
{
    if (s == NULL || *s == '\0') return 0;
    unsigned char c = (unsigned char)*s;
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) {
        /* 2-byte: check continuation */
        if (s[1] != '\0' && utf8_is_continuation((unsigned char)s[1])) return 2;
        return 1;  /* orphan start byte */
    }
    if ((c & 0xF0) == 0xE0) {
        /* 3-byte: check both continuations */
        if (s[1] != '\0' && utf8_is_continuation((unsigned char)s[1]) &&
            s[2] != '\0' && utf8_is_continuation((unsigned char)s[2])) return 3;
        return 1;  /* incomplete sequence */
    }
    if ((c & 0xF8) == 0xF0) {
        /* 4-byte: check all three continuations */
        if (s[1] != '\0' && utf8_is_continuation((unsigned char)s[1]) &&
            s[2] != '\0' && utf8_is_continuation((unsigned char)s[2]) &&
            s[3] != '\0' && utf8_is_continuation((unsigned char)s[3])) return 4;
        return 1;  /* incomplete sequence */
    }
    return 1;  /* orphan continuation byte or invalid start */
}

/* Return number of UTF-8 characters (visual width).
 * Returns 0 for NULL or empty string. */
static inline size_t utf8_strlen(const char* s)
{
    if (s == NULL) return 0;
    size_t count = 0;
    while (*s) {
        int len = utf8_char_len(s);
        if (len == 0) len = 1;
        s += len;
        count++;
    }
    return count;
}

#endif /* UTF8_UTILS_H */
