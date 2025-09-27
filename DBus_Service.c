#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <sqlite3.h> 
#include "Database.h"

#define SERVICE_NAME "com.example.DBService"
#define OBJECT_PATH "/com/example/DBService"
#define INTERFACE_NAME "com.example.DBService"

static const DBusObjectPathVTable object_vtable =
{
    .message_function = message_filter
};

static void send_error_reply(DBusConnection *conn, DBusMessage *msg, const char *error_name, const char *error_msg)
{
    DBusMessage *reply = dbus_message_new_error(msg, error_name, error_msg);
    if(reply)
    {
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
    }
}

static DBusHandlerResult handle_insert(DBusConnection *conn, DBusMessage *msg)
{
    char *client_ip, *recv_data, *send_data;

    printf("Start InsertData\n");

    if(!dbus_message_get_args(msg, NULL,
                              DBUS_TYPE_STRING, &client_ip,
                              DBUS_TYPE_STRING, &recv_data,
                              DBUS_TYPE_STRING, &send_data,
                              DBUS_TYPE_INVALID))
    {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS, "无效的参数");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    char *values = sqlite3_mprintf("'%s', '%s', '%s', datetime('now')", client_ip, recv_data, send_data);
    if(!values)
    {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS, "内存分配失败");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int ret = database_insert("server_data.db", "communication",
                              "client_ip, received_data, send_data, timestamp",
                               values);
    
    sqlite3_free(values);

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if(reply)
    {
        dbus_message_append_args(reply, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_connection_flush(conn);
        dbus_message_unref(reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_select(DBusConnection *conn, DBusMessage *msg)
{
    char *condition;
    char result[4096] = {0};

    if(!dbus_message_get_args(msg, NULL,
                              DBUS_TYPE_STRING, &condition,
                              DBUS_TYPE_INVALID))
    {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS, "无效的参数");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int ret = database_select("server_data.db", "communication",
                              "client_ip, received_data, send_data, timestamp",
                               condition);

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if(reply)
    {
        dbus_message_append_args(reply, 
                                 DBUS_TYPE_INT32, &ret, 
                                 DBUS_TYPE_STRING, &result, 
                                 DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_connection_flush(conn);
        dbus_message_unref(reply);
    }
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult message_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    printf("receive message: port=%s, method=%s\n", dbus_message_get_interface(msg), dbus_message_get_member(msg));
    if(dbus_message_is_method_call(msg, INTERFACE_NAME, "InsertData"))
    {
        return handle_insert(conn, msg);
    }
    if(dbus_message_is_method_call(msg, INTERFACE_NAME, "SelectData"))
    {
        return handle_select(conn, msg);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int main()
{
    DBusError err;
    DBusConnection *conn;
    int ret;

    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if(dbus_error_is_set(&err))
    {
        fprintf(stderr, "D-bus连接失败: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    ret = dbus_bus_request_name(conn, SERVICE_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if(dbus_error_is_set(&err))
    {
        fprintf(stderr, "注册服务器名失败: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    dbus_connection_add_filter(conn, message_filter, NULL, NULL);
    if(!dbus_connection_register_object_path(conn, OBJECT_PATH, &object_vtable, NULL))
    {
        fprintf(stderr,"register object path failed\n");
        return -1;
    }

    database_create("server_data.db", "communication",
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "client_ip TEXT NOT NULL, "
                    "received_data TEXT, "
                    "send_data TEXT, "
                    "timestamp DATETIME");

    printf("D-Bus数据库服务启动成功\n");
    while(1)
    {
        dbus_connection_read_write(conn, -1);
        DBusMessage *msg = dbus_connection_pop_message(conn);
        if(msg)
        {
            dbus_connection_dispatch(conn);
            dbus_message_unref(msg);
        }
    }

    dbus_connection_unref(conn);
    return 0;
}
