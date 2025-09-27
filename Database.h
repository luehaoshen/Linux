#ifndef DATABASR_H

#define DATABASR_H

int database_create(const char *db_path, const char *table_name, const char *schema);

int database_insert(const char *db_path, const char *table_name, const char *columns, const char *values);

int database_select(const char *db_path, const char *table_name, const char *columns, const char *condition);

#endif
