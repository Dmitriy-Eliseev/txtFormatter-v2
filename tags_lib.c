#define _POSIX_C_SOURCE 200809L
/* tags_lib.c
 *
 * Copyright (C) 2024 Dmitriy Eliseev
 * This file is part of txtFormatter.
 *
 * txtFormatter is licensed under the GNU General Public License, version 3.
 * See the LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.en.html>
 * for details.
 */
#include "tags_lib.h"
#include "utf8_utils.h"

/* Forward declarations for functions defined in other translation units.
 * These are needed because tags_lib.h no longer includes tags.h/tag_handler.h
 * to break circular dependencies. */
char*  get_tag_name         (char* tag);
char*  center               (char* str, char** attrs);
char*  calc                 (char* str, char** attrs);
char*  tag_doc_width        (char* str, char** attrs);

uint8_t DOC_WIDTH = DEFAULT_DOC_WIDTH;
/***************************************************************************
* functions for working with errors
***************************************************************************/
void exit_on_error(char* msg, void* ptr)
{
    if ( ptr == NULL ) {
        fprintf(stderr, "%s", msg);
        exit(EXIT_FAILURE);
    }
}

void is_memory_allocated(void* mem_ptr)
{
    exit_on_error("Memory allocation error\n", mem_ptr);
}

void is_directory_opened(void* dir_ptr)
{
    exit_on_error("Error opening directory\n", dir_ptr);
}

void print_file_error(char* filename)
{
    printf("  Error opening file \"%s\"\n", filename);
}


/***************************************************************************
* functions for working with files
***************************************************************************/

/* Unified function: scans directory once, returns NULL-terminated file list. */
char** get_files_in_dir(char* dirname, char* file_extension)
{
    DIR *dir = opendir(dirname);
    is_directory_opened(dir);

    struct dirent *file;
    /* Dynamic array: start with capacity 16, grow as needed */
    size_t capacity = 16;
    size_t count = 0;
    char** file_names = calloc(capacity, sizeof(char*));
    is_memory_allocated(file_names);

    while ( (file = readdir(dir)) != NULL ) {
        char* extension = strrchr(file->d_name, '.');
        if ( extension != NULL && strcmp(extension, file_extension) == 0 ) {
            if (count + 1 >= capacity) {  /* +1 for NULL terminator */
                capacity *= 2;
                char** tmp = realloc(file_names, capacity * sizeof(char*));
                if (tmp == NULL) {
                    /* Free already allocated entries before exiting */
                    for (size_t k = 0; k < count; k++)
                        free(file_names[k]);
                    free(file_names);
                    closedir(dir);
                    exit_on_error("Memory allocation error\n", NULL);
                }
                file_names = tmp;
            }
            file_names[count] = calloc(NAME_MAX + 2, sizeof(char));
            is_memory_allocated(file_names[count]);
            /* Use snprintf for guaranteed null termination */
            snprintf(file_names[count], NAME_MAX + 2, "%s", file->d_name);
            count++;
        }
    }

    closedir(dir);

    /* NULL-terminate the array */
    if (count >= capacity) {
        capacity = count + 1;
        char** tmp = realloc(file_names, capacity * sizeof(char*));
        if (tmp == NULL) {
            for (size_t k = 0; k < count; k++)
                free(file_names[k]);
            free(file_names);
            exit_on_error("Memory allocation error\n", NULL);
        }
        file_names = tmp;
    }
    file_names[count] = NULL;

    return file_names;
}

/* Returns count by scanning the NULL-terminated array. */
size_t get_files_count(char* dirname, char* file_extension)
{
    char** files = get_files_in_dir(dirname, file_extension);
    size_t count = 0;
    while (files[count] != NULL) {
        count++;
    }
    /* Free the array */
    for (size_t i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
    return count;
}

char* get_file_content(char* filename)
{
    FILE *file = fopen(filename, "r");
    if ( file == NULL ) {
        print_file_error(filename);
        return NULL;
    }

    /* Getting file size */
    fseek(file, 0, SEEK_END);
    long file_size_long = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size_long < 0) {
        printf("  Error: cannot determine size of \"%s\"\n", filename);
        fclose(file);
        return NULL;
    }

    /* Limit file size to 100 MB */
    if (file_size_long > 100 * 1024 * 1024) {
        printf("  Error: file \"%s\" is too large (%ld bytes, max 100 MB)\n",
               filename, file_size_long);
        fclose(file);
        return NULL;
    }

    uint32_t file_size = (uint32_t)file_size_long;

    /* Getting text from file */
    char* str = (char*)calloc(file_size + 1,  sizeof(char));
    is_memory_allocated(str);
    size_t result = fread(str, 1, file_size, file);

    if (result != (size_t)file_size) {
        /* File may contain NUL bytes (fread stops at '\0') or a read error.
           Truncate safely and warn the user. */
        if (result < (size_t)file_size) {
            if (ferror(file)) {
                printf("  Warning: read error in \"%s\" (read %zu of %u bytes)\n",
                       filename, result, file_size);
            } else {
                printf("  Warning: file \"%s\" contains NUL bytes — "
                       "truncated at byte %zu\n", filename, result);
            }
        }
        str[result] = '\0';
    } else {
        str[result] = '\0';
    }

    fclose(file);
    return str;
}

