#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
/* tags.c
 *
 * Copyright (C) 2024 Dmitriy Eliseev
 * This file is part of txtFormatter.
 *
 * txtFormatter is licensed under the GNU General Public License, version 3.
 * See the LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.en.html>
 * for details.
 */
#include "tags.h"
#include <errno.h>
/* Suppress -Wpedantic for tinyexpr's unnamed union (third-party library). */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "tinyexpr.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include "utf8_utils.h"
#include <unistd.h>
/***************************************************************************
* Date and Time
***************************************************************************/
char* get_date(char* str, char** attrs)
{
    if (str) free(str);
    (void)attrs;
    char* date = calloc(32, sizeof(char));
    is_memory_allocated(date);
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if ( local_time == NULL ) {
        strcpy(date, "unknown");
        return date;
    }
    snprintf(date, 32, "%02d.%02d.%d", local_time->tm_mday, local_time->tm_mon + 1,
            local_time->tm_year + 1900);
    return date;
}

char* get_time(char* str, char** attrs)
{
    if (str) free(str);
    (void)attrs;
    char* t = calloc(16, sizeof(char));
    is_memory_allocated(t);
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if ( local_time == NULL ) {
        strcpy(t, "unknown");
        return t;
    }
    snprintf(t, 16, "%02d:%02d:%02d", local_time->tm_hour, local_time->tm_min,
            local_time->tm_sec);
    return t;
}

char* get_datetime(char* str, char** attrs)
{
    free(str);
    (void)attrs;
    /* Single time() call shared between date and time */
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    char date[32], t[16];
    if (local_time == NULL) {
        return strdup("unknown unknown");
    }
    snprintf(date, sizeof(date), "%02d.%02d.%d", local_time->tm_mday, local_time->tm_mon + 1,
            local_time->tm_year + 1900);
    snprintf(t, sizeof(t), "%02d:%02d:%02d", local_time->tm_hour, local_time->tm_min,
            local_time->tm_sec);
    char* datetime = calloc(strlen(date) + strlen(t) + 3, sizeof(char));
    is_memory_allocated(datetime);
    snprintf(datetime, strlen(date) + strlen(t) + 3, "%s %s", date, t);
    return datetime;
}


/***************************************************************************
* Text Alignment
***************************************************************************/
char* right(char* str, char** attrs)
{
    (void)attrs;
    char** t = calloc(1, sizeof(char*));
    is_memory_allocated(t);
    char* result = get_aligned_text(str, t);
    free(str);
    free(t);
    return result;
}

char* center(char* str, char** attrs)
{
    (void)attrs;
    char* result = get_aligned_text(str, NULL);
    free(str);
    return result;
}


/***************************************************************************
* Headers
***************************************************************************/
char* h1(char* str, char** attrs)
{
    return header(str, 1, attrs);
}

char* h2(char* str, char** attrs)
{
    return header(str, 2, attrs);
}

char* h3(char* str, char** attrs)
{
    return header(str, 3, attrs);
}

char* h4(char* str, char** attrs)
{
    size_t str_bytes = strlen(str);
    size_t str_width = utf8_strlen(str);
    /* Buffer needs: str bytes + '\n' + separator (str_width chars) + '\0' */
    size_t buf_size = str_bytes + 1 + str_width + 1;
    char* h = calloc(buf_size, sizeof(char));
    is_memory_allocated(h);
    char sep_sym = '-';
    if ( attrs != NULL ) {
        if ( attrs[0] != NULL && attrs[0][0] != '\0' )
            sep_sym = attrs[0][0];
    }

    char* s = get_str_from_sym(sep_sym, str_width);
    snprintf(h, buf_size, "%s\n%s", str, s);
    free(s);
    free(str);
    return h;
}


