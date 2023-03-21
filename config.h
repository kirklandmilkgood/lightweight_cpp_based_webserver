#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

class config
{
public:
    config();
    ~config() {}

    void parse_arg(int argc, char *argv[]);

    int port;            // port number
    int log_write;       // mode of log-writing
    int TRIGMode;        // trigger mode of connection and listen event
    int listen_TRIGMode; // listenfd trigger mode
    int conn_TRIGMode;   // connfd trigger mode
    int opt_linger;      // whether to close the linger connection
    int sql_number;      // num of mysql connection
    int thread_number;   // num of threads
    int close_log;       // whether to close the log writting function
    int actor_model;     // reactor/proactor switch
};

#endif