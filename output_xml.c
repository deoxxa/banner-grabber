#include <stdio.h>
#include <time.h>

#include "output_xml.h"

void output_function_xml_pre()
{
  printf("<!DOCTYPE xml>\n");
  printf("<responses>\n");
}

void output_function_xml_record(unsigned long int host, int port, time_t time, char* data, int len)
{
  printf("  <response host=\"%lu.%lu.%lu.%lu\" port=\"%d\" time=\"%d\">\n", ((host & 0xFF000000) >> 24), ((host & 0x00FF0000) >> 16), ((host & 0x0000FF00) >> 8), ((host & 0x000000FF) >> 0), port, time);
  int i;
  for (i=0;i<len;++i)
  {
    switch (data[i])
    {
    case '"':
      printf("&quot;");
      break;
    case '<':
      printf("&gt;");
      break;
    case '>':
      printf("&lt;");
      break;
    case '&':
      printf("&amp;");
      break;
    case '%':
      printf("&#37;");
      break;
    default:
      printf("%c", data[i]);
      break;
    }
  }
  printf("  </response>\n");
}

void output_function_xml_post()
{
  printf("</responses>\n");
}
