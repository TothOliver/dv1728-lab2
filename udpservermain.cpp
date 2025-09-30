#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <protocol.h>
#include <ctime>

// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

int sendMessage(int sockfd, const void* msg, size_t msgSize, struct sockaddr_in* clientAddr, socklen_t addrLen);
int recvMessage(int sockfd, char* buf, size_t bufsize, int timeOutSec, struct sockaddr_in* clientAddr, socklen_t* addrLen);
int generateTask(char* buffer, size_t bufsize);
struct addrinfo* reverseList(struct addrinfo* head);


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
  
  printf("UDP Server on: %s:%s\n", hoststring,portstring);

  /*My Magic*/
  struct addrinfo hints, *results;
  int sockfd, bind_status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hoststring, portstring, &hints, &results);
  if(status != 0 || results == NULL)
  {
    fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
    return EXIT_FAILURE;
  }

  results = reverseList(results);

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

  fd_set reading;
  struct timeval timeout;
  int rc;

  while(true){
    FD_ZERO(&reading);
    FD_SET(sockfd, &reading);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    rc = select(sockfd+1, &reading, NULL, NULL, &timeout);

    if(rc == 0){
      continue;
    }
    else if(rc < 0){
      perror("select");
      break;
    }

    if (FD_ISSET(sockfd, &reading)) {
      char buf[1500];
      struct sockaddr_in clientAddr;
      socklen_t addrLen = sizeof(clientAddr);
      int byte_size = recvMessage(sockfd, buf, sizeof(buf), 5, &clientAddr, &addrLen);
      
      if(byte_size == sizeof(calcMessage)){ //BINARY
        calcMessage cm;
        memcpy(&cm, buf, sizeof(cm));

        uint16_t type = ntohs(cm.type);
        uint32_t message = ntohl(cm.message);
        uint16_t protocol = ntohs(cm.protocol);
        uint16_t majorv = ntohs(cm.major_version);
        uint16_t minorv = ntohs(cm.minor_version);

        if(type != 22 || message != 0 || protocol != 17 || majorv != 1 || minorv != 1){
          fprintf(stderr, "Incorrect calcMessage\n");
          return EXIT_FAILURE;
        }
        
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

        if(sendMessage(sockfd, &cp, sizeof(cp), &clientAddr, addrLen) == -1){
          return EXIT_FAILURE;
        }
        printf("sendMessage\n");

        memset(&buf, 0, sizeof(buf));
        addrLen = sizeof(clientAddr);
        int byte_size = recvMessage(sockfd, buf, sizeof(buf), 5, &clientAddr, &addrLen);
        
        if(byte_size != sizeof(calcProtocol)){
          printf("Wrong Proctocol from recv\n");
          return EXIT_FAILURE;
        }

        calcProtocol respons;
        memcpy(&respons, buf, sizeof(respons));

        uint16_t type2 = ntohs(respons.type);
        uint16_t major_version = ntohs(respons.major_version);
        uint16_t minor_version = ntohs(respons.minor_version);
        uint32_t responsId = ntohl(respons.id);
        int32_t inResult = ntohl(respons.inResult);

        if(type2 != 2 || major_version != 1 || minor_version != 1 || responsId != id){
          printf("type: %d, majv: %d, minv: %d, responsId: %d, id: %d\n", type, major_version, minor_version, responsId, id);
          fprintf(stderr, "Incorrect calcProtocol\n");
          return EXIT_FAILURE;
        }

        calcMessage reply;
        reply.type = htons(1);
        reply.protocol = htons(17);
        reply.major_version = htons(1);
        reply.minor_version = htons(1);

        if(inResult == result){
          reply.message = htonl(1);
          printf("OK\n");
        }
        else{
          reply.message = htonl(2);
          printf("NOT OK\n");
        }

        if(sendMessage(sockfd, &reply, sizeof(reply), &clientAddr, addrLen) == -1){
          return EXIT_FAILURE;
        }

      }
      
      else if(byte_size >= 12 && byte_size <= 14){ //TEXT
        buf[byte_size] = '\0';
        if(strcmp(buf, "TEXT UDP 1.1\n") != 0){
          fprintf(stderr, "Incorrect TEXT message: %s", buf);
          return EXIT_FAILURE;
        }

        memset(&buf, 0, sizeof(buf));
        int result = generateTask(buf, sizeof(buf));

        if(sendMessage(sockfd, &buf, strlen(buf), &clientAddr, addrLen) == -1){
          return EXIT_FAILURE;
        }

        memset(&buf, 0, sizeof(buf));
        byte_size = recvMessage(sockfd, buf, sizeof(buf), 5, &clientAddr, &addrLen);

        if(byte_size <= 0){
          perror("recvMessage");
          return EXIT_FAILURE;
        }

        int clientResult = atoi(buf);
        memset(&buf, 0, sizeof(buf));

        if(clientResult == result){
          printf("OK\n");
          strcpy(buf, "OK\n");
        }
        else{
          printf("NOT OK\n");
          strcpy(buf, "NOT OK\n");
        }

        if(sendMessage(sockfd, &buf, strlen(buf), &clientAddr, addrLen) == -1){
          return EXIT_FAILURE;
        }

      }
      
      else if(byte_size != -1){
        printf("Wrong message byte_size: %d\n", byte_size);
        return EXIT_FAILURE;
      }
      else{
        printf("Message Invalid");
        continue;
      } 
    }
  }

}

int sendMessage(int sockfd, const void* msg, size_t msgSize, struct sockaddr_in* clientAddr, socklen_t addrLen){
  ssize_t sent = sendto(sockfd, msg, msgSize, 0, (struct sockaddr*)clientAddr, addrLen);
  if(sent == -1){
    perror("sendto failed");
    return -1;
  }
    return 0;
}

int recvMessage(int sockfd, char* buf, size_t bufsize, int timeOutSec, struct sockaddr_in* clientAddr, socklen_t* addrLen){

  ssize_t byte_size = recvfrom(sockfd, buf, bufsize, 0, (struct sockaddr*)clientAddr, addrLen);
  if(byte_size <= 0){
    fprintf(stderr, "ERROR: read failed!\n");
    return -1;
  }

  return byte_size;
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

struct addrinfo* reverseList(struct addrinfo* head){
    struct addrinfo* prev = NULL;
    struct addrinfo* curr = head;
    while (curr != NULL){
        struct addrinfo* next = curr->ai_next;
        curr->ai_next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}
