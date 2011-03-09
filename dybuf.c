#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dybuf.h"

struct dybuf* dybuf_new()
{
#ifdef _DEBUG
  fprintf(stderr, "dybuf_new()\n");
#endif

  struct dybuf* node = malloc(sizeof(struct dybuf));
  node->size = 0;
  node->used = 0;
  node->data = NULL;
  return node;
}

void dybuf_free(struct dybuf* node)
{
#ifdef _DEBUG
  fprintf(stderr, "dybuf_new(%p)\n", node);
#endif

  free(node->data);
  free(node);
}

void dybuf_expand(struct dybuf* node, size_t len)
{
#ifdef _DEBUG
  fprintf(stderr, "dybuf_new(%p, %d)\n", node, len);
#endif

  size_t old_size = node->size;
  char* old_data = node->data;
  node->size += len;
  node->data = realloc(node->data, node->size);
}

void dybuf_append(struct dybuf* node, char* data, size_t len)
{
#ifdef _DEBUG
  fprintf(stderr, "dybuf_new(%p, %p, %d)\n", node, data, len);
#endif

  while ((node->size - node->used) < len)
    dybuf_expand(node, 1024);
  memcpy(node->data+node->used, data, len);
  node->used += len;
}
