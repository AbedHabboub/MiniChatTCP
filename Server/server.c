#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "wrsock.h"
#include "vector.h"

#define DEBUG       1
#define BUF_SIZE    1024
#define SERVER_PORT 4040

#define MAX_AMOUNT_CLIENTS FD_SETSIZE

typedef enum {
	UNDEFINED = 0,
	CONNECT,
	QUIT,
	WHO,
	KILL,
	SHUTDOWN
} ServerCommand;

typedef struct {
	int socket;
	char *name;
	struct sockaddr_in *address;
	int connected;
} Client;

static char* get_command_argument(char *a_data);
static void send_broadcast_msg(void *a_client, void *a_message);
static void collect_all_names(void *item, void *names);
static int send_msg(Client *a_client, char *a_message);

static volatile int _s_runing = -1;
static vector *_s_clients = NULL;

static Client* client_create(void)
{
	Client *c = malloc(sizeof(Client));
	c->name = NULL;
	c->address = malloc(sizeof(struct sockaddr_in));
	c->socket = -1;
	c->connected = 0;
	return c;
}

static void client_destroy(Client *client)
{
	if (client)
	{
		free(client->name);
		free(client->address);
		free(client);
	}
}

int cmp_clients_by_name(void *item, void *pattern)
{
	Client *cl_item = item;
	char *pat_name = pattern;
	if (cl_item->name == NULL && pat_name == NULL)
	{
		return 0;
	}
	else if (cl_item->name && pat_name && strcmp(cl_item->name, pat_name) == 0)
	{
		return 0;
	}
	return 1;
}

int cmp_clients_by_addr(void *item, void *pattern)
{
	Client *cl_item = item;
	struct sockaddr_in *pat_addr = pattern;
	if (cl_item->address->sin_port == pat_addr->sin_port && cl_item->address->sin_addr.s_addr == pat_addr->sin_addr.s_addr)
	{
		return 0;
	}
	return 1;
}

int cmp_clients_by_ptr(void *item, void *pattern)
{
	return !(item == pattern);
}

static ServerCommand parse_user_command(char *str)
{
	ServerCommand ret = UNDEFINED;
	if (strncmp(str, "_kill", 5) == 0)
	{
		ret = KILL;
	}
	else if (strncmp(str, "_shutdown", 9) == 0)
	{
		ret = SHUTDOWN;
	}
	else if (strncmp(str, "_who", 4) == 0)
	{
		ret = WHO;
	}
	return ret;
}

void TraitementClavier(void)
{
	char buf[BUF_SIZE] = {0};
	char message[BUF_SIZE] = {0};
	char *name;
	Client *client;
	int count_recv = read(0, buf, BUF_SIZE);    
	buf[count_recv - 1] = '\0';

	switch(parse_user_command(buf))
	{
		case KILL:
			name = get_command_argument(buf);
			if (!name)
			{
				printf("Name of client should be provided.\n");
				break;
			}
			client = vector_delete_first_equal(_s_clients, name, cmp_clients_by_name);
			printf("Server has '%d' clients\n", vector_total(_s_clients));
			if (client)
			{
				sprintf(message, "The client '%s' has quit.", client->name);
				vector_foreach(_s_clients, message, send_broadcast_msg);
				printf("%s\n", message);
				/* send info data to client */
				sprintf(message, "server: You were killed by the server.");
				send_msg(client, message);
				close(client->socket);
				client_destroy(client);
			}
			else
			{
				printf("Client '%s' was not found\n", name);
			}
		break;
		case SHUTDOWN:
			sprintf(message, "The server is going to shutdown.");
			vector_foreach(_s_clients, message, send_broadcast_msg);
			_s_runing = 0;
			printf("%s\n", message);
		break;
		case WHO:
			vector_foreach(_s_clients, message, collect_all_names);
			printf("All clients: '%s'\n", message);
		break;
		default:
			printf("Unknown command. Allowed commands: _kill <name>, _shutdown, _who\n");
		break;
	}
	fflush(stdout);
}


