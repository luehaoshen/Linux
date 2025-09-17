#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

static int select_callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    for(i = 0; i < argc; i++)
    {
       printf("%s = %s\t", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

//创建数据库文件
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
        return 1;
    }

    //创建SQL表
    snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS %s(%s);", table, schema);

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK)
    {

        fprintf(stderr,"SQL error: %s\n",err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("Table created or already exists\n");

    sqlite3_close(db);
    return 0;
}

//向数据库中插入数据
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
        return 1;
    }

    snprintf(sql, sizeof(sql), "INSERT INTO %s(%s) VALUES(%s);", table, columns, value);

    rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if(rc != SQLITE_OK)
    {

        fprintf(stderr,"SQL error: %s\n",err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("Insert Successfully\n");

    sqlite3_close(db);
    return 0;
}

//在数据库中查找数据
int database_select(const char *db_path, const char *table, const char *columns, const char *condition)
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
        return 1;
    }

    if(columns == NULL)
    {
        columns = "*";
    }
    
    if(condition == NULL)
    {
        snprintf(sql, sizeof(sql), "SELECT %s FROM %s;", columns, table);
    }
    else
    {
        snprintf(sql, sizeof(sql), "SELECT %s FROM %s WHERE %s;", columns, table, condition);
    }

    rc = sqlite3_exec(db, sql, select_callback, NULL, &err_msg);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr,"SQL error: %s\n",err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    printf("Select Successfully\n");

    sqlite3_close(db);
    return 0;
}

int main()
{
    database_create("test.db", "users", "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                        "name TEXT NOT NULL, " 
                                        "age INTEGER, "
                                        "sex TEXT");

    // database_insert("test.db", "users", "name, age, sex", "'lisi', 22, 'male'");

    database_select("test.db", "users", NULL, "name == 'lisi'");
}
