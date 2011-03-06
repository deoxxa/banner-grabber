#ifndef _DYBUF_H
#define _DYBUF_H

struct dybuf
{
  size_t size;
  char* data;
};

struct dybuf* dybuf_new();
void dybuf_free(struct dybuf* node);
void dybuf_expand(struct dybuf* node, size_t len);
void dybuf_append(struct dybuf* node, char* data, size_t len);

#endif
