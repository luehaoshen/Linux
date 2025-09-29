#ifndef DATABASE_H

#define DATABASE_H

int database_create(const char *db_path, const char *table_name, const char *schema);

int database_insert(const char *db_path, const char *table_name, const char *columns, const char *values);

int database_select(const char *db_path, const char *table_name, const char *columns, const char *condition,
                    char **result, size_t max_len);

#endif