void write_to_file(char* filename, char* str)
{
    FILE *file;
    file = fopen(filename, "w");
    if ( file == NULL ) {
        print_file_error(filename);
        return;
    }

    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, file);
    if (written != len) {
        printf("  Error: failed to write \"%s\" (wrote %zu of %zu bytes)\n",
               filename, written, len);
    }
    if (fclose(file) != 0) {
        printf("  Error: failed to close \"%s\"\n", filename);
    }
}

char* change_file_extension(char* filename, char* extension)
{
    /* Ensure extension starts with '.' */
    const char* dot = (extension[0] == '.') ? "" : ".";
    char* result = calloc(strlen(filename) + strlen(dot) + strlen(extension) + 1, sizeof(char));
    is_memory_allocated(result);
    strcpy(result, filename);
    /* find start of file extension */
    char* extension_start = strrchr(result, '.');
    if ( extension_start != NULL ) {
        /* Truncate at old extension and append new one */
        *extension_start = '\0';
        strcat(result, dot);
        strcat(result, extension);
    } else {
        /* No extension found — append it */
        strcat(result, dot);
        strcat(result, extension);
    }

    return result;
}


/***************************************************************************
* functions for working with strings
***************************************************************************/
size_t get_elements_count(char sym, char* str)
{
    if (str == NULL || str[0] == '\0')
        return 1; /* Empty string = 1 empty token */

    size_t count = 1;
    for (const char* p = str; *p; p++) {
        if (*p == sym)
            count++;
    }
    return count;
}

char** split(char sym, char* str)
{
    return split_count(sym, str, NULL);
}

/* Split string and optionally return element count. */
char** split_count(char sym, char* str, size_t *out_count)
{
    if (str == NULL) {
        char** elements = calloc(1, sizeof(char*));
        is_memory_allocated(elements);
        elements[0] = calloc(1, sizeof(char));
        is_memory_allocated(elements[0]);
        if (out_count) *out_count = 1;
        return elements;
    }

    size_t count = get_elements_count(sym, str);
    if (out_count) *out_count = count;
    char** elements = (char**)calloc(count, sizeof(char*));
    is_memory_allocated(elements);

    const char* start = str;
    const char* end;
    size_t idx = 0;

    while ((end = strchr(start, sym)) != NULL) {
        size_t len = (size_t)(end - start);
        elements[idx] = calloc(len + 1, sizeof(char));
        is_memory_allocated(elements[idx]);
        memcpy(elements[idx], start, len);
        elements[idx][len] = '\0';
        idx++;
        start = end + 1;
    }

    /* Last token (after final delimiter, or the whole string if no delimiter) */
    size_t len = strlen(start);
    elements[idx] = calloc(len + 1, sizeof(char));
    is_memory_allocated(elements[idx]);
    strcpy(elements[idx], start);
    idx++;

    (void)idx; /* idx should equal count */
    return elements;
}

char* get_str_from_sym(char sym, size_t count)
{
    char* str = calloc(count + 1, sizeof(char));
    is_memory_allocated(str);
    for ( size_t i=0; i<count; i++ )
        str[i] = sym;
    str[count] = '\0';
    return str;
}

void change_symbols(char from, char to, char* str)
{
    char* p = str;
    while (*p) {
        if (*p == from) *p = to;
        p++;
    }
}