/***************************************************************************
* Text Formatting
***************************************************************************/
char* tag_doc_width(char* str, char** attrs)
{
    free(str);
    if ( attrs != NULL ) {
        if ( attrs[0] != NULL ) {
            if ( is_number(attrs[0], 1) ) {
                /* Use strtol instead of atoi to avoid undefined behavior
                   on integer overflow. strtol sets errno on overflow. */
                char* endptr;
                errno = 0;
                long w = strtol(attrs[0], &endptr, 10);
                /* Check for overflow/underflow and invalid tail chars */
                if (errno == ERANGE || endptr == attrs[0]) {
                    printf("  Error: invalid document width \"%s\"\n",
                           attrs[0]);
                } else {
                    set_doc_width((int)w);
                }
            }
        }
    }

    char* r = calloc(1, sizeof(char));
    return r;
}

char* def_width(char* str, char** attrs)
{
    free(str);
    (void)attrs;
    set_doc_width(DEFAULT_DOC_WIDTH);
    char* r = calloc(1, sizeof(char));
    return r;
}

char* separator(char* str, char** attrs)
{
    free(str);
    char sep_symbol = '-';
    if ( attrs != NULL ) {
        if ( attrs[0] != NULL && attrs[0][0] != '\0' )
            sep_symbol = attrs[0][0];
    }

    return get_str_from_sym(sep_symbol, DOC_WIDTH);
}

char* p(char* str, char** attrs)
{
    char* tmp_str = NULL;
    if ( attrs != NULL ) {
        uint8_t slot = push_doc_width();
        set_doc_width((int)DOC_WIDTH - 2);
        tmp_str = right(strdup(str), NULL);
        restore_doc_width(slot);
    } else
        tmp_str = strdup(str);

    /* Free the original str now */
    free(str);

    size_t lines_count;
    char** lines = split_count('\n', tmp_str, &lines_count);
    /* UTF-8: allocate more space */
    size_t len = strlen(tmp_str) * 2 + lines_count * 4 + 10;
    char* pr = calloc(len, sizeof(char));
    is_memory_allocated(pr);
    size_t pos = 0;
    for ( size_t i=0; i<lines_count; i++ ) {
        size_t slen = strlen(lines[i]);
        if ( attrs == NULL ) {
            memcpy(pr + pos, "  ", 2); pos += 2;
            memcpy(pr + pos, lines[i], slen); pos += slen;
        } else {
            memcpy(pr + pos, lines[i], slen); pos += slen;
            memcpy(pr + pos, "  ", 2); pos += 2;
        }
        pr[pos++] = '\n';
        free(lines[i]);
    }

    free(tmp_str);
    free(lines);
    pr[pos] = '\0';
    return pr;
}

