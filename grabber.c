#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <event.h>

#include "grabber.h"

struct bg_client* bg_client_new(int fd, unsigned long int host, int port)
{
  struct bg_client* tmp = malloc(sizeof(struct bg_client));
  memset(tmp, 0, sizeof(struct bg_client));

  tmp->fd = fd;
  tmp->host = host;
  tmp->port = port;
  tmp->request = evbuffer_new();
  tmp->request_sent = 0;
  tmp->response = evbuffer_new();
  tmp->response_read = 0;

  evbuffer_add_printf(tmp->request, "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

  return tmp;
}

void bg_client_free(struct bg_client* client)
{
  evbuffer_free(client->request);
  evbuffer_free(client->response);
}

void client_read_callback(struct bufferevent* ev, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Read callback called for client %p\n", client);

  int total = 0;
  int read = 0;
  char tmp[1024];
  do
  {
    memset(tmp, 0, 1024);
    read = bufferevent_read(ev, tmp, 1024);
    evbuffer_add(client->response, tmp, read);
    printf("Read %d bytes from client %p\n", read, client);
    total += read;
  } while (read > 0);

  printf("Read %d bytes total from client %p\n", total, client);

  if (total == 0)
    client_finished_callback(ev, arg);
  if (total == -1)
    client_error_callback(ev, 0, arg);
}

void client_write_callback(struct bufferevent* ev, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Write callback called for client %p\n", client);

  if (client->request_sent == 0)
  {
    evbuffer_write(client->request, client->fd);
    client->request_sent = 1;
  }
  bufferevent_enable(client->ev, EV_READ);
  bufferevent_disable(client->ev, EV_WRITE);
}

void client_finished_callback(struct bufferevent* ev, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Finished callback called for client %p\n", client);

  printf("Data from client %p:\n", client);
  printf("============\n");
  char tmp[1024];
  memset(tmp, 0, 1024);
  while (evbuffer_remove(client->response, tmp, 255))
    printf("%s", tmp);
  printf("============\n");

  bufferevent_free(client->ev);
  close(client->fd);
  bg_client_free(client);
}

void client_error_callback(struct bufferevent* ev, short err, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Error callback called for client %p\n", client);

  if (err & EVBUFFER_READ)    printf("`- Error occured during reading\n");
  if (err & EVBUFFER_WRITE)   printf("`- Error occured during writing\n");
  if (err & EVBUFFER_EOF)     printf("`- Error was EOF\n");
  if (err & EVBUFFER_ERROR)   printf("`- Error was generic\n");
  if (err & EVBUFFER_TIMEOUT) printf("`- Error was timeout related\n");

  client_finished_callback(ev, arg);
}

void stdin_read_callback(struct bufferevent* ev, void* arg)
{
  printf("Read callback called for stdin\n");

  char* line;
  while ((line = evbuffer_readline(ev->input)) != NULL)
  {
    char host_str[16];
    struct in_addr host_in;
    unsigned long int host = 0;
    char port_str[6];
    int port = 0;

    memset(host_str, 0, 16);
    memset(port_str, 0, 6);

    if (strchr(line, ':') != NULL)
    {
      strncpy(host_str, line, strchr(line, ':')-line);
      strncpy(port_str, strchr(line, ':')+1, 5);
      inet_pton(AF_INET, host_str, &host_in);
      host = host_in.s_addr;
      port = htons(atoi(port_str));
    }

    if (host == INADDR_NONE || port < 1 || port > 65535)
      return;

    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
      return;

    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
      return;

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = port;
    saddr.sin_addr.s_addr = host;
    memset(saddr.sin_zero, 0, sizeof(saddr.sin_zero));

    struct bg_client* client = bg_client_new(fd, host, port);

    client->ev = bufferevent_new(client->fd, &client_read_callback, &client_write_callback, &client_error_callback, client);
    bufferevent_enable(client->ev, EV_WRITE);

    connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
  }
}

void stdin_error_callback(struct bufferevent* ev, short err, void* arg)
{
  printf("Error callback called for stdin\n");
}

int main(int argc, char** argv)
{
  struct event_base* ev_base = event_init();

  struct bufferevent* event_stdin = bufferevent_new(fileno(stdin), &stdin_read_callback, NULL, &stdin_error_callback, NULL);
  bufferevent_enable(event_stdin, EV_READ);

  printf("Dispatching events...\n");
  event_base_dispatch(ev_base);
  printf("All events finished apparently\n");

  printf("Freeing event base...\n");
  event_base_free(ev_base);
  printf("Done\n");
}