uint8_t is_number(char* str, uint8_t mode)
{
    if ( str[0] == '\n' || str[0] == '\0' )
        return 0;
    /* mode:
       0 - only digits
       1 - digits + '-' (only at start or after leading spaces) + '.' + ' '
       2 - digits + '-', '.', ',', ' ' (for table cells with comma decimals) */
    if ( mode > 2 )
        mode = 2;

    uint8_t result = 0;
    uint8_t has_digit = 0;
    uint8_t has_dot = 0;
    uint8_t has_minus = 0;
    size_t len = strlen(str);
    for ( size_t i=0; i<len; i++ ) {
        if ( isdigit((unsigned char)str[i]) ) {
            has_digit = 1;
            result = 1;
        } else if (str[i] == '-' && !has_minus && (mode == 1 || mode == 2)) {
            /* Minus sign valid only at start or after leading spaces */
            /* Check that all previous chars are spaces */
            uint8_t all_spaces = 1;
            for (size_t k = 0; k < i; k++) {
                if (str[k] != ' ') { all_spaces = 0; break; }
            }
            if (all_spaces) {
                has_minus = 1;
                result = 1;
            } else
                return 0;
        } else if (str[i] == '.' && !has_dot && (mode == 1 || mode == 2)) {
            /* Single decimal point allowed */
            has_dot = 1;
            result = 1;
        } else if (str[i] == ' ' && (mode == 1 || mode == 2)) {
            /* Spaces for padding */
            result = 1;
        } else if (str[i] == ',' && !has_dot && mode == 2) {
            /* Comma as decimal separator (mode 2 only) */
            result = 1;
        } else
            return 0;
    }

    /* Must contain at least one digit */
    return has_digit ? result : 0;
}

size_t get_number_len(size_t number)
{
    char buf[24];
    return (size_t)snprintf(buf, sizeof(buf), "%zu", number);
}

char* rm_spaces_from_str(char* str)
{
    char sym = ' ';
    size_t len = strlen(str);
    char* result = calloc(len + 2, sizeof(char));
    is_memory_allocated(result);
    size_t res_i = 0;
    for ( size_t i=0; i<len; i++ ) {
        if ( str[i] != sym ) {
            result[res_i] = str[i];
            res_i++;
        }
    }

    result[res_i] = '\0';
    free(str);
    return result;
}

char* rm_spaces_start_end(char* str)
{
    if (str == NULL || str[0] == '\0') {
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        if (str != NULL) free(str);
        return result;
    }

    size_t len = strlen(str);
    size_t start = 0;
    size_t end = 0;
    int found = 0;

    for (size_t i = 0; i < len; i++) {
        if (str[i] != ' ') {
            start = i;
            found = 1;
            break;
        }
    }

    if (!found) {
        /* String contains only spaces — return empty string */
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        free(str);
        return result;
    }

    for (int64_t i = (int64_t)len - 1; i >= 0; i--) {
        if (str[i] != ' ') {
            end = (size_t)i;
            break;
        }
    }

    end += 1;
    /* copy_len is always >= 1 here because the !found branch above
       guarantees start and end have been set to valid positions. */
    size_t copy_len = end - start;
    size_t alloc_size = (copy_len >= 1) ? (copy_len + 1) : 2;
    char* result = calloc(alloc_size, sizeof(char));
    is_memory_allocated(result);
    memcpy(result, &str[start], copy_len);
    result[copy_len] = '\0';
    free(str);
    return result;
}


/***************************************************************************
* functions for Text Formatting
***************************************************************************/
void set_doc_width(int width)
{
    if ( width < 10 ) {
        printf("  Error: Document width cannot be less than 10 characters\n");
        DOC_WIDTH = 10;
    } else if ( width > 250 ) {
        printf("  Error: Document width cannot be more than 250 characters\n");
        DOC_WIDTH = 250;
    } else {
        DOC_WIDTH = (uint8_t)width;
    }
}

/* Stack for saving/restoring document width — avoids global state pitfalls */
static uint8_t doc_width_stack[DOC_WIDTH_STACK_MAX];
static unsigned int doc_width_stack_top = 0;

uint8_t push_doc_width(void)
{
    if (doc_width_stack_top >= DOC_WIDTH_STACK_MAX) {
        /* Stack full — return last valid slot; width won't be restored correctly
           but this is better than crashing. Should rarely happen in practice. */
        return DOC_WIDTH_STACK_MAX - 1;
    }
    doc_width_stack[doc_width_stack_top] = DOC_WIDTH;
    return (uint8_t)doc_width_stack_top++;
}

void restore_doc_width(uint8_t slot)
{
    DOC_WIDTH = doc_width_stack[slot];
}

/* Reset the doc_width stack state — called before processing each file
 * to prevent state leakage between files. */
void reset_doc_width_state(void)
{
    DOC_WIDTH = DEFAULT_DOC_WIDTH;
    doc_width_stack_top = 0;
}


