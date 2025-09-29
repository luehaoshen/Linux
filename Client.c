#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

int main(int argc, char* argv[])
{
	int sock;
	struct sockaddr_in server_addr, client_addr;
	char buf[BUFFER_SIZE];
	char* local_ip = "127.0.0.1";
	
	if(argc > 2)
	{
	        perror("invalid input");
	        exit(EXIT_FAILURE);
	}
	else if(argc == 2)
	{
	        local_ip = argv[1];
	}
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, local_ip, &client_addr.sin_addr) <= 0)
	{
		perror("invaild local IP address");
		close(sock);
		exit(EXIT_FAILURE);
	}
	
	if(bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1)
	{
	        perror("bind failed");
		close(sock);
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
	{
		perror("inet_pton failed");
		close(sock);
		exit(EXIT_FAILURE);
	}

	if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("connect failed");
		close(sock);
		exit(EXIT_FAILURE);
	}

	printf("已使用IP：%s连接到服务器，输入发送数据（输入“quit”退出，输入“select”查询记录）：\n", local_ip);

	while (1)
	{
		printf(">");
		if (fgets(buf, BUFFER_SIZE, stdin) == NULL)
		{
			break;
		}
		
		buf[strcspn(buf,"\n")] = '\0';
		if (send(sock, buf, strlen(buf), 0) == -1)
		{
			perror("send failed");
			break;
		}

		if(strcmp(buf,"select") == 0)
		{
			int len = recv(sock, buf, BUFFER_SIZE - 1, 0);
			if(len > 0)
			{
				buf[len] = '\0';
				printf("查询结果:\n%s\n", buf);
			}
		    continue;
		}

		//buf[strcspn(buf,"\n")] = '\0';
		if(strcmp(buf,"quit") == 0)
		{
		    break;
		}
	}

	close(sock);
	return 0;
}
