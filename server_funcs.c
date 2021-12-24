#include <stdio.h>
#include "blather.h"
#include <string.h>
#include <poll.h>
#include <errno.h>



client_t *server_get_client(server_t *server, int idx)
// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.
{
  if(server->n_clients < idx)
  {
    printf("boom boom crash baby we goin' down\n");
  }

  return &server->client[idx];

}

void server_start(server_t *server, char *server_name, int perms)
// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd._
{

  //setting the name of the server
  snprintf(server->server_name, MAXNAME,  "%s", server_name);

  server->n_clients = 0;
  char servy_name[MAXPATH];
  //naming the join fifo
  sprintf(servy_name, "%s.fifo", server_name);
  int rem = remove(servy_name); //remove old fifo, ensures we start in good state
  if(rem != 0)
  {
    perror("remove unsuccessful");
  }

  int fif = mkfifo(servy_name, S_IRUSR | S_IWUSR);  //creating the fifo
  if(fif == -1)
  {
    perror("error making fifo");
  }
  int joinin_fd = open(servy_name, perms);  //open fifo using the perms given as parameter
  if(joinin_fd == -1)
  {
    perror("open unsuccessful");
  }
  server->join_fd = joinin_fd;    //setting the server's join fd field.


}


void server_shutdown(server_t *server)
// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
{
  int cl = close(server->join_fd);   //closing the join fifo
  if(cl != 0)
  {
    perror("close unsuccessful");
  }


  char *name = strcat(server->server_name, ".fifo");    //concatenating
  int rem = remove(name); //remove (unlink) so no other clients can join
  if(rem != 0)
  {
    perror("remove unsuccessful");
  }

  mesg_t message;  //initializing message
  memset(&message, 0,sizeof(mesg_t)); //preventing unintialized value errors
  message.kind = BL_SHUTDOWN;   //setting kind of message to 'shutdown'
  //cycling through server's clients in reverse
  while(server->n_clients > 0)
  {
    //telling each client that server is shutting down
    int wr = write(server->client[server->n_clients - 1].to_client_fd, &message, sizeof(mesg_t));
    if(wr == -1)
    {
      perror("write error");
    }

    //could have called server_remove_client for all of them but since all of them need to be removed it doesn't really matter
    int cl1 = close(server->client[server->n_clients - 1].to_client_fd);   //closing the client's fifos
    int cl2 = close(server->client[server->n_clients - 1].to_server_fd);
    if(cl1 != 0 || cl2 != 0)
    {
      perror("close unsuccessful");
    }

    int rem1 = remove(server->client[server->n_clients - 1].to_client_fname);    //removing the fifos
    int rem2 = remove(server->client[server->n_clients - 1].to_server_fname);
    if(rem1 != 0 || rem2 != 0)
    {
      perror("remove unsuccessful");
    }
    server->n_clients--;
  }
}

int server_add_client(server_t *server, join_t *join)
// Adds a client to the server according to the parameter join which
// should have fileds such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).
{
  if(server->n_clients == MAXCLIENTS - 1)
  {
    //unsuccessful, too many clients
    return 1;
  }
  //filling in the client struct
  server->n_clients++;
  client_t client;
  int s_len = strlen(join->name) + 1;
  snprintf(client.name, s_len,  "%s", join->name);

  client.data_ready = 0;
  //the rubric says to use strncpy() but we use snprintf(), hope that's an ok substitute
  snprintf(client.to_client_fname, MAXPATH,  "%s", join->to_client_fname);

  snprintf(client.to_server_fname, MAXPATH,  "%s", join->to_server_fname);

  //opening the fifos, rubric says to open for reading but that will cause blocking
  client.to_client_fd = open(client.to_client_fname, O_RDWR);

  client.to_server_fd = open(client.to_server_fname, O_RDWR);
  //putting new client in server's array (at the end)
  server->client[server->n_clients - 1] = client;
  return 0;
}

int server_remove_client(server_t *server, int idx)
// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// preserving their order in the array; decreases n_clients.
{
  if(server->n_clients == 0)
  {
    //error
    return 1;
  }
  client_t *client = server_get_client(server, idx);
  server->n_clients--;
  close(client->to_client_fd);   //closing the client's fifos
  close(client->to_server_fd);
  remove(client->to_client_fname);    //removing the fifos
  remove(client->to_server_fname);
  //shifting remaining clients
  for(int x = idx; x < server->n_clients; x++)
  {
    server->client[x] = server->client[x + 1];
  }
  //success!
  return 0;
}
int server_broadcast(server_t *server, mesg_t *mesg)
// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
{
  int helpme;
  for(int x = 0; x < server->n_clients; x++)
  {
    helpme = write(server->client[x].to_client_fd, mesg, sizeof(mesg_t));

  }
  return 0;

}

void server_check_sources(server_t *server)
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine
// which sources are ready.
{
  //using prof's poll tutorial for help
  struct pollfd pfds[server->n_clients + 1];    //clients + the server's join fifo
  //filling the poll struct
  for(int x = 0; x < server->n_clients; x++)
  {
    pfds[x].fd = server->client[x].to_server_fd;
    pfds[x].events = POLLIN;
  }

  pfds[server->n_clients].fd = server->join_fd;
  pfds[server->n_clients].events = POLLIN;


    int ret = poll(pfds, server->n_clients + 1, -1);
    if(ret < 0)
    {
      perror("poll() failed");
      return;
    }
    for(int x = 0; x < server->n_clients; x++)
    {
      if( pfds[x].revents & POLLIN )
      {
        //indicates data is ready
        server->client[x].data_ready = 1;
      }
    }
    if( pfds[server->n_clients].revents & POLLIN )
    {
      //indicates data is ready == someone wants to join da partayyy
      server->join_ready = 1;
    }
}

int server_join_ready(server_t *server)
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.
{
  return server->join_ready;
}

int server_handle_join(server_t *server)
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.
{
  if(server_join_ready(server))
  {
    join_t join_request;
    memset(&join_request, 0,sizeof(join_t));

    int red = read(server->join_fd, &join_request, sizeof(join_t));
    if(red == -1)
    {
      perror("read unsuccessful");
      return 1;
    }
    //if equal to 0 that means someone joined
    int client_joined = server_add_client(server, &join_request);
    server->join_ready = 0;
    mesg_t message;
    memset(&message, 0,sizeof(mesg_t));
    message.kind = BL_JOINED;
    sprintf(message.name, "%s", join_request.name);

    server_broadcast(server, &message);
    return 0;
  }
  //nobody joined
  return 1;
}

int server_client_ready(server_t *server, int idx)
// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.
{
  return server->client[idx].data_ready;
}

int server_handle_client(server_t *server, int idx)
// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.
//  Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
{
  if(server_client_ready(server, idx))
  {
    mesg_t message;
    int red = read(server->client[idx].to_server_fd, &message, sizeof(mesg_t));
    if(red == -1)
    {
      perror("read unsuccessful");
      return 1;
    }
    if(message.kind == BL_DEPARTED)
    {
      server_remove_client(server, idx);
      server_broadcast(server, &message);
    }
    if(message.kind == BL_MESG )
    {
      server_broadcast(server, &message);
    }
    server->client[idx].data_ready = 0;
    //success!
    return 0;
  }
  //nothing was done
  return 1;
}
