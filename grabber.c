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
#include <time.h>

#include "grabber.h"
#include "dybuf.h"

#include "output_csv.h"
#include "output_json.h"
#include "output_xml.h"

struct bg_client* bg_client_new(int fd, unsigned long int host, int port)
{
  requests_current++;

  struct bg_client* client = malloc(sizeof(struct bg_client));
  memset(client, 0, sizeof(struct bg_client));

  client->fd = fd;
  client->host = host;
  client->port = port;
  client->request = dybuf_new();
  client->request_sent = 0;
  client->response = dybuf_new();
  client->response_read = 0;

  char ip_str[16];
  sprintf(ip_str, "%lu.%lu.%lu.%lu", ((client->host & 0xFF000000) >> 24), ((client->host & 0x00FF0000) >> 16), ((client->host & 0x0000FF00) >> 8), ((client->host & 0x000000FF) >> 0));
  char port_str[6];
  sprintf(port_str, "%d", client->port);

  if (request_template == NULL)
    return client;

  int i = 0;
  int offset = 0;
  for (;i<strlen(request_template);++i)
  {
    if (request_template[i] == '%' && i < strlen(request_template))
    {
      dybuf_append(client->request, request_template+offset, i-offset);

      ++i;
      switch (request_template[i])
      {
      case 'h':
        dybuf_append(client->request, ip_str, strlen(ip_str));
        break;
      case 'p':
        dybuf_append(client->request, port_str, strlen(port_str));
        break;
      default:
        dybuf_append(client->request, "%", 1);
        --i;
        break;
      }

      offset = i+1;
    }

    if (request_template[i] == '\\' && i < strlen(request_template))
    {
      dybuf_append(client->request, request_template+offset, i-offset);

      ++i;
      switch (request_template[i])
      {
      case 'r':
        dybuf_append(client->request, "\r", 1);
        break;
      case 'n':
        dybuf_append(client->request, "\n", 1);
        break;
      default:
        dybuf_append(client->request, "\\", 1);
        --i;
        break;
      }

      offset = i+1;
    }
  }

  dybuf_append(client->request, request_template+offset, strlen(request_template)-offset);

  return client;
}

void bg_client_free(struct bg_client* client)
{
  requests_current--;

  event_del(client->ev);
  event_free(client->ev);
  close(client->fd);
  dybuf_free(client->request);
  dybuf_free(client->response);
  free(client);

  if (requests_current < 10)
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

  char tmp[2048];
  int read = recv(client->fd, tmp, 1024, 0);
  if (read > 0)
    dybuf_append(client->response, tmp, read);
  else if (read == 0)
    client_finished_callback(fd, arg);
  else
    client_error_callback(fd, arg);
}

void client_write_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;

  if (client->request_sent == 0)
  {
    if (client->request->used >= 1)
      send(client->fd, client->request->data, client->request->used, 0);

    client->request_sent = 1;
  }

  event_assign(client->ev, ev_base, client->fd, EV_READ|EV_PERSIST, &client_callback, client);
  event_add(client->ev, NULL);
}

void client_finished_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;
  (*output_function_record)(client->host, client->port, time(NULL), client->response->data, client->response->used);
  bg_client_free(client);
}

void client_error_callback(evutil_socket_t fd, void* arg)
{
  struct bg_client* client = (struct bg_client*)arg;
  client_finished_callback(fd, arg);
}

void stdin_callback(evutil_socket_t fd, short ev, void* arg)
{
  if (ev & EV_READ)
    stdin_read_callback(fd, arg);
}

void stdin_read_callback(evutil_socket_t fd, void* arg)
{
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

  if (requests_current < 10)
    event_add(event_stdin, NULL);
}

void stdin_error_callback(evutil_socket_t fd, void* arg)
{
}

int main(int argc, char** argv)
{
  requests_max = 50;
  requests_current = 0;
  request_timeout = 10;
  request_template = NULL;
  output_function_pre = &output_function_csv_pre;
  output_function_record = &output_function_csv_record;
  output_function_post = &output_function_csv_post;
  ev_base = NULL;
  event_stdin = NULL;

  int c;
  while ((c = getopt(argc, argv, "r:n:t:f:h")) != -1)
  {
    switch (c)
    {
    case 'h':
      fprintf(stderr, "Usage: %s [-h] [-r request template] [-n concurrent requests] [-t timeout] [-f format (csv|json|xml)]\n", argv[0]);
      exit(0);
    case 'r':
      request_template = optarg;
      break;
    case 'n':
      requests_max = atoi(optarg);
      break;
    case 't':
      request_timeout = atoi(optarg);
      break;
    case 'f':
      if (strcmp(optarg, "csv") == 0)
      {
        output_function_pre = &output_function_csv_pre;
        output_function_record = &output_function_csv_record;
        output_function_post = &output_function_csv_post;
      }
      if (strcmp(optarg, "json") == 0)
      {
        output_function_pre = &output_function_json_pre;
        output_function_record = &output_function_json_record;
        output_function_post = &output_function_json_post;
      }
      if (strcmp(optarg, "xml") == 0)
      {
        output_function_pre = &output_function_xml_pre;
        output_function_record = &output_function_xml_record;
        output_function_post = &output_function_xml_post;
      }
      break;
    }
  }

  ev_base = event_base_new();

  event_stdin = event_new(ev_base, fileno(stdin), EV_READ, &stdin_callback, NULL);
  event_add(event_stdin, NULL);

  (*output_function_pre)();

  event_base_dispatch(ev_base);
  event_base_free(ev_base);

  (*output_function_post)();
}
