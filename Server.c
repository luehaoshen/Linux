#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <systemd/sd-bus.h>

#define PORT 8888
#define BUFFER_SIZE 1024

#define SDBUS_SERVICE "com.example.sdbusService"
#define SDBUS_PATH "/com/example/sdbusService"
#define SDBUS_INTERFACE "com.example.sdbusService.Manager"

static sd_bus *sd_bus_init()
{
	sd_bus *bus = NULL;
	int r = sd_bus_open_user(&bus);
	if(r < 0)
	{
		fprintf(stderr, "sd-bus connected failed:%s\n", strerror(r));
		return NULL;
	}
	return bus;
}

static void sd_bus_insert_data(sd_bus *bus, const char *ip, const char *recv_data, const char *send_data)
{
    if(!bus) 
	{
		return;
	}
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *msg = NULL;
    int ret;
	int r;
	
    r = sd_bus_call_method(
							bus,
							SDBUS_SERVICE,
							SDBUS_PATH,
							SDBUS_INTERFACE,
							"InsertData",
							&error,
							&msg,
							"sss",
							ip,
							recv_data,
							send_data
						  );
	if(r < 0)
	{
		fprintf(stderr, "call InsertData failed:%s\n",error.message);
		sd_bus_error_free(&error);
		return;
	}

	r = sd_bus_message_read(msg, "i", &ret);
	if(r < 0)
	{
		fprintf(stderr, "failed to parse the return result:%s\n",strerror(-r));
		return;
	}

	if(ret != 0)
	{
		printf("Database insert failed");
	}
}

typedef struct
{
	int client_sock;
	struct sockaddr_in client_addr;
	sd_bus *bus;
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

		if(strncmp(recv_buffer, "select", 6) == 0)
		{
			sd_bus_error error = SD_BUS_ERROR_NULL;
			sd_bus_message *reply = NULL;
			const char *result = NULL;
			const char *condition = NULL;

			char *where_pos = strstr(recv_buffer, "where");
			if(where_pos != NULL)
			{
				condition = where_pos + 5;
				while(*condition == ' ')
				{
					condition++;
				}
			}

			int r = sd_bus_call_method(
									   data->bus,
									   SDBUS_SERVICE,
									   SDBUS_PATH,
									   SDBUS_INTERFACE,
									   "SelectData",
									   &error,
									   &reply,
									   "s",
									   condition);

			if(r < 0)
			{
				snprintf(send_buffer, BUFFER_SIZE, "select failed:%s", error.message);
				sd_bus_error_free(&error);
			}
			else
			{
				sd_bus_message_read(reply, "s", &result);
				strncpy(send_buffer, result ? result : "no data", BUFFER_SIZE - 1);
				sd_bus_message_unref(reply);
			}
			send(client_sock, send_buffer, strlen(send_buffer), 0);
			continue;
		}

		sd_bus_insert_data(data->bus, client_ip, recv_buffer, send_buffer);

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

	sd_bus *bus = sd_bus_init();
	if(!bus)
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
			data->bus = bus;
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
	sd_bus_unref(bus);
	return 0;
}
