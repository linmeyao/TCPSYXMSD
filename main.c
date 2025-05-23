#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <windows.h>
#include <stdlib.h>
#include <conio.h>
#pragma comment(lib,"Ws2_32.lib")

#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

typedef struct {
	SOCKET socket;
	struct sockaddr_in addr;
	int active;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS] = { 0 };
char blacklist[MAX_CLIENTS][32] = { 0 };
char whitelist[MAX_CLIENTS][32] = { 0 };
int blacklist_count = 0;
int whitelist_count = 0;

// 判断IP是否在黑名单
int is_blacklisted(const char* ip) {
	for (int i = 0; i < blacklist_count; ++i) {
		if (strcmp(blacklist[i], ip) == 0) return 1;
	}
	return 0;
}
// 判断IP是否在白名单（如白名单有内容，仅允许白名单）
int is_whitelisted(const char* ip) {
	if (whitelist_count == 0) return 1;
	for (int i = 0; i < whitelist_count; ++i) {
		if (strcmp(whitelist[i], ip) == 0) return 1;
	}
	return 0;
}
// 添加到黑名单
void add_to_blacklist(const char* ip) {
	if (blacklist_count < MAX_CLIENTS) {
		strcpy(blacklist[blacklist_count++], ip);
	}
}
// 添加到白名单
void add_to_whitelist(const char* ip) {
	if (whitelist_count < MAX_CLIENTS) {
		strcpy(whitelist[whitelist_count++], ip);
	}
}
// 移除黑名单
void remove_from_blacklist(const char* ip) {
	for (int i = 0; i < blacklist_count; ++i) {
		if (strcmp(blacklist[i], ip) == 0) {
			for (int j = i; j < blacklist_count - 1; ++j)
				strcpy(blacklist[j], blacklist[j + 1]);
			--blacklist_count;
			break;
		}
	}
}
// 移除白名单
void remove_from_whitelist(const char* ip) {
	for (int i = 0; i < whitelist_count; ++i) {
		if (strcmp(whitelist[i], ip) == 0) {
			for (int j = i; j < whitelist_count - 1; ++j)
				strcpy(whitelist[j], whitelist[j + 1]);
			--whitelist_count;
			break;
		}
	}
}
// 广播消息到所有客户端（包括发送者自己）
void broadcast(const char* msg, int len) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i].active) {
			send(clients[i].socket, msg, len, 0);
		}
	}
}
// 显示所有在线IP
void print_all_ips() {
	printf("当前聊天室在线IP：\n");
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i].active) {
			printf("%s\n", inet_ntoa(clients[i].addr.sin_addr));
		}
	}
}

// 服务端输入线程
DWORD WINAPI server_input_thread(LPVOID lpParam) {
	while (1) {
		char input[BUFFER_SIZE] = { 0 };
		printf("【服务端】请输入要广播的消息（输入/quit退出）：");
		if (fgets(input, sizeof(input), stdin) == NULL) break;
		size_t len = strlen(input);
		if (len > 0 && input[len - 1] == '\n') input[len - 1] = '\0';
		if (strcmp(input, "/quit") == 0) {
			printf("服务端退出输入。\n");
			break;
		}
		if (strlen(input) == 0) continue;
		char msg[BUFFER_SIZE + 64] = { 0 };
		snprintf(msg, sizeof(msg), "[Server]: %s", input);
		broadcast(msg, (int)strlen(msg));
		printf("【服务端】消息已广播。\n");
	}
	return 0;
}

