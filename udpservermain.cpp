#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <protocol.h>
#include <ctime>
#include <chrono>
#include <string>

// Included to get the support library
#include <calcLib.h>
#include <map>
#include <unordered_map>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;
using Clock = std::chrono::steady_clock;

int generateTask(char* buffer, size_t bufsize);
bool isCalcMessage(const char* buf, int byte_size);
bool isCalcProtocol(const char* buf, int byte_size);

struct ClientResult{
  int result;
  int id;
  socklen_t addrLen;
  Clock::time_point deadline;     
};

struct ClientKey{
  sockaddr_storage addr;
  socklen_t len;

  bool operator==(const ClientKey &other) const {
    return len == other.len && memcmp(&addr, &other.addr, len) == 0;
  }
};

struct ClientKeyHash {
  std::size_t operator()(const ClientKey &k) const {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&k.addr);
    return std::hash<std::string>()(std::string(data, data + k.len));
  }
};
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

  fd_set reading;
  struct timeval timeout;
  int rc;

  std::unordered_map<ClientKey, ClientResult, ClientKeyHash> pending;

  while(true){
    FD_ZERO(&reading);
    FD_SET(sockfd, &reading);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    rc = select(sockfd+1, &reading, NULL, NULL, &timeout);

    auto now = Clock::now();
    for(auto it = pending.begin(); it != pending.end();){
        if(now > it->second.deadline){
          it = pending.erase(it);
        }else{
          ++it;
        }
    }

    if(rc == 0){
      printf("Waiting for msg\n");
      continue;
    }
    else if(rc < 0){
      perror("select");
      break;
    }

    if(FD_ISSET(sockfd, &reading)){
      char buf[1500];
      struct sockaddr_storage clientAddr;
      socklen_t addrLen = sizeof(clientAddr);

      ssize_t byte_size = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&clientAddr, &addrLen);
      if(byte_size <= 0){
        fprintf(stderr, "ERROR: read failed!\n");
        return -1;
      }
      printf("Bite_size %zd\n", byte_size);

      if(isCalcMessage(buf, byte_size)){
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

        ssize_t sent = sendto(sockfd, &cp, sizeof(cp), 0, (struct sockaddr*)&clientAddr, addrLen);
        if(sent == -1){
          perror("sendto failed");
          continue;
        } 
        printf("sendMessage calcProtocol\n");

        ClientKey key;
        key.addr = clientAddr;
        key.len = addrLen;
        std::to_string(id);

        ClientResult cr;
        cr.result = result;
        cr.id = id;
        cr.deadline = Clock::now() + std::chrono::seconds(10);

        pending[key] = cr;
      }
      
      else if(isCalcProtocol(buf, byte_size)){
        calcMessage reply;
        reply.type = htons(2);
        reply.protocol = htons(17);
        reply.major_version = htons(1);
        reply.minor_version = htons(1);
        
        calcProtocol cp;
        memcpy(&cp, buf, sizeof(cp));
        int32_t inResult = ntohl(cp.inResult);

        int result;
        ClientKey key;
        key.addr = clientAddr;
        key.len = addrLen;

        auto it = pending.find(key);
          if(it != pending.end()){
            ClientResult &cr = it->second;
            
            if(Clock::now() > cr.deadline){
              printf("TIMEOUT\n");
              continue;
            }
            result = cr.result;
          }

        if(inResult == result){
          reply.message = htonl(1);
          printf("OK\n");
        }
        else{
          reply.message = htonl(2);
          printf("NOT OK\n");
        }

        ssize_t sent = sendto(sockfd, &reply, sizeof(reply), 0, (struct sockaddr*)&clientAddr, addrLen);
        if(sent == -1){
          perror("sendto failed");
          return -1;
        }
      }
    
      else if(byte_size > 0){
        buf[byte_size] = '\0';

        if(strcmp(buf, "TEXT UDP 1.1\n") == 0){
          memset(&buf, 0, sizeof(buf));
          int result = generateTask(buf, sizeof(buf));

          ssize_t sent = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&clientAddr, addrLen);
          if(sent == -1){
            perror("sendto failed");
            return -1;
          }

          ClientKey key;
          key.addr = clientAddr;
          key.len = addrLen;

          ClientResult cr;
          cr.result = result;
          cr.deadline = Clock::now() + std::chrono::seconds(10);

          pending[key] = cr;

        }
        else{
          bool isNumber = true;
          for(int i = 0; i < byte_size; i++){
            if(!(isdigit(buf[i]) || buf[i] == '-' || buf[i] == '\n')){
                isNumber = false;
                break;
            }
          }
          if(isNumber == true){
            int clientResult = atoi(buf);
            memset(&buf, 0, sizeof(buf));
            int result;

            ClientKey key;
            key.addr = clientAddr;
            key.len = addrLen;

            auto it = pending.find(key);
            if(it != pending.end()){
                ClientResult &cr = it->second;

                if(Clock::now() > cr.deadline){
                  printf("TIMEOUT\n");
                  continue;
                }
                result = cr.result;
            }
            if(clientResult == result){
              printf("OK\n");
              strcpy(buf, "OK\n");
            }
            else{
              printf("NOT OK\n");
              strcpy(buf, "NOT OK\n");
            }

            ssize_t sent = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&clientAddr, addrLen);
            if(sent == -1){
              perror("sendto failed");
              continue;
            }
          }
        }
      }
      else{
        perror("recv");
      }
    }
  }
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

bool isCalcMessage(const char* buf, int byte_size){
    if(byte_size != sizeof(calcMessage)){
        return false;
    }

    calcMessage cm;
    memcpy(&cm, buf, sizeof(cm));

    uint16_t type = ntohs(cm.type);
    uint32_t message = ntohl(cm.message);
    uint16_t protocol = ntohs(cm.protocol);
    uint16_t majorv = ntohs(cm.major_version);
    uint16_t minorv = ntohs(cm.minor_version);

    if(type == 22 && message == 0 && protocol == 17 && majorv == 1 && minorv == 1){
      return true;
    }

    return false;
}

bool isCalcProtocol(const char* buf, int byte_size){
    if(byte_size != sizeof(calcProtocol)){
        return false;
    }

    calcProtocol cp;
    memcpy(&cp, buf, sizeof(cp));

    uint16_t type = ntohs(cp.type);
    uint16_t majorv = ntohs(cp.major_version);
    uint16_t minorv = ntohs(cp.minor_version);

    if((type == 1 || type == 2) && majorv == 1 && minorv == 1) {
      return true;
    }

    return false;
}