/***************************************************************************
* functions for working with arrays
***************************************************************************/
uint16_t get_arr_size(char** str_arr)
{
    uint16_t count = 0;
    while ( str_arr[count] != NULL )
        count++;
    
    return count;
}

uint8_t in_str_array(char** arr, char* value)
{
    if ( arr == NULL )
        return 0;

    uint16_t arr_size = get_arr_size(arr);
    for ( uint16_t i=0; i<arr_size; i++ ) {
        if ( strcmp(arr[i], value) == 0 )
            return 1;
    }

    return 0;
}

size_t get_max_len(char** str_arr, size_t arr_size)
{
    if (arr_size == 0 || str_arr == NULL)
        return 0;

    size_t max_len = utf8_strlen(str_arr[0]);
    for ( size_t i=1; i<arr_size; i++ ) {
        size_t len = utf8_strlen(str_arr[i]);
        if ( max_len < len )
            max_len = len;
    }

    return max_len;
}

uint16_t get_max(const uint16_t* arr, uint16_t size)
{
    if (size == 0 || arr == NULL) return 0;
    uint16_t max = arr[0];
    for ( uint16_t i=1; i<size; i++ ) {
        if ( max < arr[i] )
            max = arr[i];
    }

    return max;
}

uint16_t get_min(const uint16_t* arr, uint16_t size)
{
    if (size == 0 || arr == NULL) return 0;
    uint16_t min = arr[0];
    for ( uint16_t i=1; i<size; i++ ) {
        if ( min > arr[i] )
            min = arr[i];
    }

    return min;
}

int32_t get_index(const uint16_t* arr, uint16_t size, uint16_t value)
{
    int32_t index = -1;
    for ( uint16_t i=0; i<size; i++ ) {
        if ( value == arr[i] )
            index = i;
    }

    return index;
}

uint16_t get_min_index(const uint16_t* arr, uint16_t size)
{
    uint16_t min = get_min(arr, size);
    int32_t idx = get_index(arr, size, min);
    return (idx >= 0) ? (uint16_t)idx : 0;
}


/***************************************************************************
* Basic functions for some tags
***************************************************************************/
char* get_aligned_text(char* str, char** attrs)
{
    size_t lines_count;
    char** lines = split_count('\n', str, &lines_count);

    /* Pre-compute UTF-8 lengths to avoid double-pass */
    size_t* line_widths = calloc(lines_count, sizeof(size_t));
    is_memory_allocated(line_widths);
    size_t max_line = 0;
    for (size_t i = 0; i < lines_count; i++) {
        line_widths[i] = utf8_strlen(lines[i]);
        if (line_widths[i] > max_line) max_line = line_widths[i];
    }

    (void)max_line; /* Used to validate line widths; buffer sized by str length */
    /* UTF-8 safety: very generous buffer allocation */
    size_t len = strlen(str) * 8 + lines_count * 50 + 1000;
    char* aligned = calloc(len, sizeof(char));
    is_memory_allocated(aligned);
    size_t pos = 0;
    for ( size_t i=0; i<lines_count; i++ ) {
        int32_t spaces_left = (int32_t)(line_widths[i] > DOC_WIDTH) ? 0
            : (int32_t)DOC_WIDTH - (int32_t)line_widths[i];
        if ( attrs == NULL )
            spaces_left /= 2;
        if (spaces_left < 0) spaces_left = 0;

        /* Direct memset instead of calloc + memcpy + free */
        memset(aligned + pos, ' ', (size_t)spaces_left);
        pos += (size_t)spaces_left;
        size_t line_len = strlen(lines[i]);
        memcpy(aligned + pos, lines[i], line_len);
        pos += line_len;
        int32_t spaces_right = (int32_t)DOC_WIDTH - (int32_t)line_widths[i] - (int32_t)spaces_left;
        if (spaces_right < 0) spaces_right = 0;
        memset(aligned + pos, ' ', (size_t)spaces_right);
        pos += (size_t)spaces_right;
        free(lines[i]);
        if ( i < lines_count - 1 )
            aligned[pos++] = '\n';
    }

    free(lines);
    free(line_widths);
    aligned[pos] = '\0';
    return aligned;
}

