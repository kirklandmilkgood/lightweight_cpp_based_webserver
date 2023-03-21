#include "config.h"

int main(int argc, char *argv[])
{
    std::string user = "root";
    std::string password = "ubuntuarm64";
    std::string database_name = "webserver_users_data";

    // parse arguments in command line
    config config;
    config.parse_arg(argc, argv);

    webserver server;

    // initialization
    server.init(config.port, user, password, database_name, config.log_write, config.opt_linger, config.TRIGMode,
                config.sql_number, config.thread_number, config.close_log, config.actor_model);

    // log file
    server.log_write();

    // database
    server.sql_pool();

    // thread pool
    server.thread_pool();

    // trigger mode
    server.trig_mode();

    // listen mechanism
    server.event_listen();

    // work in event-loop
    server.event_loop();

    return 0;
}