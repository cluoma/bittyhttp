# bittyhttp

A simple threaded HTTP library and webserver that implements a subset of HTTP/1.1 with no external dependencies.

### Arguments

- __-p [PORT]__ Port number
- __-d [DOCROOT]__ Directory where files will be served from
- __-b [BACKLOG]__ Backlog for accept()
- __-l [LOGFILE]__ A path to a FILE
- __-a__ run as a daemon


Thanks to:    
[Beej's Guide to Network Programming](http://beej.us/guide/bgnet/output/print/bgnet_USLetter_2.pdf)    
[HTTP Parser](https://github.com/nodejs/http-parser)
