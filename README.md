![Alt text](https://travis-ci.org/cluoma/MiniHTTP.svg?branch=master  "Master Branch")

# MiniHTTP

A simple webserver that implements a subset of HTTP/1.1. No external libraries are required to compile MiniHTTP and it compiles without warnings with '-Wall' on Raspbian Jessie. This is not to say there are no bugs (far from it). MiniHTTP is an educational project with lots of improvements to be made.

Processes are forked to handle GET or POST requests. Basic CGI support is implemented for GET and POST although only CONTENT_LENGTH and QUERY_STRING environment variables are set as well as sending POST data to stdin. Any *.cgi file is assumed to be executable and MiniHTTP will attempt to fork a process and execute it. In this way php and python CGI scripts are supported as long as their necessary libraries are installed and the script is named accordingly.

This software is free as in free as in do whatever you want with it. The exception of course is http_parser.c and http_parser.h as they are not my works. Find a link to that project below.

### Arguments

__-p [PORT]__ Port number    
__-d [DOCROOT]__ Directory where files will be served from    
__-b [BACKLOG]__ Backlog for accept()    
__-l [LOGFILE]__ A path to a FILE that minihttp can write logs to, file will be created if none exists. Logs are of the form "\<method\>,\<requested file\>,\<GMT time\>"    
__-a__ run as a daemon     


Thanks to:    
[Beej's Guide to Network Programming](http://beej.us/guide/bgnet/output/print/bgnet_USLetter_2.pdf)    
[HTTP Parser](https://github.com/nodejs/http-parser)
