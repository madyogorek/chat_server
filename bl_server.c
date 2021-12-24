#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <poll.h>
#include <limits.h>             // added for NAME_MAX
#include "blather.h"

//global variable controlling main's while loop
int kill_self = 0;


void handler(int signum)
{
  if((signum == SIGINT) || (signum == SIGTERM))
  {
    //breaking while loop
    kill_self = 1;

  }
}

int main(int arc, char *argv[])
{
  server_t servy;
  memset(&servy, 0,sizeof(server_t));
  server_start(&servy, argv[1], O_RDWR );
  struct sigaction sa;
  memset(&sa, 0,sizeof(sa));
  sa.sa_handler = handler;


  //handler is at the top of the file
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);



  while(1)
  {

    server_check_sources(&servy);
    server_handle_join(&servy);
    for(int x = 0; x < servy.n_clients; x++)
    {
      server_handle_client(&servy, x);
    }
    if(kill_self == 1)
    {

      break;
    }
  }
  //shutdown chores
  server_shutdown(&servy);
  return 0;
}
