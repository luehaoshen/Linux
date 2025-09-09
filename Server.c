#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8888
#define BUFFER_SIZE 1024

typedef struct
{
	int client_sock;
	struct sockaddr_in client_addr;
}ClientData;

void* new_thread(void* arg)
{
	ClientData* data = (ClientData*)arg;
	int client_sock = data->client_sock;
	char client_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(data->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
	printf("客户端%s已连接\n", client_ip);

	char buffer[BUFFER_SIZE];
	int len;
	
	while (1)
	{
	        len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
	        if(len < 0)
	        {
	                perror("receive failed");
	                break;
	        }
	        else if(len == 0)
	        {
	                printf("客户端%s断开连接\n", client_ip);
	                break;
	        }
	        buffer[len] = '\0';
	        printf("收到客户端%s数据：%s\n", client_ip, buffer);
		if (strcmp(buffer, "quit") == 0)
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
	char buffer[BUFFER_SIZE];

	//创建套接字
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1)
	{
		perror("socket");
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
		perror("bind failed");
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	//监听连接
	if (listen(server_sock, 5) == -1)
	{
		perror("listen failed");
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	printf("服务器启动成功，等待客户端连接...\n");

	//接收客户端套接字
	while (1)
	{
	        int client_addrlen = sizeof(client_addr);
		client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addrlen);
		if (client_sock == -1)
		{
			perror("accept failed");
			continue;
		}
	
	        //为每个客户端创建新线程
	        ClientData* data = (ClientData*)malloc(sizeof(ClientData));
	        data->client_sock = client_sock;
	        data->client_addr = client_addr;
	        pthread_t id;
	        if (pthread_create(&id, NULL, new_thread, (void*)data) != 0)
	        {
		        perror("pthread_create failed");
		        free(data);
		        close(client_sock);
	        }
	}

	close(server_sock);
	return 0;
}