char* header(char* str, uint8_t header_type, char** attrs)
{
    char sep_sym = '\0';
    if ( attrs != NULL ) {
        if ( attrs[0] != NULL && attrs[0][0] != '\0' )
            sep_sym = attrs[0][0];
    }

    if ( header_type == 1 && sep_sym == '\0' )
        sep_sym = '=';

    uint8_t slot = push_doc_width();
    size_t str_width = utf8_strlen(str);
    if ( DOC_WIDTH < str_width )
        set_doc_width((int)(str_width + 4));

    char* hdr = NULL;
    if ( header_type == 1 ) {
        /* h1: two separator lines + centered text + 2 newlines + '\0' */
        /* get_aligned_text returns centered text padded to DOC_WIDTH */
        char* centered_str = get_aligned_text(str, NULL);
        /* Buffer: separator(DOC_WIDTH) + '\n' + centered_str + '\n' + separator(DOC_WIDTH) + '\0' */
        size_t centered_len = strlen(centered_str);
        size_t buf_size = (size_t)DOC_WIDTH + 1 + centered_len + 1 + (size_t)DOC_WIDTH + 1;
        hdr = calloc(buf_size, sizeof(char));
        is_memory_allocated(hdr);
        char* tmp = get_str_from_sym(sep_sym, DOC_WIDTH);
        snprintf(hdr, buf_size, "%s\n%s\n%s", tmp, centered_str, tmp);
        free(tmp);
        free(centered_str);
    } else {
        /* h2-h4: left_sep + ' ' + str + ' ' + right_sep + fill + '\0'
         * Worst case: side_len ≈ DOC_WIDTH, fill ≈ DOC_WIDTH, str bytes as-is.
         * Buffer: 2*DOC_WIDTH (sides) + strlen(str) + DOC_WIDTH (fill) + safety. */
        size_t buf_size = (size_t)DOC_WIDTH * 3 + strlen(str) + 10;
        hdr = calloc(buf_size, sizeof(char));
        is_memory_allocated(hdr);
        if ( sep_sym == '\0' )
            sep_sym = (header_type == 2) ? '=' : '-';

        int32_t side_len = (int32_t)((size_t)DOC_WIDTH - str_width - 1) / 2;
        if (side_len < 0) side_len = 0;
        char* tmp = get_str_from_sym(sep_sym, (size_t)side_len);
        snprintf(hdr, buf_size, "%s %s ", tmp, str);
        free(tmp);
        size_t hdr_width = utf8_strlen(hdr);
        int32_t fill_raw = (int32_t)DOC_WIDTH - (int32_t)hdr_width;
        size_t fill = (fill_raw > 0) ? (size_t)fill_raw : 0;
        tmp = get_str_from_sym(sep_sym, fill);
        strncat(hdr, tmp, buf_size - strlen(hdr) - 1);
        free(tmp);
    }

    restore_doc_width(slot);
    free(str);  /* header takes ownership of str */
    return hdr;
}

/***************************************************************************
* functions for working with tables
***************************************************************************/

/* Unified table parser: one pass returns all table metadata.
 * Caller must free: table_data rows/cells, and *out_cells_in_row. */
char*** parse_table(char* tbl_str, size_t* out_rows_count, uint16_t** out_cells_in_row)
{
    *out_rows_count = get_elements_count('\n', tbl_str);
    *out_cells_in_row = calloc(*out_rows_count, sizeof(uint16_t));
    is_memory_allocated(*out_cells_in_row);

    char*** table_data = (char***)calloc(*out_rows_count, sizeof(char**));
    is_memory_allocated(table_data);

    char** rows = split('\n', tbl_str);
    for (size_t i = 0; i < *out_rows_count; i++) {
        table_data[i] = split('|', rows[i]);
        size_t cnt = get_elements_count('|', rows[i]);
        (*out_cells_in_row)[i] = (cnt > 65535) ? 65535 : (uint16_t)cnt;
        free(rows[i]);
    }
    free(rows);

    return table_data;
}

/* Legacy wrappers for backward compatibility. */
size_t get_rows_count(char* tbl_str)
{
    return get_elements_count('\n', tbl_str);
}

uint16_t* get_cells_count(char* tbl_str)
{
    size_t  rows_count   = get_rows_count(tbl_str);
    uint16_t* cells_in_row = calloc(rows_count, sizeof(uint16_t));
    is_memory_allocated(cells_in_row);
    char** rows = split('\n', tbl_str);
    for ( size_t i=0; i<rows_count; i++ ) {
        size_t cnt = get_elements_count('|', rows[i]);
        cells_in_row[i] = (cnt > 65535) ? 65535 : (uint16_t)cnt;
        free(rows[i]);
    }

    free(rows);
    return cells_in_row;
}