char* get_framed_text(char* str, char** attrs)
{
    (void)attrs;
    size_t lines_count;
    char** lines = split_count('\n', str, &lines_count);
    size_t max_line = get_max_len(lines, lines_count);

    /* Empty content → empty output */
    if (max_line == 0 && lines_count <= 1 && (lines_count == 0 || lines[0][0] == '\0')) {
        for (size_t i = 0; i < lines_count; i++)
            free(lines[i]);
        free(lines);
        free(str);
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        return result;
    }

    /* UTF-8: buffer needs to be max_line * 4 bytes + framing overhead */
    size_t len = (max_line * 4 + 20) * (lines_count + 2);
    char* framed_text = calloc(len, sizeof(char));
    is_memory_allocated(framed_text);
    size_t pos = 0;

    for ( size_t i=0; i<lines_count; i++ )
        free(lines[i]);

    free(lines);

    uint8_t slot = push_doc_width();
    int frame_width = (int)max_line + 2;
    set_doc_width(frame_width);
    char* tmp_str = center(str, NULL);  /* center takes ownership of str */
    lines = split('\n', tmp_str);
    free(tmp_str);
    tmp_str = get_str_from_sym('=', max_line);
    size_t tlen;

    tlen = strlen(" .+-"); memcpy(framed_text + pos, " .+-", tlen); pos += tlen;
    tlen = strlen(tmp_str); memcpy(framed_text + pos, tmp_str, tlen); pos += tlen;
    tlen = strlen("-+. \n"); memcpy(framed_text + pos, "-+. \n", tlen); pos += tlen;

    char* al = NULL;
    for ( size_t i=0; i<lines_count; i++ ) {
        tlen = strlen(" ||"); memcpy(framed_text + pos, " ||", tlen); pos += tlen;
        tlen = strlen(lines[i]); memcpy(framed_text + pos, lines[i], tlen); pos += tlen;
        size_t line_width = utf8_strlen(lines[i]);
        int32_t spaces_raw = (int32_t)DOC_WIDTH - (int32_t)line_width;
        size_t spaces = (spaces_raw > 0) ? (size_t)spaces_raw : 0;
        al = get_str_from_sym(' ', spaces);
        tlen = strlen(al); memcpy(framed_text + pos, al, tlen); pos += tlen;
        free(al);
        tlen = strlen("|| \n"); memcpy(framed_text + pos, "|| \n", tlen); pos += tlen;
        free(lines[i]);
    }

    restore_doc_width(slot);
    free(lines);

    tlen = strlen(" '+-"); memcpy(framed_text + pos, " '+-", tlen); pos += tlen;
    tlen = strlen(tmp_str); memcpy(framed_text + pos, tmp_str, tlen); pos += tlen;
    free(tmp_str);
    tlen = strlen("-+' "); memcpy(framed_text + pos, "-+' ", tlen); pos += tlen;
    framed_text[pos] = '\0';
    return framed_text;
}

char* get_list(char* str, char** attrs)
{
    size_t items_count;
    char** items = split_count('\n', str, &items_count);
    free(str);  /* Free original str after splitting */

    /* Filter out empty items and count real ones */
    size_t real_count = 0;
    for (size_t i = 0; i < items_count; i++) {
        if (items[i][0] != '\0') real_count++;
    }

    /* Empty list → empty output */
    if (real_count == 0) {
        for (size_t i = 0; i < items_count; i++)
            free(items[i]);
        free(items);
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        return result;
    }

    size_t align = 0;
    if ( attrs == NULL )
        align = get_number_len(real_count);

    /* UTF-8: allocate more space for multi-byte characters */
    size_t max_item_len = 0;
    for (size_t i = 0; i < items_count; i++) {
        size_t l = utf8_strlen(items[i]);
        if (l > max_item_len) max_item_len = l;
    }
    /* Each line needs: marker (align+4 bytes) + item content (max_item_len * 4 for UTF-8) + newline + padding */
    size_t len = real_count * (max_item_len * 4 + align + 10) + 1;
    char* lst = calloc(len, sizeof(char));
    is_memory_allocated(lst);
    size_t pos = 0;
    size_t item_num = 0;

    for ( size_t i=0; i<items_count; i++ ) {
        /* Skip empty lines */
        if (items[i][0] == '\0') continue;
        item_num++;

        /* Use stack buffer for marker — never exceeds 15 chars */
        char mrk_str[16];
        int mrk_len;
        if ( attrs == NULL ) {
            size_t pad = align - get_number_len(item_num);
            mrk_len = snprintf(mrk_str, sizeof(mrk_str), "%*s %zu) ", (int)pad, "", item_num);
        } else {
            /* Use '#' as default bullet if attribute is empty or NULL.
             * <list>   → numbered list (attrs == NULL)
             * <list *> → bullet list with '*'
             * <list -> → bullet list with '-'
             * <list >  → bullet list with '#' (empty attribute defaults to '#') */
            char bullet = (attrs[0] != NULL && attrs[0][0] != '\0') ? attrs[0][0] : '#';
            mrk_len = snprintf(mrk_str, sizeof(mrk_str), " %c ", bullet);
        }

        if (mrk_len > 0) {
            memcpy(lst + pos, mrk_str, (size_t)mrk_len);
            pos += (size_t)mrk_len;
        }

        size_t item_strlen = strlen(items[i]);
        memcpy(lst + pos, items[i], item_strlen); pos += item_strlen;

        if ( item_num < real_count )
            lst[pos++] = '\n';
    }
    /* Cleaning */
    for ( size_t i=0; i<items_count; i++ )
        free(items[i]);

    free(items);
    return lst;
}

