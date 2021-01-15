TODO

server:
- logging + arg
- daemon + arg

Request:
- Parse after every buf read
- Make it look nicer

Respond:
- Basically full rewrite required

CGI:
- proper env variable setting, implementation depends on respon rewrite
- fork -> execl -> waitpid