char*** get_table_data(char* tbl_str)
{
    size_t  rows_count   = get_rows_count(tbl_str);
    char**    rows         = split('\n', tbl_str);
    char***   table_data   = (char***)calloc(rows_count, sizeof(char**));
    is_memory_allocated(table_data);
    for ( size_t i=0; i<rows_count; i++ ) {
        table_data[i] = split('|', rows[i]);
        free(rows[i]);
    }

    free(rows);
    return table_data;
}

uint16_t* get_cells_len(char** row, uint16_t cells_count)
{
    uint16_t* cells_len = calloc(cells_count, sizeof(uint16_t));
    is_memory_allocated(cells_len);
    for ( uint16_t i=0; i<cells_count; i++ )
        cells_len[i] = (uint16_t)utf8_strlen(row[i]);

    return cells_len;
}

uint16_t* get_rows_len(char*** table_data, uint16_t rows_count,
                       const uint16_t* cells_in_row)
{
    uint16_t* rows_len = calloc(rows_count, sizeof(uint16_t));
    is_memory_allocated(rows_len);
    for ( uint16_t i=0; i<rows_count; i++ ) {
        size_t row_len = 0;
        for ( uint16_t j=0; j<cells_in_row[i]; j++ )
            row_len += utf8_strlen(table_data[i][j]);

        row_len += cells_in_row[i] - 1;
        rows_len[i] = (row_len > 65535) ? 65535 : (uint16_t)row_len;
    }

    return rows_len;
}

char* get_table_border(const char* row1, const char* row2)
{
    size_t row_len = utf8_strlen(row1);
    char* border = calloc(row_len + 1, sizeof(char));
    is_memory_allocated(border);

    const char* p1 = row1;
    const char* p2 = row2;
    size_t pos = 0;
    for ( size_t i=0; i<row_len; i++ ) {
        /* Safety: if we've reached end of either string, use '-' */
        int len1 = utf8_char_len(p1);
        int len2 = utf8_char_len(p2);
        if (len1 == 0 || len2 == 0) {
            border[pos++] = '-';
            break;
        }
        /* '|' is ASCII 0x7C. UTF-8 multi-byte sequences always start with
         * bytes >= 0xC0, so a simple first-byte check is sufficient and safe. */
        unsigned char c1 = (unsigned char)*p1;
        unsigned char c2 = (unsigned char)*p2;
        if (c1 == '|' || c2 == '|')
            border[pos++] = '+';
        else
            border[pos++] = '-';

        /* Advance to next UTF-8 char */
        p1 += len1;
        p2 += len2;
    }
    border[pos] = '\0';

    return border;
}

char* add_table_border(char* table_str)
{
    char**   rows = split('\n', table_str);
    size_t rows_count = get_rows_count(table_str);

    /* Remove trailing empty rows caused by trailing newline in table_str */
    while (rows_count > 0 && rows[rows_count - 1][0] == '\0') {
        free(rows[rows_count - 1]);
        rows_count--;
    }

    if (rows_count == 0) {
        /* Empty table - just return empty string */
        char* result = calloc(1, sizeof(char));
        is_memory_allocated(result);
        free(rows);
        return result;
    }
    size_t row_len = utf8_strlen(rows[0]);
    size_t len = (rows_count * 2 + 1) * (row_len + 1);
    char*    table = calloc(len * 4, sizeof(char)); /* UTF-8 safety */
    is_memory_allocated(table);
    char* space_str = get_str_from_sym(' ', row_len);
    size_t pos = 0;
    size_t tlen;
    for ( size_t i=0; i<rows_count; i++ ) {
        char* row1;
        char* row2;
        if ( i == 0 ) {
            row1 = space_str;
            row2 = rows[i];
        } else if ( i == rows_count - 1 ) {
            row1 = rows[i];
            row2 = space_str;
        } else {
            row1 = rows[i - 1];
            row2 = rows[i];
        }

        char* border = get_table_border(row1, row2);
        tlen = strlen(border); memcpy(table + pos, border, tlen); pos += tlen;
        table[pos++] = '\n';
        tlen = strlen(rows[i]); memcpy(table + pos, rows[i], tlen); pos += tlen;
        if ( i != rows_count - 1 ) {
            free(border);
            table[pos++] = '\n';
        } else {
            table[pos++] = '\n';
            tlen = strlen(border); memcpy(table + pos, border, tlen); pos += tlen;
            free(border);
        }
    }
    /* Cleaning */
    for ( size_t i=0; i<rows_count; i++ )
        free(rows[i]);

    free(rows);
    free(space_str);

    return table;
}

