#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <sqlite3.h> 
#include <errno.h>
#include "Database.h"

#define SERVICE_NAME "com.example.sdbusService"
#define OBJECT_PATH "/com/example/sdbusService"
#define INTERFACE_NAME "com.example.sdbusService.Manager"

#define MAX_RESULT_LEN 4096

static int handle_insert(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    const char *client_ip, *recv_data, *send_data;
    int r;
    
    r = sd_bus_message_read(m, "sss", &client_ip, &recv_data, &send_data);
    if(r < 0)
    {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Can't parse permanent:%s", strerror(-r));
        return r;
    }

    char *values = sqlite3_mprintf("'%s', '%s', '%s', datetime('now')", client_ip, recv_data, send_data);
    
    if(!values)
    {
        sd_bus_error_set(ret_error, SD_BUS_ERROR_NO_MEMORY, "Memory allocation failed");
        return -ENOMEM;
    }

    int db_ret = database_insert("server_data.db", "communication",
                                 "client_ip, received_data, send_data, timestamp",
                                 values);
    
    sqlite3_free(values);

    return sd_bus_reply_method_return(m, "i", db_ret);
}

static int handle_select(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    char *result = NULL;
    char *condition = NULL;
    
    int r =sd_bus_message_read(m, "s", &condition);
    if(r < 0)
    {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Parse condition failed:%s", strerror(-r));
        return r;
    }

    int db_ret = database_select("server_data.db",
                                 "communication",
                                 "*",
                                 condition,
                                 &result,
                                 MAX_RESULT_LEN);
    

    if(db_ret != 0)
    {
        const char *err_msg = "select failed";
        if(result)
        {
            err_msg = result;
            free(result);
            result = NULL;
        }
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_FAILED, "%s", err_msg);
        return -1;
    }

    r = sd_bus_reply_method_return(m, "s", result);
    free(result);
    return r;
}

static const sd_bus_vtable service_vtable[] = 
{
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("InsertData", "sss", "i", handle_insert, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SelectData", "s", "s", handle_select, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

int main()
{
    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;
    int r;

    r = sd_bus_open_user(&bus);
    if(r < 0)
    {
        fprintf(stderr, "Can't connect to the session bus:%s\n", strerror(-r));
        goto finish;
    }

    r = sd_bus_add_object_vtable(bus, &slot, OBJECT_PATH, INTERFACE_NAME, service_vtable, NULL);
    if(r < 0)
    {
        fprintf(stderr, "Can't add object vtable:%s\n",strerror(-r));
        goto finish;
    }

    r = sd_bus_request_name(bus, SERVICE_NAME, 0);
    if(r < 0)
    {
        fprintf(stderr, "Can't request service name:%s\n",strerror(-r));
        goto finish;
    }

    database_create("server_data.db", "communication",
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "          
                    "client_ip TEXT NOT NULL, " 
                    "received_data TEXT, "
                    "send_data TEXT, "
                    "timestamp DATATIME");

    for(;;)
    {
        r = sd_bus_process(bus, NULL);
        if(r < 0)
        {
            fprintf(stderr, "Deal with message from session bus failed:%s\n",strerror(-r));
            goto finish;
        }

        if(r > 0)
        {
            continue;
        }

        r = sd_bus_wait(bus, (uint64_t) -1);
        if(r < 0)
        {
            fprintf(stderr, "Wait for message from session bus failed:%s\n",strerror(-r));
            goto finish;
        }
    }

    finish:
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
