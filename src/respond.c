//
//  respond.c
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "server.h"
#include "request.h"
#include "respond.h"
#include "http_parser.h"
#include "mime_types.h"

void
handle_request(int sock, bhttp_server *server, http_request *request)
{
    response_header rh;
    rh.status.version = "HTTP/1.1";

    switch (request->method) {
        case HTTP_POST:
        case HTTP_GET:
        {
            char *url = url_path(request);

            /* Match handler */

            char *file_path = calloc(strlen(server->docroot) + strlen(url) + 1, 1);
            file_path = strcat(file_path, server->docroot);
            file_path = strcat(file_path, url);
            free(url);

            file_stats fs = get_file_stats(file_path);

            /* found directory, look for default file */
            if (fs.found && fs.isdir) {
                file_path = realloc(file_path, strlen(file_path) + strlen(server->default_file) + 2);
                file_path = strcat(file_path, "/");
                file_path = strcat(file_path, server->default_file);
                fs = get_file_stats(file_path);
            }
            /* found file */
            if (fs.found && !fs.isdir)
            {
                // Add file information to header
                build_header(&rh, &fs);
                send_header(sock, request, &rh, &fs);
                send_file(sock, file_path, &fs, server->use_sendfile);
            }
            /* not found */
            else
            {
		        char resp_not_found[300];
		        char *not_found = "<html><p>404 Not Found</p></html>";
		        sprintf(resp_not_found, "HTTP/1.1 404 Not Found\r\nServer: minihttp\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n%s", (int)strlen(not_found), not_found);
		        send(sock, resp_not_found, strlen(resp_not_found), 0);
            }
            free(file_path);
        }
        break;
    }
}

void
send_header(int sock, http_request *request, response_header *rh, file_stats *fs)
{
    /* TODO:
     * make this cleaner
     */
    bstr headers;
    bstr_init(&headers);

    bstr_append_printf(&headers, "%s %s %s\r\nServer: bittyhttp\r\n", rh->status.version, rh->status.status_code, rh->status.status);

    // Keep Alive
    if (request->keep_alive == HTTP_KEEP_ALIVE)
    {
        bstr_append_printf(&headers, "Connection: Keep-Alive\r\nKeep-Alive: timeout=5\r\n");
    }
    else
    {
        bstr_append_printf(&headers, "Connection: Close\r\n");
    }
    // File content
    bstr_append_printf(&headers, "Content-Type: %s\r\nContent-Length: %lld\r\n\r\n", mime_from_ext(fs->extension), (long long int)fs->bytes);
    send(sock, bstr_cstring(&headers), bstr_size(&headers), 0);
    bstr_free_contents(&headers);
}

// Needs a lot of work
void
send_file(int sock, char *file_path, file_stats *fs, int use_sendfile)
{
    if (use_sendfile)
    {
        int f = open(file_path, O_RDONLY);
        if ( f <= 0 )
        {
            printf("Cannot open file %d\n", errno);
            return;
        }

        off_t len = 0;
        #ifdef __APPLE__
        if ( sendfile(f, sock, 0, &len, NULL, 0) < 0 )
        {
            printf("Mac: Sendfile error: %d\n", errno);
        }
        #elif __linux__
        size_t sent = 0;
        ssize_t ret;
        while ( (ret = sendfile(sock, f, &len, fs->bytes - sent)) > 0 )
        {
            sent += ret;
            if (sent >= fs->bytes) break;
        }
        #endif
        close(f);
    }
    else
    {
        FILE *f = fopen(file_path, "rb");
        if ( f == NULL )
        {
            printf("Cannot open file %d\n", errno);
            return;
        }

        size_t len = 0;
        char *buf = malloc(TRANSFER_BUFFER);
        while ( (len = fread(buf, 1, TRANSFER_BUFFER, f)) > 0 )
        {
            ssize_t sent = 0;
            ssize_t ret  = 0;
            while ( (ret = send(sock, buf+sent, len-sent, 0)) > 0 )
            {
                sent += ret;
                if (sent >= fs->bytes) break;
            }
            if (ret < 0)
            {
                printf("ERROR!!!\n");
                break;
            }

            // Check for being done, either fread error or eof
            if (feof(f) || ferror(f)) {break;}
        }
        free(buf);
        fclose(f);
    }
}

// Needs a lot of work
void
build_header(response_header *rh, file_stats *fs)
{
    if (fs->found)
    {
        rh->status.status_code = "200";
        rh->status.status = "OK";
    } else
    {
        rh->status.status_code = "404";
        rh->status.status = "Not Found";
    }
}

// Needs a lot of work
file_stats
get_file_stats(char *file_path)
{
    file_stats fs;
    struct stat s;

    if (stat(file_path, &s) == -1) {  // Error in stat
        fs.found = 0;
        fs.isdir = 0;
    } else if (S_ISDIR(s.st_mode)) {  // Found a directory
        fs.found = 1;
        fs.isdir = 1;
    } else if (S_ISREG(s.st_mode)) {  // Found a file
        fs.found = 1;
        fs.isdir = 0;
        fs.bytes = s.st_size;
        fs.extension = strrchr(file_path, '.') + 1; // +1 because of '.'
        fs.name = NULL;
    } else {  // Anything else we pretend we didn't find it
        fs.found = 0;
        fs.isdir = 0;
    }

    return fs;
}

// Returns pointer to a string containing a sanitized url path
char *
url_path(http_request *request)
{
    if ((request->parser_url.field_set >> UF_PATH) & 1) {
        char *path = malloc(request->parser_url.field_data[UF_PATH].len + 1);
        html_to_text(bstr_cstring(&(request->uri))+(request->parser_url.field_data[UF_PATH].off),
                     path,
                     request->parser_url.field_data[UF_PATH].len);
        path = sanitize_path(path);
        return path;
    } else { // Something went wrong, return base url path "/"
        char *path = malloc(2);
        path[0] = '/'; path[1] = '\0';
        return path;
    }
}

// Decodes URL strings to text (eg '+' -> ' ' and % hex codes)
void
html_to_text(const char *source, char *dest, size_t length)
{
    size_t n = 0;
    while (*source != '\0' && n < length ) {
        if (*source == '+') {
            *dest = ' ';
        }
        else if (*source == '%') {
            int hex_char;
            sscanf(source+1, "%2x", &hex_char);
            *dest = hex_char;
            source += 2;
            n += 2;
        } else {
            *dest = *source;
        }
        source++;
        dest++;
        n++;
    }
    *dest = '\0';
}

// Removes './' and '/../' from paths
char *
sanitize_path(char *path)
{
    char *token, *tofree;
    tofree = path;

    char *clean = malloc(strlen(path) + 1);
    memset(clean, 0, strlen(path));

    char **argv = malloc(sizeof(char *));
    int argc = 0;

    // Tokenize
    while ((token = strsep(&path, "/")) != NULL) {
        if (strcmp(token, ".") == 0 || strcmp(token, "") == 0) {
            continue;
        } else if (strcmp(token, "..") == 0) {
            argc = MAX(--argc, 0);
        } else {
            argv[argc] = token;
            argc++;
            argv = realloc(argv, sizeof(char *) * (argc + 1));
        }
    }

    // Combine cleaned filepath
    for (int i = 0; i < argc; i++) {
        strcat(clean, "/");
        strcat(clean, argv[i]);
    }

    free(tofree);
    free(argv);

    return clean;
}
