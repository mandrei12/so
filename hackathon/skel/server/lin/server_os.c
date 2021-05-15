/**
 * Hackathon SO: LogMemCacher
 * (c) 2020-2021, Operating Systems
 */
#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>


#include "../../include/server.h"

typedef struct thread_client {
	pthread_t client_thread;
	struct sockaddr_in client;
	int client_size;
	SOCKET client_socket;
} thread_client;

extern char *logfile_path;
thread_client *clients;


static int client_function(SOCKET client_sock)
{
	int rc;
	struct logmemcache_client_st *client;

	client = logmemcache_create_client(client_sock);

	while (1) {
		rc = get_command(client);
		if (rc == -1)
			break;
	}

	close(client_sock);
	free(client);

	return 0;
}

void *start_thread(void *params)
{
	thread_client thr = *(thread_client *)params;
	client_function(thr.client_socket);
	return NULL;
}


void logmemcache_init_server_os(int *socket_server)
{
	int sock, client_size, client_sock;
	struct sockaddr_in server, client;
	int pos = 0;

	memset(&server, 0, sizeof(struct sockaddr_in));

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return;
	// printf("Server sock is %d\n", sock);

	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(SERVER_IP);

	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Could not bind");
		exit(1);
	}

	if (listen(sock, DEFAULT_CLIENTS_NO) < 0) {
		perror("Error while listening");
		exit(1);
	}

	*socket_server = sock;
	
	
	while (1) {
		if (!clients)
			clients = malloc(sizeof(thread_client));
		else
			clients = realloc(clients, (pos + 1) * sizeof(thread_client));
		memset(&client, 0, sizeof(struct sockaddr_in));
		client_size = sizeof(struct sockaddr_in);
		client_sock = accept(sock, (struct sockaddr *)&client,
				(socklen_t *)&client_size);

		if (client_sock < 0) {
			perror("Error while accepting clients");
		}

		clients[pos].client_size = client_size;
		clients[pos].client_socket = client_sock;
		clients[pos].client = client;
		if (pthread_create(&clients[pos].client_thread, NULL, start_thread, &clients[pos]) < 0) {
			perror("Error while creating");
			exit(1);
		}
		pos++;

		// client_function(client_sock);
	}
}

int logmemcache_init_client_cache(struct logmemcache_cache *cache)
{
	printf("Init cache %s\n", cache->ptr);
	cache->ptr = NULL;
	cache->pages = 0;
	cache->logs = 0;
	cache->logs_per_page = 0;
	return 0;
}

int logmemcache_add_log_os(struct logmemcache_client_st *client,
	struct client_logline *log)
{
	int pageSize = getpagesize();
	char *buffer;

	printf("Adding for client sock %d\n", client->client_sock);

	printf("Adding log line\n");
	if (!client->cache->ptr) {
		printf("ptr is null\n");
		client->cache->ptr = mmap(NULL, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC, 
			MAP_ANONYMOUS | MAP_SHARED, -1, 0);
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
		client->cache->ptr = mmap(NULL, (client->cache->pages + 1) * pageSize, 
			PROT_READ | PROT_WRITE | PROT_EXEC, 
			MAP_ANONYMOUS | MAP_SHARED, -1, 0);
		if (client->cache->ptr == NULL) {
			fprintf(stderr, "mmap\n");
			exit(EXIT_FAILURE);
		}
		memcpy(client->cache->ptr, buffer, client->cache->pages * pageSize);
		memcpy(client->cache->ptr + client->cache->pages * pageSize, log->logline, LOGLINE_SIZE);
		free(buffer);
		client->cache->pages++;
		client->cache->logs++;
		client->cache->logs_per_page = 1;
		
	} else {
		printf("I have enough space in the memcpy pages %d\n", client->cache->pages);
		
		memcpy(client->cache->ptr + (client->cache->pages - 1) * pageSize 
			+ client->cache->logs_per_page - 1, log->logline, LOGLINE_SIZE);
		client->cache->logs_per_page++;
		client->cache->logs++;
	}
	return 0;
}

int logmemcache_disconnect_client_os(struct logmemcache_client_st *client)
{
	// int pageSize = getpagesize();

	if (close(client->client_sock) < 0) {
		perror("Error while closing");
		return -1;
	}
	
	// printf("Going to munmap\n");
	// if (munmap(client->cache->ptr, pageSize * client->cache->pages) < 0) {
	// 	perror("Error while munmap");
	// 	return -1;
	// }
	// client->cache->ptr = NULL;
	// free(client->cache->)
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