/* Forward declarations from tags.c */
char* calc_single_expr(const char* expr, int show_expr);
char* calc(char* str, char** attrs);

void calc_in_table(char*** table_data, uint16_t rows_count,
                   const uint16_t* cells_in_row)
{
    char* calc_res = NULL;
    for ( uint16_t i=0; i<rows_count; i++ ) {
        for ( uint16_t j=0; j<cells_in_row[i]; j++ ) {
            const char* cell = table_data[i][j];
            /* Single pass: check for math operators AND digits simultaneously */
            int has_digit = 0;
            int has_math = 0;
            int has_dot_or_comma = 0;
            for (const char* p = cell; *p; p++) {
                unsigned char c = (unsigned char)*p;
                if (c >= '0' && c <= '9') has_digit = 1;
                else if (c == '+' || c == '*' || c == '/' || c == '^') has_math = 1;
                else if (c == '.' || c == ',') has_dot_or_comma = 1;
                else if (c == '-') {
                    /* '-' counts as math only if not at start */
                    if (p != cell) has_math = 1;
                }
            }

            /* Skip plain numbers */
            if (has_digit && !has_math && !has_dot_or_comma) continue;
            /* Skip if no math operators at all */
            if (!has_math && !has_dot_or_comma) continue;

            /* Skip date-like strings (YYYY-MM-DD, DD.MM.YYYY, etc.) */
            size_t cell_len = strlen(cell);
            if (cell_len >= 8) {
                int is_date = 0;
                /* YYYY-MM-DD: check digits at positions 0-3, 5-6, 8-9 */
                if (cell[4] == '-' && cell[7] == '-') {
                    int all_digits = 1;
                    for (int k = 0; k < 4; k++) {
                        if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                    }
                    if (all_digits) {
                        for (int k = 5; k < 7; k++) {
                            if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                        }
                    }
                    if (all_digits && cell_len >= 10) {
                        for (int k = 8; k < 10; k++) {
                            if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                        }
                    }
                    if (all_digits) is_date = 1;
                }
                /* DD.MM.YYYY: check digits at positions 0-1, 3-4, 6-9 */
                if (cell[2] == '.' && cell[5] == '.' && cell_len >= 10) {
                    int all_digits = 1;
                    for (int k = 0; k < 2; k++) {
                        if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                    }
                    if (all_digits) {
                        for (int k = 3; k < 5; k++) {
                            if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                        }
                    }
                    if (all_digits) {
                        for (int k = 6; k < 10; k++) {
                            if (cell[k] < '0' || cell[k] > '9') { all_digits = 0; break; }
                        }
                    }
                    if (all_digits) is_date = 1;
                }
                if (is_date) continue;
            }

            char* tmp = strdup(cell);
            is_memory_allocated(tmp);
            change_symbols(',', '.', tmp);

            calc_res = calc_single_expr(tmp, 0);
            free(tmp);

            /* Replace cell with result only if it's not "error" —
             * keep the original cell content on evaluation failure */
            if (strcmp(calc_res, "error") != 0) {
                free(table_data[i][j]);
                table_data[i][j] = strdup(calc_res);
            }
            free(calc_res);
        }
    }
}

uint16_t** get_column_width(char*** table_data, uint16_t rows_count,
                            uint16_t* cells_in_row)
{
    uint16_t max_cells = get_max(cells_in_row, rows_count);
    /* Memory allocation */
    uint16_t** column_width = calloc(max_cells, sizeof(uint16_t*));
    is_memory_allocated(column_width);
    for ( uint16_t i=0; i<max_cells; i++ ) {
        column_width[i] = calloc(i + 1, sizeof(uint16_t));
        is_memory_allocated(column_width[i]);
    }

    for ( uint16_t i=0; i<rows_count; i++ ) {
        for ( uint16_t j=0; j<cells_in_row[i]; j++ ) {
            size_t cell_width = utf8_strlen(table_data[i][j]);
            if ( cell_width > column_width[cells_in_row[i]-1][j] )
                column_width[cells_in_row[i] - 1][j] = (cell_width > 65535) ? 65535 : (uint16_t)cell_width;
        }
    }

    return column_width;
}

