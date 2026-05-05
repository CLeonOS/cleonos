#include <sqlite3.h>

#include <stdio.h>

typedef struct sqlitetest_row_count {
    int count;
} sqlitetest_row_count;

static int sqlitetest_exec(sqlite3 *db, const char *sql) {
    char *error = (char *)0;
    int rc;

    rc = sqlite3_exec(db, sql, 0, 0, &error);
    if (rc != SQLITE_OK) {
        printf("sqlitetest: exec failed rc=%d sql=%s\n", rc, sql);
        if (error != (char *)0) {
            printf("sqlitetest: error: %s\n", error);
            sqlite3_free(error);
        }
        return 0;
    }

    return 1;
}

static int sqlitetest_insert_rows(sqlite3 *db) {
    static const char *names[] = {"alpha", "beta", "gamma"};
    static const int values[] = {11, 22, 33};
    sqlite3_stmt *stmt = (sqlite3_stmt *)0;
    int rc;
    int i;

    rc = sqlite3_prepare_v2(db, "INSERT INTO demo(name, value) VALUES(?, ?)", -1, &stmt, (const char **)0);
    if (rc != SQLITE_OK || stmt == (sqlite3_stmt *)0) {
        printf("sqlitetest: prepare insert failed rc=%d\n", rc);
        return 0;
    }

    for (i = 0; i < 3; i++) {
        if (sqlite3_bind_text(stmt, 1, names[i], -1, SQLITE_STATIC) != SQLITE_OK ||
            sqlite3_bind_int(stmt, 2, values[i]) != SQLITE_OK) {
            puts("sqlitetest: bind failed");
            sqlite3_finalize(stmt);
            return 0;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("sqlitetest: step insert failed rc=%d\n", rc);
            sqlite3_finalize(stmt);
            return 0;
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int sqlitetest_print_rows(sqlite3 *db, sqlitetest_row_count *out_rows) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)0;
    int rc;

    rc = sqlite3_prepare_v2(db, "SELECT id, name, value FROM demo ORDER BY id", -1, &stmt, (const char **)0);
    if (rc != SQLITE_OK || stmt == (sqlite3_stmt *)0) {
        printf("sqlitetest: prepare select failed rc=%d\n", rc);
        return 0;
    }

    out_rows->count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        int value = sqlite3_column_int(stmt, 2);

        printf("row id=%d name=%s value=%d\n", id, (name == (const unsigned char *)0) ? "" : (const char *)name,
               value);
        out_rows->count++;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        printf("sqlitetest: select step failed rc=%d\n", rc);
        return 0;
    }

    return 1;
}

int cleonos_app_main(void) {
    static const char *db_path = "/temp/sqlite_test.db";
    sqlite3 *db = (sqlite3 *)0;
    sqlitetest_row_count rows;
    int rc;

    printf("sqlitetest: sqlite %s\n", sqlite3_libversion());
    (void)sqlite3_shutdown();

    rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, (const char *)0);
    if (rc != SQLITE_OK || db == (sqlite3 *)0) {
        printf("sqlitetest: open failed rc=%d msg=%s\n", rc,
               (db != (sqlite3 *)0) ? sqlite3_errmsg(db) : "no db handle");
        if (db != (sqlite3 *)0) {
            sqlite3_close(db);
        }
        return 1;
    }

    if (sqlitetest_exec(db, "PRAGMA journal_mode=DELETE;") == 0 ||
        sqlitetest_exec(db, "DROP TABLE IF EXISTS demo;") == 0 ||
        sqlitetest_exec(db, "CREATE TABLE demo(id INTEGER PRIMARY KEY, name TEXT NOT NULL, value INTEGER NOT NULL);") ==
            0 ||
        sqlitetest_insert_rows(db) == 0 || sqlitetest_print_rows(db, &rows) == 0) {
        sqlite3_close(db);
        return 1;
    }

    printf("sqlitetest: rows=%d\n", rows.count);
    sqlite3_close(db);

    rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, (const char *)0);
    if (rc != SQLITE_OK || db == (sqlite3 *)0) {
        printf("sqlitetest: reopen failed rc=%d\n", rc);
        if (db != (sqlite3 *)0) {
            sqlite3_close(db);
        }
        return 1;
    }

    if (sqlitetest_print_rows(db, &rows) == 0) {
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    if (rows.count != 3) {
        printf("sqlitetest: expected 3 rows, got %d\n", rows.count);
        return 1;
    }

    puts("sqlitetest: PASS");
    return 0;
}
