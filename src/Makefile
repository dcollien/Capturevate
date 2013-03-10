OPTIMIZATION?=-O2
DEPENDENCY_TARGETS=hiredis mongoc libevent

# Default settings
STD= -std=gnu99 -pedantic
WARN= -Wall
OPT= $(OPTIMIZATION)

CFLAGS= $(STD) $(WARN) $(OPT)

HTTP_SERVICE_NAME := capturevate-server
MONGO_STORAGE_SERVICE_NAME := capturevate-store-mongoDB

HTTP_SERVICE_SRCS := capturevate-server.c
MONGO_STORAGE_SERVICE_SRCS := capturevate-store-mongoDB.c

HTTP_SERVICE_INCLUDE_DIRS := /usr/local/include /usr/local/include/hiredis
HTTP_SERVICE_LIB_DIRS := /usr/local/lib
HTTP_SERVICE_LIBS := hiredis event

MONGO_STORAGE_SERVICE_INCLUDE_DIRS := /usr/local/include /usr/local/include/hiredis
MONGO_STORAGE_SERVICE_LIB_DIRS := /usr/local/lib
MONGO_STORAGE_SERVICE_LIBS := mongoc hiredis event

HTTP_SERVICE_CFLAGS += $(foreach includedir,$(HTTP_SERVICE_INCLUDE_DIRS),-I$(includedir))
HTTP_SERVICE_LDFLAGS += $(foreach librarydir,$(HTTP_SERVICE_LIB_DIRS),-L$(librarydir))
HTTP_SERVICE_LDFLAGS += $(foreach library,$(MONGO_STORAGE_SERVICE_LIBS),-l$(library))

MONGO_STORAGE_SERVICE_CFLAGS += $(foreach includedir,$(MONGO_STORAGE_SERVICE_INCLUDE_DIRS),-I$(includedir))
MONGO_STORAGE_SERVICE_LDFLAGS += $(foreach librarydir,$(MONGO_STORAGE_SERVICE_LIB_DIRS),-L$(librarydir))
MONGO_STORAGE_SERVICE_LDFLAGS += $(foreach library,$(MONGO_STORAGE_SERVICE_LIBS),-l$(library))

.PHONY: all clean distclean

all: $(HTTP_SERVICE_NAME) $(MONGO_STORAGE_SERVICE_NAME)

$(HTTP_SERVICE_NAME): $(HTTP_SERVICE_SRCS)
	$(CC) $(CFLAGS) $? -o ../bin/$@ $(HTTP_SERVICE_CFLAGS) $(HTTP_SERVICE_LDFLAGS)

$(MONGO_STORAGE_SERVICE_NAME): $(MONGO_STORAGE_SERVICE_SRCS)
	$(CC) $(CFLAGS) $? -o ../bin/$@ $(MONGO_STORAGE_SERVICE_CFLAGS) $(MONGO_STORAGE_SERVICE_LDFLAGS)

clean:
	@- $(RM) ../bin/$(HTTP_SERVICE_NAME)
	@- $(RM) ../bin/$(MONGO_STORAGE_SERVICE_NAME)

distclean: clean