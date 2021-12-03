//
// Created by colin on 2021-03-12.
//

#ifndef BITTYHTTP_LUA_INTERFACE_H
#define BITTYHTTP_LUA_INTERFACE_H

#include "request.h"
#include "respond.h"

///* holds a LUA state and some additional data for bhttp */
//typedef struct bhttp_lua_vm
//{
//    lua_State *L;
//} bhttp_lua_vm;

int bhttp_lua_handler_callback(bhttp_request *req, bhttp_response *res,
                               bstr *lua_file, bstr *lua_cb);
int bhttp_lua_run_func(bhttp_request *req, bhttp_response *res,
                       const char * lua_file, const char * lua_cb,
                       void * data);

#endif //BITTYHTTP_LUA_INTERFACE_H
