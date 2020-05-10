#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

#define MAX_USERNAME_LEN 10
#define MAX_MESSAGE_LEN 21
#define MAX_RAW_LEN MAX_USERNAME_LEN + MAX_MESSAGE_LEN + 2
#define MAX_CONNECTIONS 3


// Keep the username in a global so we can access it from the callback
const char* username;
int connections = 0;
int peer_socket_fds[MAX_CONNECTIONS] = {-1};

// Turns msg_struct_t into raw string of bytes
void stringify_msg(const char *username, const char *message, char *msg_raw){
  strncpy(msg_raw, username, MAX_USERNAME_LEN + 1);
  strncpy(msg_raw + MAX_USERNAME_LEN + 1, message, MAX_MESSAGE_LEN + 1);
}

// Turns msg_raw into msg_struct_t
void destringify_msg(char *username, char *message, char *msg_raw){
  strncpy(username, msg_raw, MAX_USERNAME_LEN + 1);
  strncpy(message, msg_raw + MAX_USERNAME_LEN + 1, MAX_MESSAGE_LEN + 1);
}

// This function is run by a worker thread. It acts as a server to receive connections from new peers
void *server_worker(void *server_socket_fd){
  // Loop to accept new connections
  int server_fd = *(int*)server_socket_fd;
  // Start listening for connections, with a maximum of one queued connection
  if(listen(server_fd, 1)) {
    perror("listen failed");
    exit(2);
  }
  while(1){
    int peer_socket_fd = server_socket_accept(server_fd); // Blocking until new connection
    if(peer_socket_fd == -1){
      perror("Server socket was not opened");
      exit(2);
    }
    if(connections >= MAX_CONNECTIONS){
      perror("Too many connections attempted");
      exit(2);
    }
    // Add new socket to list of peers
    peer_socket_fds[connections++] = peer_socket_fd;
  }
}

// Broadcasts message to all connected peers, except for source peer
void broadcast(const char* message, int source_peer){
  for(int i=0; i < connections; i++){
    if(peer_socket_fds[i] == -1) break;
    if(i == source_peer) continue;
    if(write(peer_socket_fds[i], message, MAX_RAW_LEN) == -1){
      perror("Could not write to peer");
      exit(2);
    }
  }
}

// This thread function listens for messages and displays and broadcasts messages.
void *listener_worker(){
  char buf[MAX_RAW_LEN];
  while(1){
    for(int i=0; i < connections; i++){
      if(peer_socket_fds[i] == -1) break;
      int rd = read(peer_socket_fds[i], buf, MAX_RAW_LEN);
      if(rd == 0) continue; // Nothing to be read
      if(rd == -1){
        perror("Could not receive message");
        exit(EXIT_FAILURE);
      }
      char rec_username[MAX_USERNAME_LEN+1], rec_message[MAX_MESSAGE_LEN+1];
      memset(rec_username, '\0', sizeof(rec_username));
      memset(rec_message, '\0', sizeof(rec_message));
      const char *display_username = rec_username;
      const char *display_msg = rec_message;
      destringify_msg(rec_username, rec_message, buf);
      ui_display(display_username, display_msg);
      broadcast(buf, i);
    }
  }
}


// This function is run whenever the user hits enter after typing a message
void input_callback(const char* message) {
  if(strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {
    ui_exit();
  } else {
    ui_display(username, message);
    // build p2p message
    char msg_raw[MAX_RAW_LEN];
    memset(msg_raw, '\0', sizeof(msg_raw));
    stringify_msg(username, message, msg_raw);
    // broadcast msg to all peers
    broadcast(msg_raw, -1);
  }
}

int main(int argc, char** argv) {
  // Make sure the arguments include a username
  if(argc != 2 && argc != 4) {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the username in a global
  username = argv[1];

  // Set up a server socket to accept incoming connections
  unsigned short serve_port = 0;
  int self_socket_fd = server_socket_open(&serve_port);
  if(self_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Did the user specify a peer we should connect to?
  if(argc == 4) {
    // Unpack arguments
    char* peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);
    // connect to peer port
    int peer_socket_fd = socket_connect(peer_hostname, peer_port);
    if(peer_socket_fd == -1) {
      perror("Failed to connect");
      exit(EXIT_FAILURE);
    }
    peer_socket_fds[connections++] = peer_socket_fd;
  }

  //run listen thread
  pthread_t server_thread;
  if(pthread_create(&server_thread, NULL, server_worker, &self_socket_fd) != 0) perror("Could not create thread");
  pthread_t listener_thread;
  if(pthread_create(&listener_thread, NULL, listener_worker, NULL) != 0) perror("Could not create thread");


  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  // Once the UI is running, you can use it to display log messages
  char print_buf[26];
  sprintf(print_buf, "Listening on port %u", serve_port);
  ui_display("INFO", print_buf);

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();

  return 0;
}