char* get_lines(char* str, char** attrs)
{
    free(str);
    /* Default: 1 newline (produces 1 blank line in output).
     * <lines>     → 1 blank line (default)
     * <lines 0>   → 1 blank line (0 is treated as default)
     * <lines 5>   → 5 blank lines
     * <lines N> where N < 0 or invalid → 1 blank line (falls back to default) */
    size_t count = 1;
    if ( attrs != NULL ) {
        if ( attrs[0] != NULL ) {
            if ( is_number(attrs[0], 1) ) {
                char* endptr;
                errno = 0;
                long val = strtol(attrs[0], &endptr, 10);
                if (errno != ERANGE && endptr != attrs[0] && val > 0) {
                    count = (size_t)val;
                } else if (errno == ERANGE) {
                    printf("  Error: invalid line count \"%s\"\n",
                           attrs[0]);
                }
            }
        }
    }

    char*  lines = get_str_from_sym('\n', count);
    return lines;
}


/***************************************************************************
* Calculations and Visualization
***************************************************************************/

/* Core evaluation: compute a single expression.
   Returns newly allocated result string. Does NOT split or free expr. */
char* calc_single_expr(const char* expr, int show_expr)
{
    size_t expr_len = strlen(expr);
    size_t buf_size = expr_len + 200;
    if (buf_size < 64) buf_size = 64;

    char* tmp = calloc(buf_size, sizeof(char));
    is_memory_allocated(tmp);
    int error;
    double result = te_interp(expr, &error);

    /* tinyexpr does NOT set error for inf/nan — detect manually */
    int bad_result = error || isnan(result) || isinf(result);

    if (show_expr) {
        if (bad_result)
            snprintf(tmp, buf_size, "%s = error", expr);
        else
            snprintf(tmp, buf_size, "%s = %g", expr, result);
    } else {
        if (bad_result)
            snprintf(tmp, buf_size, "error");
        else
            snprintf(tmp, buf_size, "%g", result);
    }
    return tmp;
}

char* calc(char* str, char** attrs)
{
    size_t expr_count;
    char** expressions = split_count('\n', str, &expr_count);
    free(str);  /* Free original str after splitting */
    int show_expr = (attrs != NULL);

    /* Handle empty expression list */
    if (expr_count == 0) {
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        return result;
    }

    /* First pass: compute results */
    char** results = calloc(expr_count, sizeof(char*));
    is_memory_allocated(results);
    size_t total_len = 0;
    for ( size_t i=0; i<expr_count; i++ ) {
        results[i] = calc_single_expr(expressions[i], show_expr);
        total_len += strlen(results[i]) + 1;  /* +1 for \n or padding */
        free(expressions[i]);
    }
    free(expressions);

    /* Second pass: build result string with memcpy */
    char* result_str = calloc(total_len, sizeof(char));
    is_memory_allocated(result_str);
    size_t pos = 0;
    for ( size_t i=0; i<expr_count; i++ ) {
        size_t slen = strlen(results[i]);
        memcpy(result_str + pos, results[i], slen); pos += slen;
        if ( i < expr_count - 1 )
            result_str[pos++] = '\n';
        free(results[i]);
    }
    free(results);
    return result_str;
}

