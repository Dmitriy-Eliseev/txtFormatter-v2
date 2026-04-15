#define _POSIX_C_SOURCE 200809L
/* txtfmt.c
 *
 * txtFormatter - A simple text formatting utility.
 * Copyright (C) 2024, 2026 Dmitriy Eliseev
 * <code.eliseev2003.dmitriy@yandex.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "tag_handler.h"
#include "help.h"

static void print_logo(void)
{
    puts("\
\n    __       __  ______                           __  __           \
\n   / /__  __/ /_/ ____/___  _________ ___  ____ _/ /_/ /____  _____\
\n  / __/ |/_/ __/ /_  / __ \\/ ___/ __ `__ \\/ __ `/ __/ __/ _ \\/ ___/\
\n / /__>  </ /_/ __/ / /_/ / /  / / / / / / /_/ / /_/ /_/  __/ /    \
\n \\__/_/|_|\\__/_/    \\____/_/  /_/ /_/ /_/\\__,_/\\__/\\__/\\___/_/\n");
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    if ( argc > 1 ) {
        help();
        exit(EXIT_SUCCESS);
    }
        
    print_logo();
    puts("txtFormatter text formatting utility v2.0\n"
         "Copyright (C) 2024, 2026 Dmitriy Eliseev\n");
    char source_file_extension[] = ".txtm";
    char result_file_extension[] = ".txt";
    char** files = get_files_in_dir(".", source_file_extension);
    /* Count NULL-terminated array directly — no second readdir pass */
    size_t files_count = 0;
    while (files[files_count] != NULL) files_count++;

    if ( files_count == 0 ) {
        puts("Error: .txtm files not found");
        exit(EXIT_FAILURE);
    }

    size_t i;

    for ( i=0; i<files_count; i++) {
        reset_doc_width_state();
        printf("processing file: %s\n", files[i]);
        char* result = execute_all_tags(get_file_content(files[i]));
        if ( result == NULL ) {
            free(files[i]);
            continue;
        }

        /* Single pass: replace placeholder characters from insert tags.
         * '\f' and '\a' are used as safe placeholders for '<' and '>'
         * when including external files. They are unlikely to appear
         * in normal text files, so we restore them here. */
        for (char* p = result; *p; p++) {
            if (*p == '\f') *p = '<';
            else if (*p == '\a') *p = '>';
        }

        char* result_file = change_file_extension(files[i],
            result_file_extension);
        
        write_to_file(result_file, result);
        puts("  done");

        free(result_file);
        free(result);
        free(files[i]);
    }

    free(files);
    return 0;
}
