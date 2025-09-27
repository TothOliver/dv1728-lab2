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
#include <ctime>

// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

int handleClient(int clientfd);
int writeMessage(int sockfd, const char* msg);
int readMessage(int sockfd, char* buf, size_t bufsize, int timeOutSec);
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

  int listen_status = listen(sockfd, 10);
  if(listen_status == -1){
    fprintf(stderr, "ERROR: LISTEN FAILED %d\n", sockfd);
    return EXIT_FAILURE;
  }

  struct sockaddr_storage client_addr;
  socklen_t client_addr_size = sizeof(client_addr);

  while(true){
    int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
    if(clientfd == -1){
        perror("accept failed");
        continue;
    }

    pid_t pid = fork();

    if(pid < 0){
      perror("fork");
    }
    else if(pid == 0){
      close(sockfd);

      handleClient(clientfd);

      close(clientfd);
      _exit(0);
    }
    else{
      close(clientfd);
    }
  }

  printf("EXIT SERVER\n");
  return EXIT_SUCCESS;
}

int handleClient(int clientfd){

  char tmsg[] = "TEXT TCP 1.1\nBINARY TCP 1.1\n";
  if(writeMessage(clientfd, tmsg) == -1)
    return EXIT_FAILURE;
    
  char buf[1500];
  memset(&buf, 0, sizeof(buf));
  if(readMessage(clientfd, buf, sizeof(buf), 5) == -1)
    return EXIT_FAILURE;

  if((strcmp(buf, "TEXT TCP 1.1 OK\n") == 0)){

    memset(&buf, 0, sizeof(buf));
    int result = generateTask(buf, sizeof(buf));

    if(writeMessage(clientfd, buf) == -1)
      return EXIT_FAILURE;

    memset(&buf, 0, sizeof(buf));
    if(readMessage(clientfd, buf, sizeof(buf), 5) == -1)
      return EXIT_FAILURE;

    int clientResult = atoi(buf);

    if(clientResult == result){
      strcpy(buf, "OK\n");
    }
    else{
      strcpy(buf, "NOT OK\n");
    }

    if(writeMessage(clientfd, buf) == -1)
      return EXIT_FAILURE;

  }
  
  else if(strcmp(buf, "BINARY TCP 1.1 OK\n") == 0){

    calcProtocol cp;

    int a, result;
    char* arith = randomType();
    int v1 = randomInt();
    int v2 = randomInt();

    if(v2 == 0 && strcmp(arith, "div") == 0) v2 = 1;
    if(strcmp(arith, "add") == 0){
      a = 1;
      result = v1 + v2;
    }
    if(strcmp(arith, "sub") == 0){
      a = 2;
      result = v1 - v2;
    }
    if(strcmp(arith, "mul") == 0){
      a = 3;
      result = v1 * v2;
    }
    if(strcmp(arith, "div") == 0){
      a = 4;
      result = v1 / v2;
    }

    srand(time(NULL));
    uint32_t id = rand();

    cp.type = htons(1);
    cp.major_version = htons(1);
    cp.minor_version = htons(1);
    cp.id = htonl(id);
    cp.arith = htonl(a);
    cp.inValue1 = htonl(v1);
    cp.inValue2 = htonl(v2);
    cp.inResult = htonl(0);

    if(write(clientfd, &cp, sizeof(cp)) == -1){
      perror("write");
      return EXIT_FAILURE;
    }


    calcProtocol reply;
    ssize_t r = read(clientfd, &reply, sizeof(reply));
    if(r != sizeof(reply)){
      perror("read");
      return EXIT_FAILURE;
    }
    printf("HERE\n");

    reply.type          = ntohs(reply.type);
    reply.major_version = ntohs(reply.major_version);
    reply.minor_version = ntohs(reply.minor_version);
    reply.id            = ntohl(reply.id);
    reply.arith         = ntohl(reply.arith);
    reply.inValue1      = ntohl(reply.inValue1);
    reply.inValue2      = ntohl(reply.inValue2);
    reply.inResult      = ntohl(reply.inResult);

    if(reply.id != id){
      fprintf(stderr, "ERROR: INVALID IP %d", reply.id);
      return EXIT_FAILURE;
    }

    calcMessage response;
    response.type = htons(22);
    response.protocol = htons(6);
    response.major_version = htons(1);
    response.minor_version = htons(1);

    if(reply.inResult == result){
      response.message = htonl(1);
    }else{
      response.message = htonl(2);
    }

    if(write(clientfd, &response, sizeof(response)) == -1){
      perror("write");
      return EXIT_FAILURE;
    }

  }
  else{
    fprintf(stderr, "ERROR: MISSMATCH PROTOCOL\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int writeMessage(int fd, const char* msg){
  ssize_t sent = write(fd, msg, strlen(msg));
  if(sent == -1){
    close(fd);
    perror("write");
    return -1;
  }

  printf("Sent to client:\n%s", msg);
  return 0;
}

int readMessage(int fd, char* buf, size_t bufsize, int timeOutSec){
  fd_set reading;
  struct timeval timeout;
  int rc;

  FD_ZERO(&reading);
  FD_SET(fd, &reading);

  timeout.tv_sec = timeOutSec;

  rc = select(fd+1, &reading, NULL, NULL, &timeout);
  if(rc == 0){
    close(fd);
    fprintf(stderr, "ERROR: TIMEOUT\n");
    return EXIT_FAILURE;
  }
  else if(rc < 0){
    perror("select");
    return -1;
  }

  ssize_t byte_size = read(fd, buf, bufsize-1);
  if(byte_size <= 0){
    fprintf(stderr, "ERROR: read failed!\n");
    return -1;
  }

  buf[byte_size] = '\0';
  printf("Sent from client:\n%s", buf);

  return 0;
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