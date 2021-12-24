
#include "blather.h"
#include <string.h>
#include <errno.h>

simpio_t simpio_actual;
simpio_t *simpio = &simpio_actual;

client_t client_actual;
client_t *client = &client_actual;

pthread_t user_thread;          // thread managing user input
pthread_t server_thread;

typedef struct
{
  char j_name[MAXNAME];
  int fdclient;
  int fdserver;
} args;

// Worker thread to manage user input
void *user_worker(void *arg){

  args *argsa = (args*) arg;


  while(!simpio->end_of_input){
    simpio_reset(simpio);
    iprintf(simpio, "");                                          // print prompt
    while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
      simpio_get_char(simpio);
    }
    if(simpio->line_ready){
      mesg_t mess;
      memset(&mess, 0,sizeof(mesg_t));
      mess.kind = BL_MESG;
      snprintf(mess.name, MAXNAME,  "%s", argsa->j_name);

      snprintf(mess.body, MAXLINE,  "%s", simpio->buf);

      //sending data to server
      write(argsa->fdserver, &mess, sizeof(mesg_t));
    }
  }

  mesg_t messy;
  memset(&messy, 0,sizeof(mesg_t));
  messy.kind = BL_DEPARTED;
  snprintf(messy.name, MAXNAME,  "%s", argsa->j_name);
  //sending data to server
  write(argsa->fdserver, &messy, sizeof(mesg_t));
  pthread_cancel(server_thread); // kill the background thread
  return NULL;
}

// worker thread to listen to the info from the server.
void *server_worker(void *arg)
{

  args *argsa = (args*) arg;



  while(1)
  {
    mesg_t mess;

    //listening to data from toclient fifo
    read(argsa->fdclient, &mess, sizeof(mesg_t));
    if(mess.kind == BL_MESG)
    {
      iprintf(simpio, "[%s] : %s\n", mess.name, mess.body);
    }
    else if(mess.kind == BL_JOINED)
    {
      iprintf(simpio, "-- %s JOINED --\n", mess.name);
    }
    else if(mess.kind == BL_DEPARTED)
    {
      iprintf(simpio, "-- %s DEPARTED --\n", mess.name);
    }
    else if(mess.kind == BL_SHUTDOWN)
    {
      iprintf(simpio, "!!! server is shutting down !!!\n");
      break;
    }

  }
  pthread_cancel(user_thread); // kill the user thread after receiving shutdown message
  return NULL;
}



int main(int argc, char *argv[])
{


  join_t joiner;
  memset(&joiner, 0,sizeof(join_t));

  snprintf(joiner.name, MAXNAME, "%s", argv[2]);



  sprintf(joiner.to_server_fname, "%d.server.fifo", getpid());    //concatenating

  remove(joiner.to_server_fname); //remove old fifo, ensures we start in good state
  int ret = mkfifo(joiner.to_server_fname, S_IRUSR | S_IWUSR);  //creating the fifo


  if((ret == -1) && (errno != EEXIST))
  {
    perror("error creating pipe");
  }
  sprintf(joiner.to_client_fname, "%d.client.fifo", getpid());    //concatenating


  remove(joiner.to_client_fname); //remove old fifo, ensures we start in good state
  int reto = mkfifo(joiner.to_client_fname, S_IRUSR | S_IWUSR);  //creating the fifo

  if((reto == -1) && (errno != EEXIST))
  {
    perror("error creating pipe");
  }

  args arg_struct;

  snprintf(arg_struct.j_name, MAXNAME, "%s", joiner.name);


  char server_j_fifo[MAXPATH];
  snprintf(server_j_fifo, MAXPATH,  "%s.fifo", argv[1]);
  int j_fd = open(server_j_fifo, O_RDWR);

  write(j_fd, &joiner, sizeof(join_t));

  char prompt[MAXNAME];
  snprintf(prompt, MAXNAME, "%s>> ",joiner.name); // create a prompt string

  simpio_set_prompt(simpio, prompt);         // set the prompt
  simpio_reset(simpio);                      // initialize io
  simpio_noncanonical_terminal_mode();       // set the terminal into a compatible mode

  int fdserver = open(joiner.to_server_fname, O_RDWR);

  int fdclient = open( joiner.to_client_fname, O_RDWR);
  arg_struct.fdserver = fdserver;
  arg_struct.fdclient = fdclient;
  pthread_create(&user_thread,   NULL, user_worker,   &arg_struct);     // start user thread to read input
  pthread_create(&server_thread, NULL, server_worker, &arg_struct);
  pthread_join(user_thread, NULL);
  pthread_join(server_thread, NULL);

  simpio_reset_terminal_mode();   //reseting terminal
  printf("\n");                 // newline just to make returning to the terminal prettier
  return 0;

}
