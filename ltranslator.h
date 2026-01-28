/*
** Lua指令翻译器头文件
*/

#ifndef ltranslator_h
#define ltranslator_h

#include "lua.h"
#include "lobject.h"

/* 翻译器配置结构 */
typedef struct TranslatorConfig TranslatorConfig;

/* 主翻译函数 */
LUAI_FUNC int luaU_translate(lua_State *L, const Proto *f, FILE *out, TranslatorConfig *config);

/* 解析配置选项 */
LUAI_FUNC void luaU_parse_config(TranslatorConfig *config, const char *opt_string);

#endif
