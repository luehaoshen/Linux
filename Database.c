#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "Database.h"

#define BUFFER_SIZE 1024
#define MAX_RESULT_LEN 4096

static int select_callback(void *data, int argc, char **argv, char **azColName)
{
    char *result = (char *)data;
    size_t current_len = strlen(result);
    size_t max_len = MAX_RESULT_LEN;
    size_t remaining = max_len - current_len - 1;

    if(remaining <= 0)
    {
        return 0;
    }

    for(int i = 0; i < argc; i++)
    {
        const char *col = azColName[i] ? azColName[i] : "NULL";
        const char *val = argv[i] ? argv[i] : "NULL";

        int needed = snprintf(NULL, 0, "%s=%s\t", col, val);

        if(needed < 0 || (size_t)needed >= remaining)
        {
            break;
        }

        current_len += snprintf(result + current_len, max_len - current_len, "%s=%s\t", col, val);
        remaining -= needed;
    }

    if(remaining >= 1)
    {
        result[current_len++] = '\n';
        result[current_len] = '\0';
    }

    return 0;
}

int database_create(const char *db_path, const char *table, const char *schema)
{
    sqlite3 *db;
    char *err_msg = 0;
    int rc;
    char sql[1024];

    rc = sqlite3_open(db_path, &db);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr,"Can't open database: %s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS %s(%s);", table, schema);

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK)
    {

        fprintf(stderr,"SQL error: %s\n",err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    printf("Table created or already exists\n");            

    sqlite3_close(db);
    return 0;
}

int database_insert(const char *db_path, const char *table, const char *columns, const char *value)
{
    sqlite3 *db;
    char *err_msg = 0;
    int rc;
    char sql[1024];

    rc = sqlite3_open(db_path, &db);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr,"Can't open database: %s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    snprintf(sql, sizeof(sql), "INSERT INTO %s(%s) VALUES(%s);", table, columns, value);

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK)
    {

        fprintf(stderr,"SQL error: %s\n",err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    printf("Insert Successfully\n");

    sqlite3_close(db);
    return 0;
}

int database_select(const char *db_path, const char *table, const char *columns, const char *condition, char **result, size_t max_len) 
{
    if (!db_path || !table) 
    {
        fprintf(stderr, "Error: db_path or table is NULL\n");
        return -1;
    }
    sqlite3 *db = NULL;
    char *err_msg = NULL;
    int rc;
    char *sql = NULL;
    
    *result = malloc(max_len);
    if (!*result) 
    {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }
    (*result)[0] = '\0';
    
    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        free(*result);
        return -1;
    }
    
    const char *cols = columns ? columns : "*";
    if (condition && *condition != '\0') 
    { 
        sql = sqlite3_mprintf("SELECT %s FROM %s WHERE %s;", cols, table, condition);
    }
    else 
    {
        sql = sqlite3_mprintf("SELECT %s FROM %s;", cols, table);
    }
    if (!sql) 
    { 
        fprintf(stderr, "sqlite3_mprintf failed\n");
        sqlite3_close(db);
        free(*result);
        return -1;
    }
    rc = sqlite3_exec(db, sql, select_callback, *result, &err_msg);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "SQL error: %s (SQL: %s)\n", err_msg, sql);
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        sqlite3_close(db);
        free(*result);
        return -1;
    }
    sqlite3_free(sql);
    sqlite3_close(db);
    return 0;
}