void align_to_columns_with_width(char*** table_data, uint16_t rows_count,
                      uint16_t* cells_in_row, uint8_t na,
                      uint16_t** column_width, uint16_t max_cells_unused)
{
    (void)max_cells_unused;
    char* align = NULL;
    char* tmp = NULL;
    for ( uint16_t i=0; i<rows_count; i++ ) {
        for ( uint16_t j=0; j<cells_in_row[i]; j++ ) {
            size_t cell_width = utf8_strlen(table_data[i][j]);
            size_t col_w = column_width[cells_in_row[i] - 1][j];
            int32_t al_len_raw = (int32_t)col_w - (int32_t)cell_width;
            size_t al_len = (al_len_raw > 0) ? (size_t)al_len_raw : 0;
            if ( al_len > 0 ) {
                align = get_str_from_sym(' ', al_len);
                tmp = strdup(table_data[i][j]);
                free(table_data[i][j]);
                size_t buf_size = (size_t)al_len * 4 + strlen(tmp) + 1;
                table_data[i][j] = calloc(buf_size, sizeof(char));
                int is_num = is_number(tmp, 2);
                if (is_num && na == 0) {
                    snprintf(table_data[i][j], buf_size, "%s%s", align, tmp);
                } else {
                    snprintf(table_data[i][j], buf_size, "%s%s", tmp, align);
                }

                free(tmp);
                free(align);
            }
        }
    }
}

uint16_t get_max_row_len(char*** table_data, uint16_t rows_count,
                         uint16_t* cells_in_row)
{
    uint16_t max_row_len = 0;
    uint16_t max_cells = get_max(cells_in_row, rows_count);
    uint16_t** column_width = get_column_width(table_data, rows_count,
                                               cells_in_row);
    for ( uint16_t i=0; i<max_cells; i++ ) {
        uint16_t row_len = 0;
        for ( uint16_t j=0; j<=i; j++ )
            row_len += column_width[i][j];

        row_len += i;
        if ( row_len > max_row_len )
            max_row_len = row_len;
    }
    /* Cleaning */
    for ( uint16_t i=0; i<max_cells; i++ )
        free(column_width[i]);

    free(column_width);
    return max_row_len;
}


/***************************************************************************
* functions for working with Histograms
***************************************************************************/
double get_max_value(char** values, size_t values_count)
{
    double max_value = 0;
    for ( size_t i=0; i<values_count; i++ ) {
        if ( is_number(values[i], 1) ) {
            double vl = strtod(values[i], NULL);
            vl = (vl < 0) ? vl * (double)-1 : vl;/* abs */
            max_value = (max_value < vl) ? vl : max_value;
        }
    }
    return max_value;
}

void get_histogram_data(char* str, char** names, char** values)
{
    size_t lines_count;
    char** lines = split_count('\n', str, &lines_count);
    for ( size_t i=0; i<lines_count; i++ ) {
        char* name = NULL;
        char* value = NULL;

        /* Skip completely empty lines */
        if (lines[i][0] == '\0') {
            name = strdup(" ");
            value = strdup(" ");
            names[i] = name;
            values[i] = value;
            continue;
        }

        size_t t_count = get_elements_count('|', lines[i]);
        if ( t_count >= 2 ) {
            char** t = split('|', lines[i]);
            if (t[0] == NULL || t[1] == NULL) {
                /* Malformed line: not enough cells */
                name = strdup(" ");
                value = strdup("error");
                is_memory_allocated(value);
            } else {
                name = strdup(t[0]);
                is_memory_allocated(name);

                value = strdup(t[1]);
                is_memory_allocated(value);

                change_symbols(',', '.', value);
                value = rm_spaces_from_str(value);

                if ( is_number(value, 1) == 0 ) {
                    free(value);
                    value = strdup("error");
                    is_memory_allocated(value);
                }
            }

            for ( uint16_t j=0; j<t_count; j++ )
                free(t[j]);

            free(t);
        } else if ( t_count == 1 ) {
            name = calloc(2, sizeof(char));
            is_memory_allocated(name);
            strcpy(name, " ");

            change_symbols(',', '.', lines[i]);

            if (lines[i][0] != '\0' && is_number(lines[i], 1) ) {
                value = strdup(lines[i]);
                is_memory_allocated(value);
                value = rm_spaces_from_str(value);
            } else {
                value = strdup("error");
                is_memory_allocated(value);
            }
        }

        /* Safety: if neither branch executed (e.g. t_count == 0), initialize */
        if (name == NULL) {
            name = strdup(" ");
        }
        if (value == NULL) {
            value = strdup("error");
            is_memory_allocated(value);
        }

        if ( strcmp(name, " ") != 0 )
            name = rm_spaces_start_end(name);

        names[i] = name;
        values[i] = value;
        free(lines[i]);
    }
    free(lines);
}