char* get_table(char* str, char** attrs)
{
    /* Unified parse: one pass instead of three separate calls */
    uint16_t* cells_in_row = NULL;
    size_t rows_count;
    char*** table_data = parse_table(str, &rows_count, &cells_in_row);
    free(str);  /* Free original str after parsing */

    uint8_t nb = in_str_array(attrs, "nb");/* no border */
    uint8_t nc = in_str_array(attrs, "nc");/* no calculations */
    uint8_t na = in_str_array(attrs, "na");/*don't align numbers to the right*/

    if ( nb == 1 )
        na = 1;

    if ( nc == 0 )
        calc_in_table(table_data, (uint16_t)rows_count, cells_in_row);

    /* Compute column widths ONCE, reuse for alignment and max_row_len */
    uint16_t max_cells = 0;
    for (size_t i = 0; i < rows_count; i++)
        if (cells_in_row[i] > max_cells) max_cells = cells_in_row[i];
    uint16_t** column_width = get_column_width(table_data, (uint16_t)rows_count, cells_in_row);

    align_to_columns_with_width(table_data, (uint16_t)rows_count, cells_in_row, na,
                                column_width, max_cells);
    uint16_t* rows_len     = get_rows_len(table_data, (uint16_t)rows_count, cells_in_row);
    size_t  max_row_len  = get_max_row_len(table_data, (uint16_t)rows_count,
                                             cells_in_row);

    /* Free column_width after use */
    for (uint16_t i = 0; i < max_cells; i++)
        free(column_width[i]);
    free(column_width);

    if ( max_row_len < (size_t)DOC_WIDTH - 2 )
        max_row_len = (size_t)DOC_WIDTH - 2;

    size_t table_buf = rows_count * (max_row_len * 4 + 10) + 1;
    char* table = calloc(table_buf, sizeof(char));
    is_memory_allocated(table);
    size_t tpos = 0;  /* track position for O(1) appends instead of O(n) strcat */

    for ( size_t i=0; i<rows_count; i++ ) {
        if ( nb == 0 ) {
            table[tpos++] = '|';
        }

        uint16_t* cells_len = get_cells_len(table_data[i], cells_in_row[i]);
        size_t  align     = max_row_len - rows_len[i];
        int overflow_detected = 0;

        /* Guard against zero-cell rows — prevents OOB access in get_min_index */
        if (cells_in_row[i] == 0) {
            free(cells_len);
            if (tpos + 1 < table_buf)
                table[tpos++] = '\n';
            continue;
        }

        while ( align > 0 ) {
            uint16_t min_cell_i = get_min_index(cells_len, cells_in_row[i]);
            size_t old_len = strlen(table_data[i][min_cell_i]);
            char* al_str = calloc(old_len + 3, sizeof(char));
            is_memory_allocated(al_str);
            int is_num = is_number(table_data[i][min_cell_i], 2);
            if (is_num && na == 0) {
                al_str[0] = ' ';
                memcpy(al_str + 1, table_data[i][min_cell_i], old_len + 1);
            } else {
                memcpy(al_str, table_data[i][min_cell_i], old_len);
                al_str[old_len] = ' ';
                al_str[old_len + 1] = '\0';
            }

            free(table_data[i][min_cell_i]);
            table_data[i][min_cell_i] = al_str;
            cells_len[min_cell_i]++;
            align--;
        }

        for ( uint16_t j=0; j<cells_in_row[i]; j++ ) {
            size_t slen = strlen(table_data[i][j]);
            if (tpos + slen + 2 >= table_buf) {
                printf("  Warning: table buffer overflow prevented\n");
                overflow_detected = 1;
                break;
            }
            memcpy(table + tpos, table_data[i][j], slen);
            tpos += slen;
            if ( nb == 0 ) {
                table[tpos++] = '|';
            } else {
                table[tpos++] = ' ';
            }
        }

        if (overflow_detected) {
            free(cells_len);
            break;
        }

        if (tpos + 1 < table_buf)
            table[tpos++] = '\n';
        free(cells_len);
    }

    char* result = (nb == 0) ? add_table_border(table) : table;
    /* Cleaning */
    if ( nb == 0 )
        free(table);
    
    for ( size_t i=0; i<rows_count; i++ ) {
        for ( uint16_t j=0; j<cells_in_row[i]; j++ )
            free(table_data[i][j]);
        
        free(table_data[i]);
    }

    free(table_data);
    free(rows_len);
    free(cells_in_row);

    return result;
}

