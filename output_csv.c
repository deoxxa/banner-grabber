#include <stdio.h>
#include <time.h>

#include "output_csv.h"

void output_function_csv_pre()
{
  printf("\"Host\",\"Port\",\"Time\",\"Response\"\n");
}

void output_function_csv_record(unsigned long int host, int port, time_t time, char* data, int len)
{
  printf("\"%lu.%lu.%lu.%lu\",%d,%d,", ((host & 0xFF000000) >> 24), ((host & 0x00FF0000) >> 16), ((host & 0x0000FF00) >> 8), ((host & 0x000000FF) >> 0), port, time);
  printf("\"");
  int i;
  for (i=0;i<len;++i)
  {
    switch (data[i])
    {
    case '\n':
      printf("\\n");
      break;
    case '\r':
      printf("\\r");
      break;
    case '"':
      printf("\\\"");
      break;
    case '\\':
      printf("\\");
      break;
    default:
      printf("%c", data[i]);
    }
  }
  printf("\"\n");
}

void output_function_csv_post()
{
}
