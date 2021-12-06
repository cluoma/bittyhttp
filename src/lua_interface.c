//
// Created by colin on 2021-03-12.
//

#ifdef LUA

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lua_interface.h"

#define BHLUA_CONFIGURE_FUNCTIONS \
    BHLUAFUNC(bhlua_req_get_uri_path, req)      \
    BHLUAFUNC(bhlua_req_get_uri_query, req)     \
    BHLUAFUNC(bhlua_req_get_header, req)        \
    BHLUAFUNC(bhlua_req_get_body, req)          \
    BHLUAFUNC(bhlua_res_add_header, res)        \
    BHLUAFUNC(bhlua_res_get_header, res)        \
    BHLUAFUNC(bhlua_res_set_body_text, res)     \
    BHLUAFUNC(bhlua_res_set_body_file_rel, res) \
    BHLUAFUNC(bhlua_res_set_body_file_abs, res)
#define BHLUAFUNC(k, v)                         \
    lua_pushlightuserdata(L, (v));              \
    lua_pushcclosure(L, &(k), 1);               \
    lua_setglobal(L, #k);

/* bhttp_request functions */
static int bhlua_req_get_uri_path(lua_State *L)
{
    bhttp_request *req = (bhttp_request *)lua_topointer(L, lua_upvalueindex(1));
    if (!lua_checkstack(L, 1))
    {
        /* handle error ; no space on lua stack */
    }
    lua_pushlstring(L, bstr_cstring(&req->uri_path), bstr_size(&req->uri_path));
    return 1;
}
static int bhlua_req_get_uri_query(lua_State *L)
{
    bhttp_request *req = (bhttp_request *)lua_topointer(L, lua_upvalueindex(1));
    if (!lua_checkstack(L, 1))
    {
        /* handle error ; no space on lua stack */
    }
    lua_pushlstring(L, bstr_cstring(&req->uri_query), bstr_size(&req->uri_query));
    return 1;
}
static int bhlua_req_get_header(lua_State *L)
{
    bhttp_request *req = (bhttp_request *)lua_topointer(L, lua_upvalueindex(1));
    const char * field = luaL_checklstring(L, 1, NULL);
    const bhttp_header *header = bhttp_req_get_header(req, field);
    if (!lua_checkstack(L, 1))
    {
        /* handle error ; no space on lua stack */
        return 0;
    }
    if (header == NULL)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushlstring(L, bstr_cstring(&header->value), bstr_size(&header->value));
    }
    return 1;
}
static int bhlua_req_get_body(lua_State *L)
{
    bhttp_request *req = (bhttp_request *)lua_topointer(L, lua_upvalueindex(1));
    if (!lua_checkstack(L, 1))
    {
        /* handle error ; no space on lua stack */
    }
    lua_pushlstring(L, bstr_cstring(&req->body), bstr_size(&req->body));
    return 1;
}

/* bhttp_respond functions */
static int bhlua_res_get_header(lua_State *L)
{
    bhttp_response *res = (bhttp_response *)lua_topointer(L, lua_upvalueindex(1));
    const char * field = luaL_checklstring(L, 1, NULL);
    fprintf(stderr, "%s\n", field);
    const bhttp_header *header = bhttp_res_get_header(res, field);
    if (!lua_checkstack(L, 1))
    {
        /* handle error ; no space on lua stack */
    }
    if (header == NULL)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushlstring(L, bstr_cstring(&header->value), bstr_size(&header->value));
    }
    return 1;
}
static int
bhlua_res_add_header(lua_State *L)
{
    bhttp_response *res = (bhttp_response *)lua_topointer(L, lua_upvalueindex(1));
    size_t fieldlen, valuelen;
    const char * field = luaL_checklstring(L, 1, &fieldlen);
    const char * value = luaL_checklstring(L, 2, &valuelen);
    if (bhttp_res_add_header(res, field, value) != 0)
    {
        /* handle error ; couldn't add header to response */
    }
    return 0;
}
static int
bhlua_res_set_body_text(lua_State *L)
{
    bhttp_response *res = (bhttp_response *)lua_topointer(L, lua_upvalueindex(1));
    size_t textlen;
    const char * text = luaL_checklstring(L, 1, &textlen);
    if (bhttp_res_set_body_text(res, text) != 0)
    {
        /* handle error ; couldn't set body text */
    }
    return 0;
}
static int
bhlua_res_set_body_file_rel(lua_State *L)
{
    bhttp_response *res = (bhttp_response *)lua_topointer(L, lua_upvalueindex(1));
    size_t textlen;
    const char * text = luaL_checklstring(L, 1, &textlen);
    if (bhttp_res_set_body_file_rel(res, text) != 0)
    {
        /* handle error */
    }
    return 0;
}
static int
bhlua_res_set_body_file_abs(lua_State *L)
{
    bhttp_response *res = (bhttp_response *)lua_topointer(L, lua_upvalueindex(1));
    size_t textlen;
    const char * text = luaL_checklstring(L, 1, &textlen);
    if (bhttp_res_set_body_file_abs(res, text) != 0)
    {
        /* handle error */
    }
    return 0;
}

int
bhttp_lua_run_func(bhttp_request *req, bhttp_response *res,
                   const char * lua_file, const char * lua_cb,
                   void * data)
{
    lua_State *L = luaL_newstate();
    if (!L)
    {
        return -1;
    }
    luaL_openlibs(L);

    if (luaL_loadfile(L, lua_file) != LUA_OK)
    {
        fprintf(stderr, "Could not load lua file\n");
        goto end;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "Error in initial parse of lua file\n");
        goto end;
    }

    /* add bittyhttp functions to lua global state */
    BHLUA_CONFIGURE_FUNCTIONS;

    if (lua_getglobal(L, lua_cb) != LUA_TFUNCTION)
    {
        fprintf(stderr, "Supplied lua callback is not a function\n");
        goto end;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK)
    {
        fprintf(stderr, "Error running supplied lua callback\n");
        fprintf(stderr, "LUA: %s\n", lua_tostring(L, -1));
        goto end;
    }

end:
    lua_close(L);
    return 0;
}

int
bhttp_lua_handler_callback(bhttp_request *req, bhttp_response *res,
                           bstr *lua_file, bstr *lua_cb)
{
    return bhttp_lua_run_func(req, res,
                       bstr_cstring(lua_file), bstr_cstring(lua_cb),
                       NULL);
}

#endif