#include "config.h"

config::config()
{
    port = 9006;
    log_write = 0;       // default mode: sync
    TRIGMode = 0;        // default combination: LT + LT
    listen_TRIGMode = 0; // listenfd: LT
    conn_TRIGMode = 0;   // connfd: LT
    opt_linger = 0;      // default: close linger connection
    sql_number = 8;
    thread_number = 8;
    close_log = 0;   // default: log-writting function on
    actor_model = 0; // default: proactor
}

void config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            port = atoi(optarg);
            break;
        }
        case 'l':
        {
            log_write = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            opt_linger = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_number = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_number = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}
