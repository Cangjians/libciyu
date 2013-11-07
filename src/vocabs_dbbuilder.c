/* Copyright (c) 2013 - The libcangjie authors.
 *
 * This file is part of libcangjie.
 *
 * libcangjie is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libcangjie is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libcangjie.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "cangjieerrors.h"

#ifdef _WIN32
/*
 * public domain strtok_r() by Charlie Gordon
 *
 *   from comp.lang.c  9/14/2007
 *
 *      http://groups.google.com/group/comp.lang.c/msg/2ab1ecbb86646684
 *
 *     (Declaration that it's public domain):
 *      http://groups.google.com/group/comp.lang.c/msg/7c7b39328fefab9c
 */

char* strtok_r(char *str, const char *delim, char **nextp) {
    char *ret;
    if (str == NULL)
        str = *nextp;
    str += strspn(str, delim);
    if (*str == '\0')
        return NULL;
    ret = str;
    str += strcspn(str, delim);
    if (*str)
        *str++ = '\0';
    *nextp = str;
    return ret;
}
#endif

char *create_vocab = "CREATE TABLE vocabs(vocab_index INTEGER PRIMARY KEY ASC,\n"
                     "                   first_char TEXT,\n"
                     "                   vocab TEXT UNIQUE);";
char *select_index = "SELECT vocab_index FROM vocabs WHERE vocab='%q';";
char *insert_vocab = "INSERT INTO vocabs VALUES(%d, '%q', '%q');";

int insert_line(sqlite3 *db, char *line, int i) {
    char *saveptr;
    char *query;
    char *code;
    sqlite3_stmt *stmt;
    int ret;

    // Parse the line
	char *first_char = strtok_r (line, " ", &saveptr);
	char *vocab = strtok_r (NULL, "\0", &saveptr);
    // Check whether this character already exists in the database
    query = sqlite3_mprintf(select_index, vocab);
    if (query == NULL) {
        return CANGJIE_NOMEM;
    }

    ret = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (ret != SQLITE_OK) {
        // FIXME: Unhandled error codes
        return ret;
    }
    sqlite3_free(query);

    ret = sqlite3_step(stmt);
    if(ret == SQLITE_DONE) {
        // The character does not exist yet, insert it
        query = sqlite3_mprintf(insert_vocab, i, first_char, vocab);
        if (query == NULL) {
            return CANGJIE_NOMEM;
        }

        sqlite3_exec(db, query, NULL, NULL, NULL);
        sqlite3_free(query);
    } else if (ret == SQLITE_ROW) {
        // The character exists
        i = (uint32_t)sqlite3_column_int(stmt, 0);
    } else {
        // Some error encountered
        return CANGJIE_DBERROR;
    }
    sqlite3_finalize(stmt);

    return CANGJIE_OK;
}

int main(int argc, char **argv) {
    char *tablefile;
    char *dbfile;
    sqlite3 *db;
    FILE *table;
    char line[128];
    int len;
    int i = 1;

    if (argc != 3) {
        printf("Usage: %s TABLE_FILE DB_FILE\n", argv[0]);
        printf("\n");
        printf("Build DB_FILE out of TABLE_FILE.\n");

        return -1;
    }

    tablefile = argv[1];
    dbfile = argv[2];
    printf("Building database '%s' from table '%s'...\n", dbfile, tablefile);

    // Create the database
    sqlite3_open_v2(dbfile, &db,
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    sqlite3_exec(db, create_vocab, NULL, NULL, NULL);

    table = fopen(tablefile, "r");

    while(fgets(line, 128, table) != NULL) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        len = strlen(line);

        if (line[len-1] == '\n') {
            line[len-1] = '\0';
            len -= 1;
        }

        insert_line(db, line, i);
        i += 1;
    }

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    fclose(table);

    sqlite3_exec(db, "CREATE INDEX i1 on codes(version, code);", NULL, NULL, NULL);

    sqlite3_close(db);

    return CANGJIE_OK;
}
