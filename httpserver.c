#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"

#define LIBHTTP_REQUEST_MAX_SIZE 8192
#define max(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })
/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /* YOUR CODE HERE (Feel free to delete/modify the existing code below) */

  struct http_request *request = http_request_parse(fd);
  //My implement
  char *funcName = request->method;
  char *rPath = request->path;
  int pathLength = strlen(rPath);
  printf("Method: %s\n", funcName);

  /*Get the correct path from the request*/
  char path[strlen(server_files_directory) + pathLength];
  strcpy(path, server_files_directory);
  strcat(path, rPath);
  printf("Main Path: %s\n", path);
  //Rough write Draft code
  /*From look at the end of the path, see if that is a file. If so 
  return 200 OK and file. If not, return 404 NOT Found.
  */
  struct stat buf;
  if (stat(path, &buf) ==  0){
    /*Define the variables */
    char *content = NULL;
    char *type = NULL;
    size_t size = 0;
    char length[LIBHTTP_REQUEST_MAX_SIZE];

    /*The path contains a file*/
    if (S_ISREG(buf.st_mode)){
      type = http_get_mime_type(path);
      size = buf.st_size;
      FILE *fp = fopen(path, "rb");
      content = malloc(size * sizeof(char));
      fread(content,1,size,fp);
      fclose(fp);
    }
    else if(S_ISDIR(buf.st_mode)){ /*The path point to a directory, check if there exist index.html*/
      type = http_get_mime_type("/index.html");
      char tempPath[strlen(path) + strlen("/index.html")];
      strcpy(tempPath,path);
      strcat(tempPath, "/index.html");
      /*Check if the index.html exist*/
      if (access(tempPath, R_OK) != -1){/*Exist, response the file*/
        stat(tempPath,&buf);
        size = buf.st_size;
        FILE *fp = fopen(tempPath, "rb");
        content = malloc(sizeof(char) * size);
        fread(content, 1, size, fp);
        fclose(fp);
      }
      else {/*Not exist, response all of the immediate children of the directory*/
        content = malloc(sizeof(char) * 1024);
        strcpy(content, "<h2>Links of Directories and Parent</h2>");
        strcat(content, "<ul>");
        DIR *dir;
        struct dirent *dp;
        if ((dir = opendir (path)) != NULL) {
          char temp[1024];
          while ((dp = readdir (dir)) != NULL) {
            // printf("%s\n", dp->d_name);
            char *dName = dp->d_name;
            if (strcmp(dp->d_name, ".") == 0){
              continue;
            }
            else if (strcmp(dp->d_name, "..") == 0) {
              if (rPath[pathLength - 1] == '/')
                sprintf(temp,"<li><a href=\"%s..\">Parent directory</a></li>", rPath);
              else
                sprintf(temp,"<li><a href=\"%s/..\">Parent directory</a></li>", rPath);
            }
            else {
              if (rPath[pathLength - 1] == '/')
                sprintf(temp, "<li><a href=\"%s%s\">%s</a></li>", rPath, dName, dName);
              else
                sprintf(temp, "<li><a href=\"%s/%s\">%s</a></li>", rPath, dName, dName);
            }
            strcat(content,temp);
          }
          strcat(content,"</ul>");
          size = strlen(content) * sizeof(char);
        }
        else{
          perror ("Cannot open");
          exit (1);
        }
      }

    }




    snprintf(length, sizeof(length),"%lu", size);
    http_start_response(fd, 200);
    http_send_header(fd, "Content-type", type);
    http_send_header(fd, "Content-length", length);
    http_end_headers(fd);
    http_send_data(fd, content, size);
    return;
  }
  else {
    http_start_response(fd, 404);
    http_send_header(fd, "Content-type", "text/html");
    http_end_headers(fd);
    http_send_string(fd,
        "<center>"
        "<h1>Welcome to httpserver!</h1>"
        "<hr>"
        "<p>Nothing's here yet.</p>"
        "</center>");
  }




  

}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /* YOUR CODE HERE */
  // struct hostent * hostBuf;
  // hostBuf = gethostbyname(server_proxy_hostname);
  // char addr[1024];
  // strcat(addr,hostBuf->h_addr);
  // printf("Server proxy hostname: %s\n", server_proxy_hostname);
  int client_fd = fd;
  int proxy_fd;
  struct hostent *hostBuf;
  struct sockaddr_in server_address;
  hostBuf = gethostbyname(server_proxy_hostname);
  /*Check if there exist host*/
  if (hostBuf == NULL) {
    perror("\"gethostbyname()\" error");
    exit(1);
  }
  /*Create new socket and connect it to the IP address that I got*/
  proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
  memcpy(&server_address, hostBuf->h_addr_list[0], hostBuf->h_length);
  server_address.sin_family = AF_INET;
  // server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_proxy_port);
  /*Check if it can connect to the IP*/
  if (connect(proxy_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
    perror("\"Connect()\" Error");
    exit(1);
  }


  int connected = 1;
  fd_set fdSet;
  
  /*Wait for new data on both sockets*/
  while (connected){
    FD_ZERO(&fdSet);
    FD_SET(client_fd, &fdSet);
    FD_SET(proxy_fd, &fdSet);
    select(max(client_fd,proxy_fd)+1, &fdSet, NULL, NULL, NULL);
    if (FD_ISSET(client_fd, &fdSet)) {
      char buf[LIBHTTP_REQUEST_MAX_SIZE];
      size_t r;
      size_t w;
      r = read(client_fd, buf, LIBHTTP_REQUEST_MAX_SIZE);
      if (r == 0) {
        connected = 0;
      }
      else {
        w = write(proxy_fd,buf, r);
        if (w == -1) connected = 0;
      }
      // if (r == 0 || w == -1) connected = 0;

    }
    if (FD_ISSET(proxy_fd, &fdSet)){
      char buf[LIBHTTP_REQUEST_MAX_SIZE];
      size_t r;
      size_t w;
      r = read(proxy_fd, buf, LIBHTTP_REQUEST_MAX_SIZE);
      if (r == 0){
        connected = 0;
      }
      else {
        w = write(client_fd,buf, r);
        if (w == -1) connected = 0;
      }
      // if (r == 0 || w == -1) connected = 0;
    }

  }
  close(client_fd);
  close(proxy_fd);


}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      // Un-register signal handler (only parent should have it)
      signal(SIGINT, SIG_DFL);
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      perror("Failed to fork child");
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  server_files_directory = malloc(1024);
  getcwd(server_files_directory, 1024);
  server_proxy_hostname = "inst.eecs.berkeley.edu";
  server_proxy_port = 80;

  void (*request_handler)(int) = handle_files_request;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
