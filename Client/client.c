#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "wrsock.h"

#define BUF_SIZE 512

typedef enum {
	UNDEFINED = 0,
	CONNECT,
	QUIT,
	WHO
} ClientCommand;

struct conn_data {
	char *name;
	char *host;
	int port;
};

static int volatile _s_connected = 0;
static struct sockaddr_in *_s_server_addr;
static int _s_client_sock = 0;
int len = sizeof (struct sockaddr_in);


static ClientCommand parse_user_command(char *str)
{
	ClientCommand ret = UNDEFINED;
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

static struct conn_data* get_conn_arguments(char *a_data)
{
	char *arg;
	struct conn_data *conn = malloc(sizeof(struct conn_data));
	conn->host = NULL;
	conn->name = NULL;


#define get_string(value)                           \
    if ((arg = strtok (NULL, " ")) != NULL) {       \
        size_t len = strlen(arg);                   \
        char *param = malloc(len + 1);              \
        strncpy(param, arg, len);                   \
        param[len] = '\0';                          \
        value = param;                         \
    } else {                                        \
        goto error;                                 \
    }

	strtok(a_data, " ");
	get_string(conn->name);
	get_string(conn->host);

	if ((arg = strtok (NULL, " ")) != NULL)
	{
		conn->port = atoi(arg);
	}
	else
	{
		goto error;
	}
	return conn;

	error:
	free(conn->host);
	free(conn->name);
	free(conn);
	conn = NULL;
	return conn;
}

int send_message(char *message)
{
	int length = strlen(message);
	int res;
	res = write(_s_client_sock, &length, sizeof (length));
	if (res == -1)
	{
		printf("Could not write to server\n");
		return 1;
	}
	res = write(_s_client_sock, message, length);
	if (res == -1)
	{
		printf("Could not write to server\n");
		return 1;
	}
	return 0;
}

static ClientCommand receive_user_data (struct conn_data **data, char *msg)
{
	char buf[BUF_SIZE] = {0};
	int taillemessage;
	ClientCommand cmd;
	taillemessage = read(0, buf, BUF_SIZE);
	cmd = parse_user_command(buf);
	*data = NULL;
	if (cmd == CONNECT)
	{
		*data = get_conn_arguments(buf);
	}
	else if (cmd == UNDEFINED)
	{
		sprintf(msg, "%s", buf);
	}
	return cmd;
}

int TraitementSock (int sock)
{
#define check_connection(res)                                   \
    if (res == -1) {                                            \
        printf("Could not read data. Error was '%d'\n", errno); \
        return 1;                                               \
    } else if (res == 0) {                                      \
        /*printf("End of file.\n"); */                          \
        return 1;                                               \
    }

	char buf[BUF_SIZE] = {0};
	int length;
	int res;

	res = read(sock, &length, sizeof (length));
	check_connection(res);
	res = read(sock, buf, length);
	check_connection(res);
	printf("%s\n", buf);
	return 0;
}


int send_conn_data(char *name)
{
	char message[BUF_SIZE] = {0};
	sprintf(message, "_connect %s", name);
	return send_message(message);
}

void printline()
{
	printf("\n----------------------\n");
}

int main (int argc, char **argv)
{
#define close_client()      \
    printf("Connection will close.\n"); \
    close(_s_client_sock);  \
    free(_s_server_addr);   \
    _s_server_addr = NULL;  \
    _s_client_sock = 0;     \
    _s_connected = 0

	char message [BUF_SIZE];
	fd_set readf;
	struct conn_data *data = NULL;

	printline();
	printf("MiniChat (Client) started.\nAllowed commands:"
		"\n\t1) _connect name address port\t: to connect to the chat server by declaring the client name, server address, and port"
		"\n\t2) _who\t\t\t\t: to display a list of connected clients"
		"\n\t3) _quit\t\t\t: to disconnect the client\n"
		"Other words will be treated as messages to all other connected clients");
	printline();

	FD_ZERO(&readf);
	while(1)
	{
		FD_SET (0, &readf);
		if (_s_connected)
		{
			FD_SET(_s_client_sock, &readf);
		}
		switch (select (_s_client_sock + 1, &readf, 0, 0, 0))
		{
			default:
			if (FD_ISSET (0, &readf))
			{
				switch(receive_user_data(&data, message))
				{
					case CONNECT:
						if (_s_connected)
						{
							printline();
							printf("You are already connected to MiniChat (Server)");
							printline();
						}
						else if (!data)
						{
							printline();
							printf("Invalid parameters for _connect. Try _connect <name> <address> <port>\n");
							printline();
						}
						else
						{
							_s_client_sock = SockTcp(NULL, 0);
							_s_server_addr = CreerSockAddr(data->host, data->port);
							if (!connect_tcp_socket(_s_client_sock, _s_server_addr) && !send_conn_data(data->name))
							{
								_s_connected = 1;
								printline();
								printf("You are connected to '%s:%d'", data->host, data->port);
								printline();
								fflush(stdout);
							}
							else
							{
								printline();
								printf("You are not connected");
								printline();
							}
						}
						if (data)
						{
							free(data->host);
							free(data->name);
							free(data);
						}
						data = NULL;
					break;
					case QUIT:
						if (!_s_connected) 
						{
							printline();
							printf("You are already disconnected");
							printline();
							break;
						}
						send_message("_quit");
						close_client();
					break;
					case WHO:
						if (!_s_connected)
						{
							printline();
							printf("You are disconnected. Please connect again");
							printline();
							break;
						}
						printline();
						printf("You are already disconnected");
						printline();
						if (send_message("_who"))
						{
							close_client();
						}
					break;
					case UNDEFINED:
						if (!_s_connected)
						{
							printline();
							printf("You are disconnected. Please connect again");
							printline();
							break;
						}
						/* just simple message. need to send to server */
						if (strlen(message) > 1)
						{
							message[strlen(message) - 1] = '\0';
							if (send_message(message))
							{
								close_client();
							}
						}
					break;
					default:
						printline();
						printf("Unknown command.\nAllowed commands:"
							"\n\t1) _connect name address port\t: to connect to the chat server by declaring the client name, server address, and port"
							"\n\t2) _who\t\t\t\t: to display a list of connected clients"
							"\n\t3) _quit\t\t\t: to disconnect the client\n"
							"Other words will be treated as messages to all other connected clients");
						printline();
					break;
				}
			}
			else if (FD_ISSET (_s_client_sock, &readf))
			{
				if (TraitementSock(_s_client_sock))
				{
					close_client();
				}
			}
		}
	}
}
