/**
 * Hackathon SO: LogMemCacher
 * (c) 2020-2021, Operating Systems
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")

#include "../../include/server.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define pageSize 65536

typedef struct thread_client {
	HANDLE client_thread;
	DWORD client_tid;
	struct sockaddr_in client;
	int client_size;
	SOCKET client_socket;
} thread_client;

extern char *logfile_path;
thread_client *clients;


DWORD WINAPI client_function(SOCKET client_sock)
{
	int rc;
	struct logmemcache_client_st *client;

	client = logmemcache_create_client(client_sock);

	while (1) {
		rc = get_command(client);
		if (rc == -1)
			break;
	}

	closesocket(client_sock);
	free(client);

	return 0;
}

DWORD WINAPI start_thread(LPVOID lpParameter)
{
	thread_client thr = *(thread_client *)lpParameter;
	client_function(thr.client_socket);
	return 0;
}

void logmemcache_init_server_os(SOCKET *server_socket)
{
	SOCKET sock, client_sock;
	int client_size;
	struct sockaddr_in server, client;
	WSADATA wsaData;
	int rc;
	int pos = 0;

	memset(&server, 0, sizeof(struct sockaddr_in));

	rc = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (rc != NO_ERROR)
		return;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		WSACleanup();
		return;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(SERVER_IP);

	if (bind(sock, (SOCKADDR *)&server, sizeof(server)) == SOCKET_ERROR) {
		closesocket(sock);
		perror("Could not bind");
		WSACleanup();
		exit(1);
	}

	if (listen(sock, DEFAULT_CLIENTS_NO) == SOCKET_ERROR) {
		perror("Error while listening");
		exit(1);
	}

	*server_socket = sock;

	while (1) {
		if (!clients)
			clients = malloc(sizeof(thread_client));
		else
			clients = realloc(clients, (pos + 1) * sizeof(thread_client));
		memset(&client, 0, sizeof(struct sockaddr_in));
		client_size = sizeof(struct sockaddr_in);
		client_sock = accept(sock, (SOCKADDR *)&client,
				(socklen_t*)&client_size);
		if (client_sock == INVALID_SOCKET) {
			perror("Error while accepting clients");
			continue;
		}

		clients[pos].client_size = client_size;
		clients[pos].client_socket = client_sock;
		clients[pos].client = client;
		clients[pos].client_thread
			= CreateThread(NULL, 0, start_thread,
				&clients[pos], 0, &clients[pos].client_tid);
		if (!clients[pos].client_thread)
			exit(EXIT_FAILURE);
		pos++;
		// client_function(client_sock);
	}

}

int logmemcache_init_client_cache(struct logmemcache_cache *cache)
{
	cache->ptr = NULL;
	cache->pages = 0;
	return 0;
}

int logmemcache_add_log_os(struct logmemcache_client_st *client,
	struct client_logline *log)
{
	char *buffer;

	printf("Adding for client sock %d\n", client->client_sock);

	printf("Adding log line\n");
	if (!client->cache->ptr) {
		printf("ptr is null\n");
		client->cache->ptr = VirtualAlloc(NULL,
			pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (client->cache->ptr == NULL) {
			fprintf(stderr, "mmap\n");
			return -1;
		}
		client->cache->pages++;
		client->cache->logs++;
		client->cache->logs_per_page++;
		memcpy(client->cache->ptr, log->logline, LOGLINE_SIZE);
		printf("Pages are %d\n", client->cache->pages);
	} else if (client->cache->logs_per_page >= pageSize / LINE_SIZE) {
		printf("I need another page\n");
		printf("Log is %d\n", client->cache->logs);
		printf("Log per page is %d\n", client->cache->logs_per_page);
		printf("pagesize per page is %d\n", pageSize);
		printf("Page is %d\n", client->cache->pages);
		buffer = malloc(sizeof(char) * (client->cache->pages * pageSize));
		memcpy(buffer, client->cache->ptr, client->cache->pages * pageSize);
		client->cache->ptr = VirtualAlloc(NULL,
			(client->cache->pages + 1) * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (client->cache->ptr == NULL) {
			fprintf(stderr, "mmap\n");
			exit(EXIT_FAILURE);
		}
		memcpy(client->cache->ptr, buffer, client->cache->pages * pageSize);
		memcpy(((char *)client->cache->ptr) + client->cache->pages * pageSize, log->logline, LOGLINE_SIZE);
		free(buffer);
		client->cache->pages++;
		client->cache->logs++;
		client->cache->logs_per_page = 1;
		
	} else {
		printf("I have enough space in the memcpy pages %d\n", client->cache->pages);
		
		memcpy(((char *)client->cache->ptr) + (client->cache->pages - 1) * pageSize 
			+ client->cache->logs_per_page - 1, log->logline, LOGLINE_SIZE);
		client->cache->logs_per_page++;
		client->cache->logs++;
	}
	return 0;
}


int logmemcache_disconnect_client_os(struct logmemcache_client_st *client)
{
	return 0;
}

int logmemcache_flush_os(struct logmemcache_client_st *client)
{

	return 0;
}

int logmemcache_unsubscribe_os(struct logmemcache_client_st *client)
{

	return 0;
}
