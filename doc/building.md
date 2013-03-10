Building Capturevate
---------------------

Library Requirements:
- libevent (https://github.com/libevent/libevent)
- hiredis (https://github.com/redis/hiredis)
- mongo-c-driver (https://github.com/mongodb/mongo-c-driver)


#### LibEvent

LibEvent can normally be installed via a package manager. e.g. <code>apt-get install libevent-dev</code>

Otherwise:

    git clone https://github.com/libevent/libevent
    cd libevent
    ./configure && make

#### HiRedis

    git clone https://github.com/redis/hiredis
    cd hiredis
    make
    make install

(the last step may require sudo depending on your system setup)

#### MongoC

    git clone https://github.com/mongodb/mongo-c-driver
    cd mongo-c-driver
    scons
    make install

(the last step may require sudo depending on your system setup)

### Capturevate
    
Once the libraries are installed:

    make

Will create the binaries in the bin/ folder