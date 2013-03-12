#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <time.h>
#include <sys/types.h>

#include <event.h>
#include <evhttp.h>

#include <hiredis.h>
#include <async.h>
#include <adapters/libevent.h>

#define PACKAGE_STRING "Capturevate 0.1"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 8088

#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379

#define DEFAULT_MAX_PAIRS 24
#define DEFAULT_MAX_FIELD_SIZE 1024
#define DEFAULT_MAX_DATA_SIZE (DEFAULT_MAX_FIELD_SIZE*DEFAULT_MAX_PAIRS*2)

#define DEFAULT_UNKNOWN_KEY '?'

#define RECONNECT_WAIT 3

#define CONF_LINE_SIZE 1024

typedef struct {
   short http_port;
   char  http_addr[CONF_LINE_SIZE];

   short redis_port;
   char  redis_addr[CONF_LINE_SIZE];

   int max_pairs;
   int max_field_size;
   int max_data_size;

   char unknown_key;
} config_t;

static void loggingRequestHandler(struct evhttp_request *request, void *arg);

static void connectCallback(const redisAsyncContext *redis, int status);
static void disconnectCallback(const redisAsyncContext *redis, int status);

static void parseConfFile(char *filename);

static void brokenPipe(int signum);

static inline bool connectRedis(void);
static inline char *copyStringUntil(char *src, char *dest, char endChar, size_t maxSize);
static inline void storeData(char *data, unsigned long timestamp);

static struct event_base *base = NULL;
redisAsyncContext *redis = NULL;

static config_t config;

int main(int argc, char **argv) {
   int argi;

   struct evhttp *http_server = NULL;

   // set defaults      
   config.http_port = DEFAULT_PORT;
   config.http_addr[0] = '\0';

   config.redis_port = DEFAULT_REDIS_PORT;
   strncpy(config.redis_addr, DEFAULT_REDIS_HOST, CONF_LINE_SIZE);

   config.max_pairs = DEFAULT_MAX_PAIRS;
   config.max_field_size = DEFAULT_MAX_FIELD_SIZE;
   config.max_data_size = DEFAULT_MAX_DATA_SIZE;
   config.unknown_key = DEFAULT_UNKNOWN_KEY;



   // parse args and conf file
   for (argi = 1; argi < argc; ++argi) {
      if (strcmp(argv[argi], "-c") == 0 && (argi+1 < argc)) {
         parseConfFile(argv[argi+1]);
         argi++;
      } else if (config.http_addr[0] == '\0') {
         strncpy(config.http_addr, argv[argi], CONF_LINE_SIZE);
      } else {
         config.http_port = atoi(argv[argi]);
      }
   }
   if (config.http_addr[0] == '\0') {
      strncpy(config.http_addr, DEFAULT_HOST, CONF_LINE_SIZE);
   }
   

   // don't exit on broken pipe (just show message)
   signal(SIGPIPE, brokenPipe);

   // initialise new event base for async running
   base = event_base_new();

   // connect to redis
   while (!connectRedis()) {
      sleep(RECONNECT_WAIT);
      fprintf(stderr, "Reconnecting...");
   }

   // attach redis context to the base event loop
   redisLibeventAttach(redis, base);
   
   // add callbacks for connecting and disconnecting
   redisAsyncSetConnectCallback(redis, connectCallback);
   redisAsyncSetDisconnectCallback(redis, disconnectCallback);

   // create a new libevent http server
   http_server = evhttp_new(base);
   // bind to a particular address and port (can be specified via argvs)
   evhttp_bind_socket(http_server, config.http_addr, config.http_port);
   // set loggingRequestHandler as a callback for all requests (no others are caught specifically)
   evhttp_set_gencb(http_server, loggingRequestHandler, NULL);

   printf(PACKAGE_STRING " Logging Server started on %s port %d\n", config.http_addr, config.http_port);
   event_base_dispatch(base);

   evhttp_free(http_server);
   
   event_base_free(base);

   redisAsyncFree(redis);

   fprintf(stderr, PACKAGE_STRING " Logging Server Died\n");

   return EXIT_SUCCESS;
}

static void brokenPipe(int signum) {
   fprintf(stderr, "Broken Pipe\n");
}

static inline bool connectRedis(void) {
   if (redis != NULL) {
      fprintf(stderr, "Removing previous redis connection.\n", );
      redisAsyncFree(redis);
   }
   fprintf(stderr, "Connecting to redis...\n", );
   redis = redisAsyncConnect(config.redis_addr, config.redis_port);
   if (redis == NULL || redis->err) {
      if (redis != NULL) {
         fprintf(stderr, "Redis Connection Error: %s\n", redis->errstr);
      } else {
         fprintf(stderr, "Redis Allocation Error.\n");
      }
      return false;
   }
   return true;
}

static void connectCallback(const redisAsyncContext *redis, int status) {
   if (status != REDIS_OK) {
      fprintf(stderr, "Redis Error: %s\n", redis->errstr);
   }
   printf("Redis Connected...\n");
}

static void disconnectCallback(const redisAsyncContext *redis, int status) {
   if (status != REDIS_OK) {
      fprintf(stderr, "Redis Error: %s\n", redis->errstr);
   }
   printf("Redis Disconnected...\n");

   connectRedis();
}

// N.B. converts ' ' to '+' in src
static inline char *copyStringUntil(char *src, char *dest, char endChar, size_t maxSize) {
   size_t i = 0;
   while (*src != endChar && *src != '\0' && i < maxSize-1) {
      if (dest != NULL) {
         dest[i] = *src;
      }
      // convert spaces to '+' characters in the src string
      if (*src == ' ') {
         *src = '+';
      }
      ++i;
      ++src;
   }
   if (dest != NULL) {
      dest[i] = '\0';
   }

   return src;
}

