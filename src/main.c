#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "server.h"


static void
parse_args(int argc, char **argv, bhttp_server *server)
{
    int c;
    while ((c = getopt(argc, argv, "p:d:b:f:al:s")) != -1) {
        switch (c) {
            case 'p':
                server->port = optarg;
                break;
            case 'd':
                server->docroot = optarg;
                break;
            case 'b':
                server->backlog = atoi(optarg);
                break;
            case 'f':
                server->default_file = optarg;
                break;
            case 'a':
                server->daemon = 1;
                break;
            case 'l':
                server->log_file = optarg;
                break;
            case 's':
                server->use_sendfile = 1;
                break;
            case '?':
                if (optopt == 'c' || optopt == 'd' || optopt == 'b')
                {
                    fprintf(stderr, "Error: -%c option missing\n", optopt);
                    exit(1);
                }
                else
                {
                    fprintf(stderr, "Error: -%c unknown option\n", optopt);
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Error parsing options\nExiting...");
                exit(1);
        }
    }

}

int
main(int argc, char **argv)
{
    bhttp_server server = http_server_new();
    parse_args(argc, argv, &server);

    if (server.daemon)
    {
        if(daemon(1, 1) == -1)
        {
            perror("Daemon:");
            exit(1);
        }
    }

    printf("Starting bittyhttp with:\n port: %s\n backlog: %d\n docroot: %s\n logfile: %s\n default file: %s\n\n",
           server.port, server.backlog, server.docroot, server.log_file, server.default_file);

    if (http_server_start(&server) != 0)
      return 1;

    fflush(stdout);

    http_server_run(&server);

    return 0;
}