char* get_histogram(char* str, char** attrs)
{
    char sym = '#';

    if ( attrs != NULL ) {
        if ( attrs[0] != NULL && attrs[0][0] != '\0' )
            sym = attrs[0][0];
    }

    size_t lines_count = get_elements_count('\n', str);
    char** names = calloc(lines_count, sizeof(char*));
    is_memory_allocated(names);
    char** values = calloc(lines_count, sizeof(char*));
    is_memory_allocated(values);
    get_histogram_data(str, names, values);
    free(str);  /* Free original str after parsing */

    size_t len = (size_t)(DOC_WIDTH * 4 + 1) * lines_count; /* UTF-8 safety */
    char* histogram = calloc(len, sizeof(char));
    is_memory_allocated(histogram);
    size_t hist_pos = 0;

    size_t max_name = get_max_len(names, lines_count);
    double max_value = get_max_value(values, lines_count);
    size_t max_value_len = get_max_len(values, lines_count);
    int32_t hist_width_raw = (int32_t)DOC_WIDTH - (int32_t)max_name - (int32_t)max_value_len - 8;
    size_t hist_width = (hist_width_raw > 0) ? (size_t)hist_width_raw : 1;
    double hist_sym = (max_value > 0.0) ? max_value / (double)(hist_width) : 1.0;

    /* Reusable buffer for line formatting — allocated in bytes, not chars.
     * UTF-8 names can use up to 4 bytes per character. */
    size_t tmp_max = max_name * 4 + max_value_len * 4 + hist_width + 30;
    char* tmp = calloc(tmp_max + 1, sizeof(char));
    is_memory_allocated(tmp);

    for ( size_t i=0; i<lines_count; i++ ) {
        if ( strcmp(values[i], " ") != 0 ) {
            size_t name_width = utf8_strlen(names[i]);
            size_t pad = max_name - name_width;
            double v = 0;
            int is_negative = 0;
            if ( is_number(values[i], 1) == 1 ) {
                v = strtod(values[i], NULL);
                if (v < 0) {
                    is_negative = 1;
                    v = -v;  /* use absolute value for bar length */
                }
            }

            size_t hist_len = (size_t)round(v / (double)hist_sym);
            /* Negative values: no bar, just show the value */
            if (is_negative) {
                hist_len = 0;
            }
            /* Safety: clamp hist_len to remaining buffer space */
            if (hist_len > hist_width) hist_len = hist_width;

            /* Format: " name | bar<spaces> | value " */
            size_t pos = 0;
            tmp[pos++] = ' ';
            memset(tmp + pos, ' ', pad); pos += pad;
            size_t nlen = strlen(names[i]);
            memcpy(tmp + pos, names[i], nlen); pos += nlen;
            tmp[pos++] = ' ';
            tmp[pos++] = '|';
            tmp[pos++] = ' ';

            /* Draw histogram bar */
            if (pos + hist_len >= tmp_max) hist_len = (tmp_max > pos + 1) ? tmp_max - pos - 1 : 0;
            memset(tmp + pos, sym, hist_len); pos += hist_len;

            /* Pad remaining space with blanks */
            size_t spaces_len = hist_width - hist_len;
            if (pos + spaces_len >= tmp_max) spaces_len = (tmp_max > pos + 1) ? tmp_max - pos - 1 : 0;
            memset(tmp + pos, ' ', spaces_len); pos += spaces_len;

            /* Separator and value */
            tmp[pos++] = ' ';
            tmp[pos++] = '|';
            tmp[pos++] = ' ';
            size_t vlen = strlen(values[i]);
            memcpy(tmp + pos, values[i], vlen); pos += vlen;
            tmp[pos++] = ' '; /* trailing space to match reference */
            tmp[pos] = '\0';
        } else {
            tmp[0] = '\n';
            tmp[1] = '\0';
        }

        size_t tmp_len = strlen(tmp);
        if (hist_pos + tmp_len + 1 >= len) {
            printf("  Warning: histogram buffer overflow prevented\n");
            break;
        }
        memcpy(histogram + hist_pos, tmp, tmp_len);
        hist_pos += tmp_len;
        free(names[i]);
        free(values[i]);

        if (hist_pos + 1 < len && i < lines_count - 1) {
            histogram[hist_pos++] = '\n';
        }
    }
    histogram[hist_pos] = '\0';
    free(tmp);

    free(names);
    free(values);
    return histogram;
}


