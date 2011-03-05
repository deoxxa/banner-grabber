#include <event.h>

struct bg_client
{
  int fd;
  unsigned long int host;
  int port;
  struct bufferevent* ev;
  struct evbuffer* request;
  int request_sent;
  struct evbuffer* response;
  int response_read;
};

struct bg_client* bg_client_new(int fd, unsigned long int host, int port);
void bg_client_free(struct bg_client* client);

void client_read_callback(struct bufferevent* ev, void* arg);
void client_write_callback(struct bufferevent* ev, void* arg);
void client_finished_callback(struct bufferevent* ev, void* arg);
void client_error_callback(struct bufferevent* ev, short err, void* arg);

void stdin_read_callback(struct bufferevent* ev, void* arg);
void stdin_error_callback(struct bufferevent* ev, short err, void* arg);