static ServerCommand parse_client_command(char *str)
{
	ServerCommand ret = UNDEFINED;
	if (strncmp(str, "_connect", 8) == 0)
	{
		ret = CONNECT;
	}
	else if (strncmp(str, "_quit", 5) == 0)
	{
		ret = QUIT;
	}
	else if (strncmp(str, "_who", 4) == 0)
	{
		ret = WHO;
	}
	return ret;
}

static int send_msg(Client *a_client, char *a_message)
{
	int length = strlen(a_message);
	int res;
	res = write(a_client->socket, &length, sizeof(length));
	if (res == -1)
	{
		printf("Could not write to client '%s'\n", a_client->name);
		return 1;
	}
	res = write(a_client->socket, a_message, length);
	if (res == -1)
	{
		printf("Could not write to client '%s'\n", a_client->name);
		return 1;
	}
	return 0;
}

static void send_broadcast_msg(void *a_client, void *a_message)
{
	Client *client = a_client;
	char *message = a_message;
	if (client->connected)
	{
		send_msg(client, message);
	}
}

static void collect_all_names(void *item, void *names)
{
	Client *client = item;
	char *all = names;
	sprintf(all, "%s %s", all, client->name);
}

static char* get_command_argument(char *a_data)
{
	char *arg;
	char *ret = NULL;
	size_t len;
	strtok (a_data, " ");
	arg = strtok (NULL, " ");
	if (arg && (len = strlen(arg)) > 0)
	{
		ret = malloc(len + 1);
		strncpy(ret, arg, len);
		ret[len] = '\0';
	}
	return ret;
}

void verify_name(char **a_name)
{
	char message[BUF_SIZE];
	char *new_name = NULL;
	size_t length;
	int was_used = 0;
	sprintf(message, "Client with name '%s' is connecting.\n", *a_name);

	while (vector_is_contains(_s_clients, *a_name, cmp_clients_by_name) == 1)
	{
		was_used = 1;
		length = sizeof(*a_name);
		new_name = malloc(length + 1);
		sprintf(new_name, "%s_", *a_name);
		free(*a_name);
		*a_name = new_name;
	}
	if (was_used)
	{
		printf("%s", message);
		printf("Client already exists. Set new name to '%s'\n", new_name);
	}
}

void handle_client_command(ServerCommand a_cmd, char *a_data, Client *a_client)
{
	char *name = NULL;
	char message[BUF_SIZE] = {0};
	char buf[BUF_SIZE] = {0};
	switch (a_cmd)
	{
		case CONNECT:
			name = get_command_argument(a_data);
			verify_name(&name);
			a_client->name = name;
			sprintf(message, "New client '%s' is connected.", a_client->name);
			vector_foreach(_s_clients, message, send_broadcast_msg);
			a_client->connected = 1;
			printf("%s\n", message);
		break;
		case QUIT:
			vector_delete_first_equal(_s_clients, a_client, cmp_clients_by_ptr);
			sprintf(message, "The client '%s' has quit.", a_client->name);
			vector_foreach(_s_clients, message, send_broadcast_msg);
			printf("%s\n", message);
			close(a_client->socket);
			client_destroy(a_client);
		break;
		case WHO:
			vector_foreach(_s_clients, message, collect_all_names);
			sprintf(buf, "All clients: %s", message);
			printf("Send info about all clients to '%s'\n", a_client->name);
			send_msg(a_client, buf);
		break;
		case UNDEFINED:
			/* send received message to all clients */
			printf("Send '%s' to all clients from '%s'\n", a_data, a_client->name);
			sprintf(message, "%s: %s", a_client->name, a_data);
			vector_foreach(_s_clients, message, send_broadcast_msg);
		break;
		default:
			assert("Unknown command " == (char*)a_cmd);
		break;
    }
}

