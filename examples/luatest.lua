--
-- Created by IntelliJ IDEA.
-- User: colin
-- Date: 2021-03-12
-- Time: 11:02 p.m.
-- To change this template use File | Settings | File Templates.
--

function myCB()
    ret_string = "<html><p>Hello, world! from URL: " .. bhlua_req_get_uri_path() .. "</p>"
    ret_string = ret_string .. "<p>" .. bhlua_req_get_uri_query() .. "</p>"
    ret_string = ret_string .. "<p>user-agent: " .. bhlua_req_get_header("user-agent") .. "</p>"
    ret_string = ret_string .. "<p>accept-encoding: " .. bhlua_req_get_header("accept-encoding") .. "</p>"
    ret_string = ret_string .. "<p>BODY: " .. bhlua_req_get_body() .. "</p>"
    ret_string = ret_string .. "</html>"
    bhlua_res_set_body_text(ret_string)
    bhlua_res_add_header("content-type", "text/html")
    return 0;
end