/***************************************************************************
* Files
***************************************************************************/
char* insert(char* str, char** attrs)
{
    free(str);
    char* inserting_text = NULL;
    size_t ins_len = 0;
    if ( attrs != NULL ) {
        size_t files_count = get_arr_size(attrs);
        char** file_contents = calloc(files_count, sizeof(char*));
        is_memory_allocated(file_contents);

        /* Get current working directory for path validation */
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            puts("  Error inserting txt: cannot determine current directory");
            free(file_contents);
            inserting_text = calloc(2, sizeof(char));
            strcpy(inserting_text, "\n");
            return inserting_text;
        }
        size_t cwd_len = strlen(cwd);

        for ( size_t i=0; i<files_count; i++ ) {
            if ( attrs[i] != NULL ) {
                /* Security: reject path traversal and absolute paths */
                if (strstr(attrs[i], "..") != NULL || attrs[i][0] == '/' ||
                    attrs[i][0] == '\\') {
                    printf("  Error inserting txt: invalid path \"%s\"\n",
                           attrs[i]);
                    file_contents[i] = NULL;
                    continue;
                }

                /* Resolve the full path and check it stays within CWD.
                   This prevents symlink attacks (e.g., link -> /etc/passwd). */
                char resolved[PATH_MAX];
                if (realpath(attrs[i], resolved) == NULL) {
                    printf("  Error inserting txt: cannot resolve path \"%s\"\n",
                           attrs[i]);
                    file_contents[i] = NULL;
                    continue;
                }

                /* Verify resolved path is within current working directory */
                if (strncmp(resolved, cwd, cwd_len) != 0 ||
                    (resolved[cwd_len] != '/' && resolved[cwd_len] != '\0')) {
                    printf("  Error inserting txt: path \"%s\" is outside "
                           "the current directory\n", attrs[i]);
                    file_contents[i] = NULL;
                    continue;
                }

                file_contents[i] = get_file_content(attrs[i]);
                if ( file_contents[i] != NULL ) {
                    change_symbols('<', '\f', file_contents[i]);
                    change_symbols('>', '\a', file_contents[i]);
                    size_t flen = strlen(file_contents[i]);
                    /* Check for size_t overflow */
                    if (ins_len + flen + files_count * 2 + 2 < ins_len) {
                        printf("  Error inserting txt: content too large\n");
                        free(file_contents[i]);
                        file_contents[i] = NULL;
                        continue;
                    }
                    ins_len += flen;
                }
            } else
                file_contents[i] = NULL;
        }

        /* +1 for initial \n, +files_count*2 for \n\n after each file, +1 for \0 */
        ins_len += 1 + files_count * 2 + 1;
        inserting_text = calloc(ins_len, sizeof(char));
        is_memory_allocated(inserting_text);
        size_t pos = 0;
        inserting_text[pos++] = '\n';

        for ( uint16_t i=0; i<files_count; i++ ) {
            if ( file_contents[i] != NULL ) {
                size_t flen = strlen(file_contents[i]);
                memcpy(inserting_text + pos, file_contents[i], flen); pos += flen;
                inserting_text[pos++] = '\n';
                free(file_contents[i]);
            }
            inserting_text[pos++] = '\n';
        }

        free(file_contents);
    } else {
        puts("  Error inserting txt: file not specified");
        inserting_text = calloc(2, sizeof(char));
        strcpy(inserting_text, "\n");
    }

    return inserting_text;
}
