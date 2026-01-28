#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "lua.h"

/*
** JSON到Lua表的转换函数
** 参数:
**   L: Lua状态机
**   json: JSON字符串
**   len: JSON字符串长度
**   out: 输出缓冲区
**   outlen: 输出缓冲区长度
** 返回值:
**   1: 转换成功
**   0: 转换失败
*/
int json_to_lua(lua_State *L, const char *json, size_t len, char *out, size_t outlen);

#endif /* JSON_PARSER_H */