int TraitementSock(Client *a_client)
{
#define check_connection(res)				\
if (res==-1) {						\
printf("Could not read data. Error '%d'\n", errno);	\
return 1;						\
} else if (res==0) {					\
printf("Read 0 file. EOF\n");				\
return 1;						\
}

	char buf[BUF_SIZE] = {0};
	int size_message;
	int res, ret = 0;
	char *straddr;
	ServerCommand client_cmd;

	res = read(a_client->socket, &size_message, sizeof(size_message));
	check_connection(res);

	res = read(a_client->socket, buf, size_message);
	check_connection(res);

	straddr = inet_ntoa(a_client->address->sin_addr);

	printf("From address '%s:%d' receiving message '%s'\n", straddr, a_client->address->sin_port, buf);

	client_cmd = parse_client_command(buf);

	if (a_client->connected == 0 && client_cmd != CONNECT)
	{
		printf("Client with address '%s:%d' is not connected. Message - '%s' - is not allowed.\n", straddr, a_client->address->sin_port, buf);
	}
	else if (a_client->connected == 1 && client_cmd == CONNECT) 
	{
        	printf("Client '%s' with address '%s:%d' already connected.\n"
                            , a_client->name, straddr
                            , a_client->address->sin_port);
	}
	else
	{
		handle_client_command(client_cmd, buf, a_client);
	}
	fflush(stdout);
	return ret;
}

int main(int argc, char** argv)
{
	Client *client = NULL;
	socklen_t addr_len = -1;
	fd_set readf;
	int maxfd;
	int server_socket;
	int i;
	int ready;
	int amount;
	int server_port;
	char message[BUF_SIZE] = {0};

	if (argc < 2 || (server_port = atoi (argv[1])) == 0)
	{
		server_port = SERVER_PORT;
	}
	server_socket = SockTcp(NULL, server_port);
	if (server_socket<0)
	{
		printf("Cannot create TCP socket.\n");
		exit(1);
	}
	if (listen(server_socket, MAX_AMOUNT_CLIENTS) != 0)
	{
		printf("Could not listen server socket '%d'. Error is '%d'\n", server_socket, errno);
		exit(1);
	}
	_s_clients = vector_create();

	printf("MiniChat (Server) started using port %d\n", server_port);
	printf("Allowed commands:"
		"\n\t1) _who\t\t: to display a list of connected clients"
		"\n\t2) _kill <name>\t: to disconnect client <name>"
		"\n\t3) _shutdown\t: to shutdown the server\n");

	_s_runing = 1;

	while(_s_runing)
	{
		FD_ZERO(&readf);
		maxfd = server_socket;
		for (i = 0; i < vector_total(_s_clients); ++i)
		{
			client = vector_get(_s_clients, i);
			FD_SET(client->socket, &readf);
			if (client->socket > maxfd)
			{
				maxfd = client->socket;
			}
		}
		FD_SET(server_socket, &readf);
		FD_SET(0, &readf);
		ready = select(maxfd + 1, &readf, 0, 0, 0);
		if (FD_ISSET(0, &readf))
		{
			TraitementClavier();
			if (--ready <= 0)
			{
				/* no more descriptors for handling*/
				continue;
			}
		}
		if (FD_ISSET(server_socket, &readf))
		{
			/* new client connection */
			client = client_create();
			addr_len = sizeof(struct sockaddr_in);
			client->socket = accept(server_socket, (struct sockaddr *)client->address, &addr_len);
			vector_add(_s_clients, client);
			/* add new descriptor to set */
			FD_SET(client->socket, &readf);
			/* set maxfd for select */
			if (--ready <= 0)
			{
				/* no more descriptors for handling*/
				continue;
			}
		}
		amount = vector_total(_s_clients);
		for (i = 0; i <= amount; ++i)
		{
			/* check all clients for data */
			client = vector_get(_s_clients, i);
			if (client == NULL)
			{
				continue;
			}
			if (FD_ISSET(client->socket, &readf))
			{
				if (TraitementSock(client))
				{
					/* connection closed by client */
					vector_delete_first_equal(_s_clients, client, cmp_clients_by_ptr);
					sprintf(message, "Something happened. The client '%s' has quit.", client->name);
					vector_foreach(_s_clients, message, send_broadcast_msg);
					close(client->socket);
					printf("%s\n", message);
					client_destroy(client);
				}
				if (--ready <= 0)
				{
					/* no more descriptors for handling */
					break;
				}
			}
		}
	}
	printf("Closing all sockets and removing all clients...\n");
	close(server_socket);
	amount = vector_total(_s_clients);
	for (i = 0; i <= amount; ++i)
	{
		if ((client = vector_get(_s_clients, i)) != NULL)
		{
			close(client->socket);
			client_destroy(client);
		}
	}
	vector_free(_s_clients);
	printf("Done.\n");
}
