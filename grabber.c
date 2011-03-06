#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <event2/event.h>

#include "grabber.h"
#include "dybuf.h"

struct bg_client* bg_client_new(int fd, unsigned long int host, int port)
{
  current_clients++;

  struct bg_client* client = malloc(sizeof(struct bg_client));
  memset(client, 0, sizeof(struct bg_client));

  client->fd = fd;
  client->host = host;
  client->port = port;
  client->request = dybuf_new();
  client->request_sent = 0;
  client->response = dybuf_new();
  client->response_read = 0;

  char request_a[] = "GET / HTTP/1.1\r\nHost: ";
  char request_b[] = "\r\nConnection: close\r\n\r\n";
  char host_str[16];

  sprintf(host_str, "%lu.%lu.%lu.%lu", ((client->host & 0xFF000000) >> 24), ((client->host & 0x00FF0000) >> 16), ((client->host & 0x0000FF00) >> 8), ((client->host & 0x000000FF) >> 0));

  dybuf_append(client->request, request_a, strlen(request_a));
  dybuf_append(client->request, host_str, strlen(host_str));
  dybuf_append(client->request, request_b, strlen(request_b));

  return client;
}

void bg_client_free(struct bg_client* client)
{
  current_clients--;

  event_del(client->ev);
  event_free(client->ev);
  close(client->fd);
  dybuf_free(client->request);
  dybuf_free(client->response);
  free(client);

  if (current_clients < 250)
    event_add(event_stdin, NULL);
}

void client_callback(evutil_socket_t fd, short ev, void* arg)
{
  if (ev & EV_READ)
    client_read_callback(fd, arg);
  if (ev & EV_WRITE)
    client_write_callback(fd, arg);
}

void client_read_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Read callback called for client %p\n", client);

  char tmp[2048];
  int read = recv(client->fd, tmp, 1024, 0);
  if (read > 0)
    dybuf_append(client->response, tmp, read);
  printf("Read %d bytes from client %p\n", read, client);

  if (read == 0)
    client_finished_callback(fd, arg);
  if (read == -1)
    client_error_callback(fd, arg);
}

void client_write_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Write callback called for client %p\n", client);

  if (client->request_sent == 0)
  {
    send(client->fd, client->request->data, client->request->size, 0);
    client->request_sent = 1;
  }

  event_assign(client->ev, ev_base, client->fd, EV_READ|EV_PERSIST, &client_callback, client);
  event_add(client->ev, NULL);
}

void client_finished_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Finished callback called for client %p\n", client);

  printf("Data from client %p (%lu.%lu.%lu.%lu:%d):\n", client, ((client->host & 0xFF000000) >> 24), ((client->host & 0x00FF0000) >> 16), ((client->host & 0x0000FF00) >> 8), ((client->host & 0x000000FF) >> 0), client->port);
  printf("============\n");
  int i;
  for (i=0;i<client->response->size;++i)
    printf("%c", *(client->response->data+i));
  printf("============\n");

  bg_client_free(client);
}

void client_error_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  printf("Error callback called for client %p\n", client);

  client_finished_callback(fd, arg);
}

void stdin_callback(evutil_socket_t fd, short ev, void* arg)
{
  if (ev & EV_READ)
    stdin_read_callback(fd, arg);
}

void stdin_read_callback(evutil_socket_t fd, void* arg)
{
  printf("Read callback called for stdin\n");

  char line[1024];
  if (fgets(line, 1024, stdin) == NULL)
  {
    event_del(event_stdin);
    return;
  }

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
    host = ntohl(host_in.s_addr);
    port = atoi(port_str);
  }

  if (host == INADDR_NONE || port < 1 || port > 65535)
    return;

  printf("Host: %s\nPort: %d\n", host_str, port);

  int s;
  if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
  return;

  int flags = fcntl(s, F_GETFL);
  flags |= O_NONBLOCK;
  if (fcntl(s, F_SETFL, flags) == -1)
    return;

  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  saddr.sin_addr.s_addr = htonl(host);
  memset(saddr.sin_zero, 0, sizeof(saddr.sin_zero));

  struct bg_client* client = bg_client_new(s, host, port);

  client->ev = event_new(ev_base, client->fd, EV_READ|EV_WRITE, &client_callback, client);
  event_add(client->ev, NULL);

  connect(s, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));

  if (current_clients < 250)
    event_add(event_stdin, NULL);
}

void stdin_error_callback(evutil_socket_t fd, void* arg)
{
  printf("Error callback called for stdin\n");
}

int main(int argc, char** argv)
{
  ev_base = event_base_new();

  event_stdin = event_new(ev_base, fileno(stdin), EV_READ, &stdin_callback, NULL);
  event_add(event_stdin, NULL);

  current_clients = 0;

  printf("Dispatching events...\n");
  event_base_dispatch(ev_base);
  printf("All events finished apparently\n");

  printf("Freeing event base...\n");
  event_base_free(ev_base);
  printf("Done\n");
}
