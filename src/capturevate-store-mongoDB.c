#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include <time.h>
#include <sys/types.h>

#include <event.h>
#include <evhttp.h>

#include <mongo.h>
#include <hiredis.h>

#define PRIORITY_DELAY_US 10000

#define MAX_KEY_SIZE 1024
#define CONF_LINE_SIZE 1024

#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_MONGO_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379
#define DEFAULT_MONGO_PORT 27017
#define DEFAULT_REDIS_TIMEOUT 2
#define DEFAULT_MONGO_COLLECTION "log.useractions"
#define DEFAULT_NO_RESULTS_SLEEP 3

typedef struct {
   char mongodb_addr[CONF_LINE_SIZE];
   short mongodb_port;

   char  redis_addr[CONF_LINE_SIZE];
   short redis_port;

   char mongodb_collection[CONF_LINE_SIZE];
   struct timeval redis_timeout;

   int sleep;
} config_t;

static void parseConfFile(char *filename, config_t *config);

int main(int argc, char **argv) {
   int i = 0;
   config_t config;

   mongo mongoDB;
   bson document;

   redisContext *redis = NULL;
   redisReply *reply = NULL;

   bool isEmpty = false;

   char key[MAX_KEY_SIZE] = "";

   config.sleep = DEFAULT_NO_RESULTS_SLEEP;

   config.redis_timeout.tv_usec = 0;
   config.redis_timeout.tv_sec = DEFAULT_REDIS_TIMEOUT;


   strncpy(config.mongodb_collection, DEFAULT_MONGO_COLLECTION, CONF_LINE_SIZE);

   strncpy(config.mongodb_addr, DEFAULT_MONGO_HOST, CONF_LINE_SIZE);
   strncpy(config.redis_addr, DEFAULT_REDIS_HOST, CONF_LINE_SIZE);

   config.mongodb_port = DEFAULT_MONGO_PORT;
   config.redis_port = DEFAULT_REDIS_PORT;

   for (i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "-c") == 0 && (i+1 < argc)) {
         parseConfFile(argv[i+1], &config);
         i++;
      }
   }

   #ifdef DEBUG
   printf("Connecting to redis on %s:%d\n", config.redis_addr, config.redis_port);
   #endif

   redis = redisConnectWithTimeout(config.redis_addr, config.redis_port, config.redis_timeout);
   
   #ifdef DEBUG
   printf("   ...Connected.\n");
   #endif


   #ifdef DEBUG
   printf("Connected to MongoDB on %s:%d\n", config.mongodb_addr, config.mongodb_port);
   #endif
   
   if (mongo_client(&mongoDB, config.mongodb_addr, config.mongodb_port) != MONGO_OK) {
      switch( mongoDB.err ) {
         case MONGO_CONN_NO_SOCKET:
            fprintf(stderr, "MongoDB Error: Could not create a socket!\n");
            break;
         case MONGO_CONN_FAIL:
            fprintf(stderr, "MongoDB Error: Could not connect to mongod. Make sure it's listening at 127.0.0.1:27017.\n");
            break;
         default:
            fprintf(stderr, "MongoDB Error: Could not connect to mongod, error %d\n", mongoDB.err);
            break;
      }

      fprintf(stderr, "Exiting...\n");
      return EXIT_FAILURE;
   }

   #ifdef DEBUG
   printf("   ...Connected.\n");
   #endif

   
   while (1) {
      isEmpty = false;
      reply = redisCommand(redis, "LPOP capturevate_keys");
      if (reply->type == REDIS_REPLY_STRING) {
         strncpy(key, reply->str, MAX_KEY_SIZE-1);
      } else {
         isEmpty = true;
      }

      freeReplyObject(reply);

      if (!isEmpty) {
         printf("key: %s\n", key);

         reply = redisCommand(redis, "HGETALL %s", key);

         // ensure an even-lengthed array for key-value pairs (alternate)
         if (reply->type == REDIS_REPLY_ARRAY && (reply->elements % 2 == 0)) {
            bson_init(&document);
            bson_append_new_oid(&document, "_id");
            
            #ifdef DEBUG
            printf("\nRedis:\n");
            #endif

            for (i = 0; i != reply->elements; i += 2) {
               if (strncmp(reply->element[i]->str, "timestamp", MAX_KEY_SIZE) == 0) {
                  bson_append_int(&document, "timestamp", atoi(reply->element[i+1]->str));
               } else {
                  bson_append_string(&document, reply->element[i]->str, reply->element[i+1]->str);
               }

               #ifdef DEBUG
               printf("%s) %s\n", reply->element[i]->str, reply->element[i+1]->str);
               #endif
            }
            bson_finish(&document);

            #ifdef DEBUG
            printf("Storing MongoDB Document:\n");
            bson_print(&document);
            #endif

            if (mongo_insert(&mongoDB, config.mongodb_collection, &document, NULL) != MONGO_OK) {
               fprintf(stderr, "MongoDB Error: Failed to insert document with error %d\n", mongoDB.err);
            } else {
               #ifdef DEBUG
               printf("   ...Complete.\n");
               #endif
            }

            bson_destroy(&document);
         }
         freeReplyObject(reply);

         // remove key from redis
         reply = redisCommand(redis, "DEL %s", key);
         freeReplyObject(reply);

      } else {
         #ifdef DEBUG
         printf("No Results, sleeping for %ds\n", config.sleep);
         #endif
         sleep(config.sleep);
      }

      usleep(PRIORITY_DELAY_US);
   }

   mongo_destroy(&mongoDB);
   redisFree(redis);

   return EXIT_SUCCESS;
}

static void parseConfFile(char *filename, config_t *config) {
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
               if (strncmp(property, "REDIS_ADDRESS", CONF_LINE_SIZE) == 0) {
                  strncpy(config->redis_addr, value, CONF_LINE_SIZE);
               } else if (strncmp(property, "REDIS_PORT", CONF_LINE_SIZE) == 0) {
                  config->redis_port = atoi(value);
               } else if (strncmp(property, "MONGODB_ADDRESS", CONF_LINE_SIZE) == 0) {
                  strncpy(config->mongodb_addr, value, CONF_LINE_SIZE);
               } else if (strncmp(property, "MONGODB_PORT", CONF_LINE_SIZE) == 0) {
                  config->mongodb_port = atoi(value);
               } else if (strncmp(property, "MONGODB_COLLECTION", CONF_LINE_SIZE) == 0) {
                  strncpy(config->mongodb_collection, value, CONF_LINE_SIZE);
               } else if (strncmp(property, "REDIS_TIMEOUT", CONF_LINE_SIZE) == 0) {
                  config->redis_timeout.tv_sec = atoi(value);
               } else if (strncmp(property, "NO_RESULTS_SLEEP", CONF_LINE_SIZE) == 0) {
                  config->sleep = atoi(value);
               }
            }
         }
      }
      fclose(fp);
   }
}
