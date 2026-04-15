#define _POSIX_C_SOURCE 200809L
/* tag_handler.c
 *
 * Copyright (C) 2024 Dmitriy Eliseev
 * This file is part of txtFormatter.
 *
 * txtFormatter is licensed under the GNU General Public License, version 3.
 * See the LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.en.html>
 * for details.
 */
#include "tag_handler.h"
#include "utf8_utils.h"

/* Tag list — must stay in sync with tag_functions array and single_tag_mask */
char    tag_list[][20]                  =  { "right", "center", "p", "frame",
                                             "list", "lines", "histogram",
                                             "table", "calc", "sep", "h1", "h2",
                                             "h3", "h4", "insert", "doc_width",
                                             "default_width", "date", "time",
                                             "datetime"  };
const int tag_count = sizeof(tag_list) / sizeof(tag_list[0]);

/* Binary search lookup: indices of tag_list sorted alphabetically */
static const int tag_sorted_idx[] = { 8, 1, 17, 19, 16, 15, 3, 10, 11, 12,
                                       13, 6, 14, 5, 4, 2, 0, 9, 7, 18 };

char* (*tag_functions[])(char*, char**) = { right, center, p, get_framed_text,
                                            get_list, get_lines, get_histogram,
                                            get_table, calc, separator, h1, h2,
                                            h3, h4, insert, tag_doc_width,
                                            def_width, get_date, get_time,
                                            get_datetime };

/* Tags that don't have closing pairs (e.g. <date>, <sep>, <insert>) */
char single_tags[][20] = { "date", "time", "datetime", "doc_width",
                           "default_width", "sep", "lines", "insert" };


/***************************************************************************
* Tag Attribute Functions
***************************************************************************/

int have_attributes(char* tag)
{
    size_t elements_count = get_elements_count(' ', tag);
    if (elements_count <= 1)
        return 0;
    return (int)elements_count;
}

char** get_tag_attributes(char* tag)
{
    int attr_count = have_attributes(tag);
    if (attr_count == 0)
        return NULL;

    char** attrs = calloc((size_t)attr_count, sizeof(char*));
    is_memory_allocated(attrs);

    /* Find start of first attribute (skip tag name and spaces) */
    const char* p = tag;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    for (int i = 0; i < attr_count - 1; i++) {
        const char* start = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        attrs[i] = malloc(len + 1);
        is_memory_allocated(attrs[i]);
        memcpy(attrs[i], start, len);
        attrs[i][len] = '\0';
        while (*p == ' ') p++;
    }
    attrs[attr_count - 1] = NULL;
    return attrs;
}

/* Extract tag name (text before first space). Returns newly allocated string. */
char* get_tag_name(char* tag)
{
    char* sp = strchr(tag, ' ');
    if (sp) {
        size_t len = (size_t)(sp - tag);
        char* res = calloc(len + 1, sizeof(char));
        is_memory_allocated(res);
        memcpy(res, tag, len);
        res[len] = '\0';
        return res;
    }
    return strdup(tag);
}

/* Check if tag is valid without allocating memory.
 * Returns tag index or -1. Uses ptr + len to avoid copying.
 * Note: len may include attributes (e.g. "list *" len=6) — we search only the tag name part. */
