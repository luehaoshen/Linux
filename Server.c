#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dbus/dbus.h>

#define PORT 8888
#define BUFFER_SIZE 1024

#define DBUS_SERVICE "com.example.DBService"
#define DBUS_PATH "/com/example/DBService"
#define DBUS_INTERFACE "com.example.DBService"

DBusConnection *dbus_init()
{
	DBusError err;
	dbus_error_init(&err);
	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if(dbus_error_is_set(&err))
	{
		fprintf(stderr, "D-Bus客户端连接失败: %s\n", err.message);
		dbus_error_free(&err);
		return NULL;
	}
	return conn;
}

void dbus_insert_data(DBusConnection *conn, const char *ip, const char *recv_data, const char *send_data)
{
    if(!conn) return;
    DBusMessage *msg = dbus_message_new_method_call(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, "InsertData");
	
    if(!msg)
    {
        fprintf(stderr, "创建D-Bus消息失败");
        return;
    }

    dbus_message_append_args(msg, 
							 DBUS_TYPE_STRING, &ip,
							 DBUS_TYPE_STRING, &recv_data,
							 DBUS_TYPE_STRING, &send_data,
							 DBUS_TYPE_INVALID);
	
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, NULL);
	if(reply)
	{
		int ret;
		if(dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID))
		{
			if(ret != 0)
			{
				printf("数据库采集失败，返回值: %d", ret);
			}
		}
		dbus_message_unref(reply);
	}
	else
	{
		fprintf(stderr, "未收到D-Bus服务的回复\n");
	}

	dbus_message_unref(msg);
}

typedef struct
{
	int client_sock;
	struct sockaddr_in client_addr;
	DBusConnection *dbus_conn;
}ClientData;

void* new_thread(void* arg)
{
	ClientData* data = (ClientData*)arg;
	int client_sock = data->client_sock;
	char client_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(data->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
	printf("客户端%s已连接\n", client_ip);

	char recv_buffer[BUFFER_SIZE], send_buffer[BUFFER_SIZE];
	int len;
	
	while (1)
	{
	    len = recv(client_sock, recv_buffer, BUFFER_SIZE - 1, 0);
	    if(len <= 0)
	    {
	        printf("客户端%s断开连接\n", client_ip);
	        break;
	    }
	    recv_buffer[len] = '\0';
	    printf("收到客户端%s数据:%s\n", client_ip, recv_buffer);

		strcpy(send_buffer, "received sucessfully!");
		send(client_sock, send_buffer, strlen(send_buffer), 0);

		dbus_insert_data(data->dbus_conn, client_ip, recv_buffer, send_buffer);

		if (strcmp(recv_buffer, "quit") == 0)
		{
			printf("客户端%s断开连接\n", client_ip);
			break;
		}
	}
	close(client_sock);
	free(data);
	pthread_exit(NULL);
}

int main()
{
	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_addrlen;
	int opt = 1;

	DBusConnection *dbus_conn = dbus_init();
	if(!dbus_conn)
	{
		exit(EXIT_FAILURE);
	}

	//创建套接字
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1)
	{
		perror("socket创建失败");
		exit(EXIT_FAILURE);
	}

	if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		perror("setsocketopt失败");
		close(server_sock);
		exit(EXIT_FAILURE);
	}
	//设置服务器地址
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);
	
	//将套接字与服务器地址绑定
	if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("绑定失败");
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	//监听连接
	if (listen(server_sock, 5) == -1)
	{
		perror("监听失败");
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	printf("服务器启动成功，等待客户端连接...\n");

	//接收客户端套接字
	while (1)
	{
	    client_addrlen = sizeof(client_addr);
		client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addrlen);
		if (client_sock == -1)
		{
			perror("accept failed");
			continue;
		}
	
	        //为每个客户端创建新线程
	        ClientData* data = (ClientData*)malloc(sizeof(ClientData));
			if(!data)
			{
				perror("mlloc failed");
				close(client_sock);
				continue;
			}
	        data->client_sock = client_sock;
	        data->client_addr = client_addr;
			data->dbus_conn = dbus_conn;
	        pthread_t id;
	        if (pthread_create(&id, NULL, new_thread, (void*)data) != 0)
	        {
		        perror("pthread_create failed");
		        free(data);
		        close(client_sock);
	        }
			else
			{
				pthread_detach(id);
			}
	}

	close(server_sock);
	dbus_connection_unref(dbus_conn);
	return 0;
}
