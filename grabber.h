#ifndef _GRABBER_H
#define _GRABBER_H

#include <event2/event.h>

#include "dybuf.h"

struct bg_client
{
  int fd;
  unsigned long int host;
  int port;
  time_t time;
  struct event* ev;
  struct dybuf* request;
  int request_sent;
  struct dybuf* response;
  int response_read;
};

struct bg_client* bg_client_new(int fd, unsigned long int host, int port);
void bg_client_free(struct bg_client* client);

void client_callback(evutil_socket_t fd, short ev, void* arg);
void client_read_callback(evutil_socket_t fd, void* arg);
void client_write_callback(evutil_socket_t fd, void* arg);
void client_finished_callback(evutil_socket_t fd, void* arg);
void client_error_callback(evutil_socket_t fd, void* arg);

void stdin_callback(evutil_socket_t fd, short ev, void* arg);
void stdin_read_callback(evutil_socket_t fd, void* arg);
void stdin_error_callback(evutil_socket_t fd, void* arg);

struct event_base* ev_base;
struct event* event_stdin;
int requests_current;
int requests_max;
struct timeval* request_timeout;
char* request_template;

void (*output_function_pre)();
void (*output_function_record)(unsigned long int host, int port, time_t time, char* data, int len);
void (*output_function_post)();

#endif
