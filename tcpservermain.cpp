#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <protocol.h>

// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

int generateTask(char* buffer, size_t bufsize);


int main(int argc, char *argv[]){
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  char *input = argv[1];
  char *sep = strchr(input, ':');
  
  if (!sep) {
    fprintf(stderr, "Error: input must be in host:port format\n");
    return 1;
  }
  
  // Allocate buffers big enough
  char hoststring[256];
  char portstring[64];
  
  // Copy host part
  size_t hostlen = sep - input;
  if (hostlen >= sizeof(hoststring)) {
    fprintf(stderr, "Error: hostname too long\n");
    return 1;
  }
  strncpy(hoststring, input, hostlen);
  hoststring[hostlen] = '\0';
  
  // Copy port part
  strncpy(portstring, sep + 1, sizeof(portstring) - 1);
  portstring[sizeof(portstring) - 1] = '\0';
  
  printf("TCP server on: %s:%s\n", hoststring,portstring);

  /*My Magic*/
  struct addrinfo hints, *results;
  int sockfd, bind_status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hoststring, portstring, &hints, &results);
  if(status != 0 || results == NULL)
  {
    fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
    return EXIT_FAILURE;
  }

  for(struct addrinfo *p = results; p != NULL; p = p->ai_next){
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(sockfd == -1){
      perror("socket");
      continue;
    }

    int yes = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
      perror("setsockopt");
      close(sockfd);
      sockfd = -1;
      continue;
    }

    if(bind(sockfd, p->ai_addr, p->ai_addrlen) == 0){
      bind_status = 0;
      break;
    }else{
      perror("bind");
      close(sockfd);
      sockfd = -1;
    }
  }
  freeaddrinfo(results);

  if(sockfd == -1){
    fprintf(stderr, "ERROR: CANT CONNECT TO %d\n", sockfd);
    return EXIT_FAILURE;
  }
  if(bind_status == -1){
    freeaddrinfo(results);
    close(sockfd);
    perror("bind");
    fprintf(stderr, "ERROR: CANT BIND to %d\n", sockfd);
    return EXIT_FAILURE;
  }

  int listen_status = listen(sockfd, 5);
  if(listen_status == -1){
    fprintf(stderr, "ERROR: LISTEN FAILED %d\n", sockfd);
    return EXIT_FAILURE;
  }

  struct sockaddr_storage client_addr;
  socklen_t client_addr_size = sizeof(client_addr);

  int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
  if(clientfd == -1){
      perror("accept failed");
      return EXIT_FAILURE;
  }

  char tmsg[] = "TEXT TCP 1.1\nBINART TCP 1.1\n";

  ssize_t sent = write(clientfd, tmsg, strlen(tmsg));
  if(sent == -1){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: sendto failed\n");
    return EXIT_FAILURE;
  }
  printf("Sent to client:\n%s", tmsg);

  fd_set reading;
  struct timeval timeout;
  int rc;

  char buf[1500];
  memset(&buf, 0, sizeof(buf));
  ssize_t byte_size;;
    
  FD_ZERO(&reading);
  FD_SET(clientfd, &reading);
  memset(&timeout, 0, sizeof(timeout));

  timeout.tv_sec = 10;
  rc = select(clientfd+1, &reading, NULL, NULL, &timeout);

  if(rc > 0){
    if(FD_ISSET(clientfd, &reading)){
      byte_size = read(clientfd, buf, sizeof(buf));
    }
  }else if(rc == 0){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: TIMEOUT\n");
    return EXIT_FAILURE;
  }else{
    perror("select");
  }  

  if(byte_size <= 0){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: read failed!\n");
    return EXIT_FAILURE;
  }
  printf("Sent from client:\n%s", buf);
  buf[byte_size] = '\0'; 

  if((strcmp(buf, "TEXT TCP 1.1 OK\n") == 0)){

    memset(&buf, 0, sizeof(buf));
    int result = generateTask(buf, sizeof(buf));

    sent = write(clientfd, buf, strlen(buf));
    if(sent == -1){
      freeaddrinfo(results);
      close(sockfd);
      fprintf(stderr, "ERROR: sendto failed\n");
      return EXIT_FAILURE;
    }
    printf("Sent to client:\n%s", buf);


    FD_ZERO(&reading);
    FD_SET(clientfd, &reading);
    memset(&timeout, 0, sizeof(timeout));

    timeout.tv_sec = 10;
    rc = select(clientfd+1, &reading, NULL, NULL, &timeout);
    memset(&buf, 0, sizeof(buf));
    if(rc > 0){
      if(FD_ISSET(clientfd, &reading)){
        byte_size = read(clientfd, buf, sizeof(buf));
      }
    }else if(rc == 0){
      freeaddrinfo(results);
      close(sockfd);
      fprintf(stderr, "ERROR: TIMEOUT\n");
      return EXIT_FAILURE;
    }else{
      perror("select");
    }  

    if(byte_size <= 0){
      freeaddrinfo(results);
      close(sockfd);
      fprintf(stderr, "ERROR: read failed!\n");
      return EXIT_FAILURE;
    }
    printf("Sent from client:\n%s", buf);

    buf[byte_size] = '\0';
    int clientResult = atoi(buf);
    memset(&tmsg, 0, sizeof(buf));

    if(clientResult == result){
      strcpy(buf, "OK\n");
    }
    else{
      strcpy(buf, "NOT OK\n");
    }

    sent = write(clientfd, buf, strlen(buf));
    if(sent == -1){
      freeaddrinfo(results);
      close(sockfd);
      fprintf(stderr, "ERROR: sendto failed\n");
      return EXIT_FAILURE;
    }
    printf("Sent to client:\n%s", buf);

  }
  
  else if(strcmp(buf, "BINARY TCP 1.1 OK\n") == 0){

  }
  else{
    fprintf(stderr, "ERROR: MISSMATCH PROTOCOL\n");
    return EXIT_FAILURE;
  }

  printf("EXIT SERVER\n");
  return EXIT_SUCCESS;
}

int generateTask(char* buffer, size_t bufsize){
  char* arith = randomType();
  int v1 = randomInt();
  int v2 = randomInt();
  int result;

  if(strcmp(arith, "add") == 0){
    result = v1 + v2;
  }
  else if(strcmp(arith, "sub") == 0){
    result = v1 - v2;     
  }
  else if(strcmp(arith, "mul") == 0){
    result = v1 * v2;
  }
  else if(strcmp(arith, "div") == 0){
    if(v2 == 0)
      v2 = 1;  
    result = v1 / v2;
  }
  else{
    fprintf(stderr, "ERROR: Invalid operation\n");
    return EXIT_FAILURE;
  }

  snprintf(buffer, bufsize, "%s %d %d\n", arith, v1, v2);

  return result;
}