int main()
{
	// win下使用socket需要初始化
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed!\n");
		return -1;
	}

	// 1. 创建socket套接字
	// int af, // 地址族 AF_INET
	// int type, // SOCK_STREAM TCP类型套接字
	// int protocol // 默认协议 0
	SOCKET listen_socket  = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_socket == INVALID_SOCKET)
    {
        printf("create listen socket failed !!! errcode: %d\n",GetLastError());
        return -1;
    }
	
	unsigned  short a = 8080;
	unsigned short * p = &a;

	// 2. socket绑定端口
	// struct sockaddr_in {
    //     ADDRESS_FAMILY sin_family; // 地址族 AF_INET
    //     USHORT sin_port; // 端口号
	//     IN_ADDR sin_addr; // IP地址
	//     CHAR sin_zero[8]; // 保留字段
	// };
	struct sockaddr_in local = { 0 };
    local.sin_family = AF_INET;
    local.sin_port = htons(8080);// 8080端口
	//local.sin_addr.s_addr =htonl (INADDR_ANY);//本地地址 127.0.0.1 回环地址 0.0.0.0全0地址全网地址
	local.sin_addr.s_addr = inet_addr("0.0.0.0");//字符串转换为网络字节序的IP地址

	/*int bind(
		SOCKET s,
		const struct sockaddr * name,
		int namelen
	);*/
	if (bind(listen_socket, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("bind  socket failed !!! errcode: %d\n", GetLastError());
		closesocket(listen_socket);
		WSACleanup();
		return -1;
	}
	//3. socket监听端口 给他开监听
	/*int listen(
			SOCKET s,
		    int backlog);*/
	if (listen(listen_socket, 10) == SOCKET_ERROR)
	{
		printf("start listen failed !!! errcode: %d\n", GetLastError());
		closesocket(listen_socket);
		WSACleanup();
		return -1;
	}

	// 启动服务端输入线程
	HANDLE hInputThread = CreateThread(NULL, 0, server_input_thread, NULL, 0, NULL);

	fd_set readfds;
	struct timeval tv = { 1, 0 };
	for (int i = 0; i < MAX_CLIENTS; ++i) clients[i].active = 0;

	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(listen_socket, &readfds);
		int maxfd = listen_socket; // 每次循环都重新计算
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clients[i].active) {
				FD_SET(clients[i].socket, &readfds);
				if (clients[i].socket > maxfd) maxfd = clients[i].socket;
			}
		}
		int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		if (ret < 0) break;
		// 新连接
		if (FD_ISSET(listen_socket, &readfds)) {
			struct sockaddr_in client_addr;
			int addrlen = sizeof(client_addr);
			/*SOCKET
				accept(
					SOCKET s,   //监听socket
					struct sockaddr * addr,   //客户端地址
					int  * addrlen    // 结构的大小
			);*/
			SOCKET client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &addrlen);
			if (client_socket != INVALID_SOCKET) {
				const char* ip = inet_ntoa(client_addr.sin_addr);
				if (is_blacklisted(ip) || !is_whitelisted(ip)) {
					closesocket(client_socket);
					continue;
				}
				int idx = -1;
				for (int i = 0; i < MAX_CLIENTS; ++i) {
					if (!clients[i].active) {
						idx = i;
						break;
					}
				}
				if (idx != -1) {
					clients[idx].socket = client_socket;
					clients[idx].addr = client_addr;
					clients[idx].active = 1;
					printf("新用户加入：%s\n", ip);
					print_all_ips();
				} else {
					closesocket(client_socket);
				}
			}
		}
		// 客户端消息
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clients[i].active && FD_ISSET(clients[i].socket, &readfds)) {
				char buffer[BUFFER_SIZE] = { 0 };
				/*int recv(
					socket	s,  //客户端的socket
					char* buf,   //接受的数据存到哪里
					int len,      //接受数据的最大长度
					int flags     //0
				);*/
				int recv_len = recv(clients[i].socket, buffer, BUFFER_SIZE - 1, 0);
				if (recv_len <= 0) {
					printf("用户离开：%s\n", inet_ntoa(clients[i].addr.sin_addr));
					closesocket(clients[i].socket);
					clients[i].active = 0;
					print_all_ips();
					continue;
				}
				buffer[recv_len] = '\0';
				// 处理命令
				if (strncmp(buffer, "/black ", 7) == 0) {
					add_to_blacklist(buffer + 7);
					printf("已拉黑：%s\n", buffer + 7);
					continue;
				}
				if (strncmp(buffer, "/white ", 7) == 0) {
					add_to_whitelist(buffer + 7);
					printf("已加入白名单：%s\n", buffer + 7);
					continue;
				}
				if (strncmp(buffer, "/unblack ", 9) == 0) {
					remove_from_blacklist(buffer + 9);
					printf("已移除黑名单：%s\n", buffer + 9);
					continue;
				}
				if (strncmp(buffer, "/unwhite ", 9) == 0) {
					remove_from_whitelist(buffer + 9);
					printf("已移除白名单：%s\n", buffer + 9);
					continue;
				}
				// 广播消息到所有客户端（含自己），格式：[ip]: 消息
				char msg[BUFFER_SIZE + 64] = { 0 };
				snprintf(msg, sizeof(msg), "[%s]: %s", inet_ntoa(clients[i].addr.sin_addr), buffer);
				printf("%s\n", msg);
				broadcast(msg, (int)strlen(msg));
			}
		}
	}

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i].active) closesocket(clients[i].socket);
	}
	closesocket(listen_socket);
	WSACleanup();

	return 0;
}