static int is_valid_tag_nocopy(const char* tag, size_t len)
{
    /* Find the tag name — stop at first space (attributes follow) */
    size_t name_len = 0;
    while (name_len < len && tag[name_len] != ' ') name_len++;

    /* Trim leading spaces from tag name */
    while (name_len > 0 && tag[0] == ' ') { tag++; name_len--; }
    /* Trim trailing spaces from tag name */
    while (name_len > 0 && tag[name_len - 1] == ' ') name_len--;

    if (name_len == 0) return -1;

    /* Binary search on sorted tag list using memcmp */
    int lo = 0, hi = tag_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int orig = tag_sorted_idx[mid];
        size_t orig_len = strlen(tag_list[orig]);
        size_t cmp_len = orig_len < name_len ? orig_len : name_len;
        int cmp = memcmp(tag_list[orig], tag, cmp_len);
        if (cmp == 0) {
            if (orig_len == name_len) return orig;
            cmp = (orig_len < name_len) ? -1 : 1;
        }
        if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int is_valid_tag(char* tag)
{
    char* tag_name = get_tag_name(tag);
    /* Binary search on sorted tag list */
    int lo = 0, hi = tag_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int orig = tag_sorted_idx[mid];
        int cmp = strcmp(tag_list[orig], tag_name);
        if (cmp == 0) {
            free(tag_name);
            return orig;
        } else if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    free(tag_name);
    return -1;
}

/* Bitmask for single tags (no closing pair) by index in tag_list.
 * Indices: lines(5), sep(9), insert(14), doc_width(15), default_width(16),
 *          date(17), time(18), datetime(19).
 * Must be updated when tag_list order changes. */
static const uint32_t single_tag_mask =
    (1U << 5) |  /* lines */
    (1U << 9) |  /* sep */
    (1U << 14) | /* insert */
    (1U << 15) | /* doc_width */
    (1U << 16) | /* default_width */
    (1U << 17) | /* date */
    (1U << 18) | /* time */
    (1U << 19);  /* datetime */

/* Static assertion: ensure mask is wide enough for all 20 tags */
_Static_assert(sizeof(single_tag_mask) * 8 >= 20,
    "single_tag_mask must have enough bits for all tags");

int is_single_tag(char* tag)
{
    char* tag_name = get_tag_name(tag);
    int idx = is_valid_tag(tag_name);
    free(tag_name);
    if (idx < 0) return 0;
    return (single_tag_mask >> idx) & 1;
}


/***************************************************************************
* Core Tag Parsing Functions
***************************************************************************/

/* Find the closing tag matching an opening tag, tracking nesting depth.
 * Returns pointer to '<' of closing tag or NULL. */
static char* find_matching_close(const char* str, const char* open_tag_start,
                                  const char* tag_name)
{
    size_t name_len = strlen(tag_name);
    size_t str_len = strlen(str);

    const char* pos = open_tag_start + 1;
    int depth = 1;
    char first = tag_name[0];

    while (*pos && depth > 0) {
        if (*pos == '<') {
            /* Safety: check remaining length before reading pos[1], pos[2] */
            size_t remaining = str_len - (size_t)(pos - str);
            if (remaining < 2) {
                /* Not enough chars for '</' or '<tag' — skip */
                pos = strchr(pos + 1, '<');
                if (!pos) break;
                continue;
            }

            unsigned char c1 = (unsigned char)pos[1];
            if (c1 == '\0') {
                pos = strchr(pos + 1, '<');
                if (!pos) break;
                continue;
            }

            /* Check for closing tag </tagname> */
            if (c1 == '/') {
                unsigned char c2 = (remaining >= 3) ? (unsigned char)pos[2] : '\0';
                if (c2 == '\0' || c2 != (unsigned char)first) {
                    /* Not a matching close tag, continue scanning */
                } else {
                    ptrdiff_t offset = pos + 2 - str;
                    if (offset >= 0 && (size_t)offset <= str_len) {
                        size_t rem = str_len - (size_t)offset;
                        /* Need name_len + 1 bytes: name + char after */
                        if (rem >= name_len + 1 &&
                            strncmp(pos + 2, tag_name, name_len) == 0) {
                            char after = pos[2 + name_len];
                            if (after == '>' || after == ' ' || after == '\t' || after == '\0') {
                                depth--;
                                if (depth == 0)
                                    return (char*)pos;
                            }
                        }
                    }
                }
            } else {
                /* Check for opening tag <tagname> of the same type */
                ptrdiff_t offset = pos + 1 - str;
                if (offset >= 0 && (size_t)offset <= str_len) {
                    size_t rem = str_len - (size_t)offset;
                    /* Need name_len + 1 bytes: name + char after */
                    if (rem >= name_len + 1 &&
                        strncmp(pos + 1, tag_name, name_len) == 0) {
                        char after = pos[1 + name_len];
                        if (after == '>' || after == ' ' || after == '\t' || after == '\0') {
                            depth++;
                        }
                    }
                }
            }
        }
        /* Skip to next '<' for efficiency */
        pos = strchr(pos + 1, '<');
        if (!pos) break;
    }

    return NULL;
}

/* Find the first valid opening tag '<tagname>' in the string.
 * Returns pointer to '<' of the tag or NULL. */
static char* find_first_open_tag(const char* str)
{
    const char* p = str;

    while (*p) {
        if (*p == '<' && p[1] != '/') {
            const char* end = strchr(p, '>');
            if (end) {
                size_t len = (size_t)(end - p - 1);
                if (len > 0 && len < 256) {
                    /* Skip leading/trailing spaces without copying */
                    const char* start = p + 1;
                    const char* trim_end = end - 1;
                    while (start < trim_end && *start == ' ') start++;
                    while (trim_end > start && *trim_end == ' ') trim_end--;
                    size_t trimmed_len = (size_t)(trim_end - start + 1);

                    if (is_valid_tag_nocopy(start, trimmed_len) != -1)
                        return (char*)p;
                }
            }
        }
        /* Skip to next '<' for efficiency */
        p = strchr(p + 1, '<');
        if (!p) break;
    }
    return NULL;
}


char* get_tag(const char* str)
{
    char* tag_pos = find_first_open_tag(str);
    if (tag_pos == NULL)
        return NULL;

    /* Find closing '>' */
    char* end = strchr(tag_pos + 1, '>');
    if (end == NULL)
        return NULL;

    /* Extract tag content between < and > */
    size_t len = (size_t)(end - tag_pos - 1);
    char* tag = (char*)malloc(len + 1);
    is_memory_allocated(tag);
    memcpy(tag, tag_pos + 1, len);
    tag[len] = '\0';

    return tag;
}

char* get_tag_content(char* str, char* tag)
{
    char* tag_content = NULL;
    /* Single tags have no content */
    if (is_single_tag(tag) != 0) {
        tag_content = (char*)calloc(2, sizeof(char));
        is_memory_allocated(tag_content);
        strcpy(tag_content, " ");
        return tag_content;
    }

    /* Get tag name */
    char* tag_name = get_tag_name(tag);

    /* Find opening tag */
    char* tag_pos = find_first_open_tag(str);
    if (tag_pos == NULL) {
        free(tag_name);
        /* Return empty string instead of special character */
        char* result = (char*)calloc(1, sizeof(char));
        is_memory_allocated(result);
        result[0] = '\0';
        return result;
    }

    /* Find matching closing tag */
    char* close_pos = find_matching_close(str, tag_pos, tag_name);
    if (close_pos == NULL) {
        printf("  Warning: tag <%s> has no closing tag — content used as-is\n",
               tag_name);
        free(tag_name);
        /* Return all text after the opening tag as content */
        char* open_end = strchr(tag_pos, '>');
        if (open_end != NULL && open_end[1] != '\0') {
            char* result = strdup(open_end + 1);
            is_memory_allocated(result);
            return result;
        }
        /* Nothing after the tag */
        char* result = (char*)calloc(1, sizeof(char));
        is_memory_allocated(result);
        result[0] = '\0';
        return result;
    }

    /* Extract content between opening and closing tags */
    char* open_end = strchr(tag_pos, '>');
    if (open_end == NULL || open_end + 1 > close_pos) {
        free(tag_name);
        /* Empty content — return empty string, not special character */
        char* result = (char*)calloc(1, sizeof(char));
        is_memory_allocated(result);
        result[0] = '\0';
        return result;
    }

    const char* content_start = open_end + 1;
    size_t content_len = (size_t)(close_pos - content_start);

    char* content = (char*)calloc(content_len + 1, sizeof(char));
    is_memory_allocated(content);
    memcpy(content, content_start, content_len);
    content[content_len] = '\0';

    /* Remove leading newline if present */
    if (content[0] == '\n') {
        char* tmp = strdup(&content[1]);
        is_memory_allocated(tmp);
        free(content);
        content = tmp;
    }

    /* Remove up to two trailing newlines if present */
    size_t clen = strlen(content);
    while (clen > 0 && content[clen - 1] == '\n' && clen > 0)
        content[--clen] = '\0';

    free(tag_name);
    return content;
}

char* get_text_before_tag(char* str, char* tag)
{
    (void)tag;
    char* tag_pos = find_first_open_tag(str);
    if (tag_pos == NULL) {
        return strdup(str);
    }

    size_t len = (size_t)(tag_pos - str);
    char* text = (char*)calloc(len + 1, sizeof(char));
    is_memory_allocated(text);
    memcpy(text, str, len);
    text[len] = '\0';
    return text;
}

char* get_text_after_tag(char* str, char* tag)
{
    char* tag_name = get_tag_name(tag);
    char* tag_pos = find_first_open_tag(str);

    if (tag_pos == NULL) {
        free(tag_name);
        return strdup(str);
    }

    /* For single tags, text after is just after the opening tag */
    if (is_single_tag(tag)) {
        char* open_end = strchr(tag_pos, '>');
        if (open_end == NULL) {
            free(tag_name);
            return strdup(str);
        }
        char* result = strdup(open_end + 1);
        free(tag_name);
        return result;
    }

    /* Find matching closing tag */
    char* close_pos = find_matching_close(str, tag_pos, tag_name);
    free(tag_name);
    
    if (close_pos == NULL) {
        /* No closing tag found — all text after opening tag was already
         * consumed as content by get_tag_content, so return empty string
         * to avoid duplication. */
        return strdup("");
    }

    /* Move past the closing '>' */
    char* after_close = strchr(close_pos, '>');
    if (after_close == NULL)
        return strdup("");

    /* Ensure at least one newline after closing tag for proper spacing
     * between processed blocks in the output document. */
    const char* rest = after_close + 1;
    if (rest[0] != '\n' && rest[0] != '\0') {
        /* Text follows the closing tag without a leading newline —
         * prepend one to keep block separation consistent. */
        size_t rest_len = strlen(rest);
        char* buf = malloc(rest_len + 2);
        is_memory_allocated(buf);
        buf[0] = '\n';
        memcpy(buf + 1, rest, rest_len + 1);
        return buf;
    }

    return strdup(rest);
}


/***************************************************************************
* Tag Execution Functions
***************************************************************************/

char* execute_tag(char* tag, char* tag_content)
{
    /* tag is already trimmed by get_tag; find first space to get tag name */
    const char* sp = strchr(tag, ' ');
    size_t name_len = sp ? (size_t)(sp - tag) : strlen(tag);
    int tag_i = is_valid_tag_nocopy(tag, name_len);

    /* Empty content means no meaningful content to process — return as-is */
    if (tag_i != -1 && tag_content[0] != '\0') {
        /* Extract attributes without allocating — parse from tag directly */
        char** attr = NULL;
        if (name_len < strlen(tag)) {
            attr = get_tag_attributes(tag);
        }
        char* tag_result = (*tag_functions[tag_i])(tag_content, attr);
        /* Free attributes — array is NULL-terminated */
        if (attr != NULL) {
            for (int i = 0; attr[i] != NULL; i++) {
                free(attr[i]);
            }
            free(attr);
        }
        return tag_result;
    }

    /* Invalid tag or empty content — return empty string and free content */
    char* result = strdup(tag_content);
    free(tag_content);
    if (result == NULL) result = strdup("");
    return result;
}

char* execute_nested_tags_depth(char* str, int depth)
{
    if (depth > MAX_NESTING_DEPTH) {
        printf("  Error: maximum nesting depth (%d) exceeded\n", MAX_NESTING_DEPTH);
        char* result = strdup(str);
        free(str);
        return result;
    }

    char* tag = get_tag(str);
    if (tag == NULL) {
        char* result = strdup(str);
        free(str);
        return result;
    }

    char* t_tag = strdup(tag);
    t_tag = rm_spaces_start_end(t_tag);

    char* tag_content = get_tag_content(str, tag);
    char* processed_content = execute_nested_tags_depth(tag_content, depth + 1);

    char* text_before_tag = get_text_before_tag(str, tag);
    char* text_after_tag = get_text_after_tag(str, t_tag);

    /* Ensure we have valid strings */
    if (text_before_tag == NULL) text_before_tag = strdup("");
    if (text_after_tag == NULL) text_after_tag = strdup("");
    if (processed_content == NULL) processed_content = strdup("");

    char* tag_result = execute_tag(tag, processed_content);
    if (tag_result == NULL) tag_result = strdup("");

    /* Combine: before + result + after — use lengths we already know */
    size_t len1 = strlen(text_before_tag);
    size_t len2 = strlen(tag_result);
    size_t len3 = strlen(text_after_tag);
    size_t total = len1 + len2 + len3 + 1;
    char* result = (char*)malloc(total);
    is_memory_allocated(result);
    char* dst = result;
    memcpy(dst, text_before_tag, len1); dst += len1;
    memcpy(dst, tag_result, len2); dst += len2;
    memcpy(dst, text_after_tag, len3 + 1);

    /* Cleanup */
    free(text_before_tag);
    free(text_after_tag);
    free(tag_result);
    free(t_tag);
    free(tag);
    free(str);

    return result;
}

char* execute_nested_tags(char* str)
{
    return execute_nested_tags_depth(str, 1);
}

char* execute_all_tags(char* str)
{
    int iterations = 0;
    /* Count initial tags */
    int prev_tag_count = 0;
    {
        char* tmp = str;
        while ((tmp = strchr(tmp, '<')) != NULL) {
            prev_tag_count++;
            tmp++;
        }
    }

    while (iterations < MAX_TAG_ITERATIONS) {
        char* tag = get_tag(str);
        if (tag == NULL) {
            return str;
        }

        free(tag);
        /* execute_nested_tags takes ownership of str and frees it internally.
         * After this call, str is invalid and must not be used. */
        char* new_str = execute_nested_tags(str);

        /* Count remaining tags in new_str */
        int new_tag_count = 0;
        {
            const char* tmp = new_str;
            while ((tmp = strchr(tmp, '<')) != NULL) {
                new_tag_count++;
                tmp++;
            }
        }

        /* If no more tags, we're done */
        if (new_tag_count == 0) {
            return new_str;
        }

        /* Detect infinite loop: if tag count didn't decrease, we're not
         * making progress. This is more reliable than length comparison
         * since tag replacement may produce strings of equal length. */
        if (new_tag_count >= prev_tag_count) {
            /* Not making progress — return what we have so far */
            return new_str;
        }
        prev_tag_count = new_tag_count;

        str = new_str;
        iterations++;
    }

    printf("  Warning: execute_all_tags reached max iterations (%d)\n", MAX_TAG_ITERATIONS);
    return str;
}