static inline void storeData(char *data, unsigned long timestamp) {
   static unsigned short requestNum = 0;

   char field[config.max_field_size];
   char user[config.max_field_size];
   char path[config.max_field_size];
   char hmsetCommand[config.max_field_size];

   size_t numPairs = 0;
   bool isUserRead = false;
   bool isPathRead = false;
   char *dataPtr = data;

   // init
   field[0] = '\0';
   user[0] = '?';
   user[1] = '\0';
   path[0] = '?';
   path[1] = '\0';
   hmsetCommand[0] = '\0';
   
   // extract field-value pairs from form-encoded data
   while (*dataPtr != '\0' && numPairs < config.max_pairs) {
      // extract field key portion up until '='
      dataPtr = copyStringUntil(dataPtr, field, '=', config.max_field_size);

      // ensure we're at the '='
      if (*dataPtr != '=') {
         #ifdef DEBUG
         fprintf(stderr, "Unable to process data: %s at '%c'. Expecting '='.\n", data, *dataPtr);
         #endif
         return;
      }
      // convert to space
      *dataPtr = ' ';
      // skip the character
      ++dataPtr;

      // save the user and path variables, if they exist
      if (!isUserRead && strncmp(field, "user", config.max_field_size) == 0) {
         dataPtr = copyStringUntil(dataPtr, user, '&', config.max_field_size);
         isUserRead = true;
      } else if (!isPathRead && strncmp(field, "path", config.max_field_size) == 0) {
         dataPtr = copyStringUntil(dataPtr, path, '&', config.max_field_size);
         isPathRead = true;
      } else {
         dataPtr = copyStringUntil(dataPtr, NULL, '&', config.max_field_size);
      }

      if (*dataPtr == '&') {
         // convert to space
         *dataPtr = ' ';
         // skip the character
         ++dataPtr;
      } else if (*dataPtr != '\0') {
         // ensure we're either at '&' or end of string '\0'
         #ifdef DEBUG
         fprintf(stderr, "Unable to process data: %s at '%c'. Expecting '&'\n", data, *dataPtr);
         #endif
         return;
      }

      // move on to next pair
      ++numPairs;
   }
   // ensure the data's terminated here
   *dataPtr = '\0';

   #ifdef DEBUG
   printf("LPUSH capturevate_keys %s:%s:%ld:%d\n", path, user, timestamp, requestNum);
   printf("HMSET %s:%s:%ld:%d %s\n", path, user, timestamp, requestNum, data);
   #endif

   // ensure this request is in the request log
   redisAsyncCommand(
      redis, NULL, NULL,
      "LPUSH capturevate_keys %s:%s:%ld:%d",
      path, user, timestamp, requestNum
   );

   // store the entry
   sprintf(
      hmsetCommand, 
      "HMSET %s:%s:%ld:%d %s timestamp %ld",
      path, user, timestamp, requestNum, // hash
      data, // fields
      timestamp
   );
   redisAsyncCommand(redis, NULL, NULL, hmsetCommand);

   ++requestNum;
}

void loggingRequestHandler(struct evhttp_request *request, void *arg) {
   unsigned long timestamp = time(NULL);

   char requestText[config.max_data_size];
   char *decodedText = NULL;

   evhttp_add_header(request->output_headers, "Access-Control-Allow-Origin", "*");
   evhttp_send_reply(request, HTTP_OK, "OK", NULL);

   evbuffer_copyout(request->input_buffer, (void *)requestText, config.max_data_size);
   requestText[config.max_data_size-1] = '\0';

   decodedText = evhttp_decode_uri(requestText);

   #ifdef DEBUG
   printf("Storing Data: %s\n", decodedText);
   #endif

   storeData(decodedText, timestamp);

   if (decodedText != NULL) {
      free(decodedText);
   }
   
   return;
}


static void parseConfFile(char *filename) {
   FILE *fp;
   char line[CONF_LINE_SIZE];
   char property[CONF_LINE_SIZE];
   char value[CONF_LINE_SIZE];

   fp = fopen(filename, "r");
   if (fp == NULL) {
      perror(filename);
   } else {
      while (fgets(line, sizeof line, fp) != NULL) {
         if (line[0] != '#') {
            if (sscanf(line, "%s %s", property, value) == 2) {
               if (strncmp(property, "HTTP_ADDRESS", CONF_LINE_SIZE) == 0) {
                  strncpy(config.http_addr, value, CONF_LINE_SIZE);
               } else if (strncmp(property, "HTTP_PORT", CONF_LINE_SIZE) == 0) {
                  config.http_port = atoi(value);
               } else if (strncmp(property, "REDIS_ADDRESS", CONF_LINE_SIZE) == 0) {
                  strncpy(config.redis_addr, value, CONF_LINE_SIZE);
               } else if (strncmp(property, "REDIS_PORT", CONF_LINE_SIZE) == 0) {
                  config.redis_port = atoi(value);
               } else if (strncmp(property, "MAX_PAIRS", CONF_LINE_SIZE) == 0) {
                  config.max_pairs = atoi(value);
               } else if (strncmp(property, "MAX_FIELD_SIZE", CONF_LINE_SIZE) == 0) {
                  config.max_field_size = atoi(value);
               } else if (strncmp(property, "MAX_DATA_SIZE", CONF_LINE_SIZE) == 0) {
                  config.max_data_size = atoi(value);
               } else if (strncmp(property, "UNKNOWN_USER_CHAR", CONF_LINE_SIZE) == 0) {
                  config.unknown_key = value[0];
               }
            }
         }
      }
      fclose(fp);
   }
}
