#include <stdio.h>
#include <time.h>

#include "output_json.h"

void output_function_json_pre()
{
  printf("[\n");
}

void output_function_json_record(unsigned long int host, int port, time_t time, char* data, int len)
{
  printf("  {'host':'%lu.%lu.%lu.%lu','port':%d,'time':%d,'response':'", ((host & 0xFF000000) >> 24), ((host & 0x00FF0000) >> 16), ((host & 0x0000FF00) >> 8), ((host & 0x000000FF) >> 0), port, time);
  int i;
  for (i=0;i<len;++i)
  {
    switch (data[i])
    {
    case '\'':
      printf("\\'");
      break;
    default:
      printf("%c", data[i]);
      break;
    }
  }
  printf("'},\n");
}

void output_function_json_post()
{
  printf("]\n");
}
