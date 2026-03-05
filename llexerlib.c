/*
** $Id: llexerlib.c $
** Lexer library for LXCLUA
** See Copyright Notice in lua.h
*/

#define llexerlib_c
#define LUA_LIB

#include "lprefix.h"

#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "llex.h"
#include "lzio.h"
#include "lstring.h"
#include "lstate.h"
#include "ltable.h"

/*
** Reader function for luaZ_init
*/
typedef struct {
  const char *s;
  size_t size;
} LoadS;

static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}

/*
** Lexer state structure to pass to pcall
*/
typedef struct {
  ZIO *z;
  Mbuffer *buff;
  TString *source;
} PCallLexState;

static int protected_lex(lua_State *L) {
    PCallLexState *pls = (PCallLexState *)lua_touserdata(L, 1);
    LexState lexstate;
    memset(&lexstate, 0, sizeof(LexState));

    lexstate.buff = pls->buff;

    /* create table for scanner to avoid collecting/reusing strings */
    lua_newtable(L);
    lexstate.h = hvalue(s2v(L->top.p - 1));

    int firstchar = zgetc(pls->z);

    /* Call the lexer initialization */
    luaX_setinput(L, &lexstate, pls->z, pls->source, firstchar);

    lua_newtable(L); /* Result array table */
    int i = 1;

    int array_idx = lua_gettop(L);
    while (1) {
        luaX_next(&lexstate);
        int token = lexstate.t.token;
        if (token == TK_EOS) {
            break;
        }

        lua_newtable(L); /* Sub-table for this token */
        int token_tbl_idx = lua_gettop(L);

        /* token integer */
        lua_pushinteger(L, token);
        lua_setfield(L, token_tbl_idx, "token");

        /* line number */
        lua_pushinteger(L, lexstate.linenumber);
        lua_setfield(L, token_tbl_idx, "line");

        /* token string representation */
        const char *tok_str = luaX_token2str(&lexstate, token);
        if (tok_str) {
            lua_pushstring(L, tok_str);
            lua_setfield(L, token_tbl_idx, "type");
        }
        /* luaX_token2str might push string onto the stack via luaO_pushfstring */
        lua_settop(L, token_tbl_idx);

        /* semantic info based on token */
        if (token == TK_NAME || token == TK_STRING || token == TK_INTERPSTRING || token == TK_RAWSTRING) {
            if (lexstate.t.seminfo.ts) {
                lua_pushstring(L, getstr(lexstate.t.seminfo.ts));
                lua_setfield(L, token_tbl_idx, "value");
            }
        } else if (token == TK_INT) {
            lua_pushinteger(L, lexstate.t.seminfo.i);
            lua_setfield(L, token_tbl_idx, "value");
        } else if (token == TK_FLT) {
            lua_pushnumber(L, lexstate.t.seminfo.r);
            lua_setfield(L, token_tbl_idx, "value");
        }

        /* Add token to the result array */
        lua_rawseti(L, array_idx, i++);
    }

    return 1; /* Return the array table */
}

static int lexer_lex(lua_State *L) {
    size_t l;
    const char *s = luaL_checklstring(L, 1, &l);

    LoadS ls;
    ls.s = s;
    ls.size = l;

    ZIO z;
    luaZ_init(L, &z, getS, &ls);

    /* Anchor source string to avoid GC */
    lua_pushstring(L, "=(lexer)");
    TString *source = tsvalue(s2v(L->top.p - 1));

    /* Initialize Mbuffer before luaX_setinput */
    Mbuffer buff;
    luaZ_initbuffer(L, &buff);

    PCallLexState pls;
    pls.z = &z;
    pls.buff = &buff;
    pls.source = source;

    /* Push light userdata and C function to call in protected mode */
    lua_pushcfunction(L, protected_lex);
    lua_pushlightuserdata(L, &pls);

    int status = lua_pcall(L, 1, 1, 0);

    /* Clean up buffers whether it failed or succeeded */
    luaZ_freebuffer(L, &buff);

    if (status != LUA_OK) {
        /* Error message is at the top of the stack */
        return lua_error(L);
    }

    /* Move the result table below the anchored source string, then pop the source string */
    lua_insert(L, -2);
    lua_pop(L, 1);

    return 1;
}

static int lexer_token2str(lua_State *L) {
    int token = luaL_checkinteger(L, 1);

    if (token > TK_RAWSTRING || token < 0) {
        lua_pushnil(L);
        return 1;
    }

    if (token < FIRST_RESERVED) {
        /* single-byte symbols */
        char s[2] = {(char)token, '\0'};
        lua_pushstring(L, s);
    } else {
        /* reserved words and other tokens */
        /* We use luaX_token2str with a dummy LexState to get the string representation.
           Some tokens cause luaX_token2str to push the string onto the stack,
           while others just return a const char*. We handle both cases using lua_gettop. */
        LexState dummy_ls;
        memset(&dummy_ls, 0, sizeof(LexState));
        dummy_ls.L = L;

        int top = lua_gettop(L);
        const char *str = luaX_token2str(&dummy_ls, token);
        if (str) {
            if (lua_gettop(L) == top) {
                /* luaX_token2str returned a string but didn't push it */
                lua_pushstring(L, str);
            }
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    }
    return 1;
}

static const luaL_Reg lexer_lib[] = {
    {"lex", lexer_lex},
    {"token2str", lexer_token2str},
    {NULL, NULL}
};

static const char *lexer_lua_ext =
"local lexer = ...\n"
"local function unquote(s)\n"
"    return s:match('^\\'(.*)\\'$') or s\n"
"end\n"
"function lexer.to_code(tokens)\n"
"    local out = {}\n"
"    local prev_is_alnum = false\n"
"    for _, t in ipairs(tokens) do\n"
"        local str = ''\n"
"        local is_alnum = false\n"
"        if t.type == '<name>' then\n"
"            str = t.value\n"
"            is_alnum = true\n"
"        elseif t.type == '<string>' or t.type == '<interpstring>' then\n"
"            str = string.format('%q', t.value)\n"
"        elseif t.type == '<rawstring>' then\n"
"            str = '_raw' .. string.format('%q', t.value)\n"
"        elseif t.type == '<integer>' or t.type == '<number>' then\n"
"            str = tostring(t.value)\n"
"            is_alnum = true\n"
"        else\n"
"            str = unquote(t.type)\n"
"            if str:match('^[a-zA-Z_][a-zA-Z0-9_]*$') then\n"
"                is_alnum = true\n"
"            end\n"
"        end\n"
"        if prev_is_alnum and is_alnum then\n"
"            table.insert(out, ' ')\n"
"        end\n"
"        table.insert(out, str)\n"
"        prev_is_alnum = is_alnum\n"
"    end\n"
"    return table.concat(out)\n"
"end\n"
"local end_openers = {\n"
"    [\"'do'\"] = true, [\"'if'\"] = true, [\"'while'\"] = true, [\"'for'\"] = true,\n"
"    [\"'function'\"] = true, [\"'class'\"] = true, [\"'try'\"] = true,\n"
"    [\"'switch'\"] = true, [\"'enum'\"] = true, [\"'interface'\"] = true,\n"
"    [\"'namespace'\"] = true\n"
"}\n"
"function lexer.find_match(tokens, start_pos)\n"
"    local t_start = tokens[start_pos]\n"
"    if not t_start then return nil end\n"
"    local target_closer\n"
"    local is_end_group = false\n"
"    if end_openers[t_start.type] then\n"
"        target_closer = \"'end'\"\n"
"        is_end_group = true\n"
"    elseif t_start.type == \"'repeat'\" then target_closer = \"'until'\"\n"
"    elseif t_start.type == \"'('\" then target_closer = \"')'\"\n"
"    elseif t_start.type == \"'['\" then target_closer = \"']'\"\n"
"    elseif t_start.type == \"'{'\" then target_closer = \"'}'\"\n"
"    else return nil end\n"
"    local stack = 1\n"
"    for i = start_pos + 1, #tokens do\n"
"        local t = tokens[i]\n"
"        if is_end_group then\n"
"            if end_openers[t.type] then stack = stack + 1\n"
"            elseif t.type == target_closer then stack = stack - 1 end\n"
"        else\n"
"            if t.type == t_start.type then stack = stack + 1\n"
"            elseif t.type == target_closer then stack = stack - 1 end\n"
"        end\n"
"        if stack == 0 then return i end\n"
"    end\n"
"    return nil\n"
"end\n"
"function lexer.build_tree(tokens)\n"
"    local root = { type = 'Root', children = {} }\n"
"    local stack = { root }\n"
"    for i, t in ipairs(tokens) do\n"
"        if end_openers[t.type] or t.type == \"'repeat'\" or t.type == \"'('\" or t.type == \"'['\" or t.type == \"'{'\" then\n"
"            local node = { type = 'Block', opener = t, children = {} }\n"
"            table.insert(stack[#stack].children, node)\n"
"            table.insert(stack, node)\n"
"        elseif t.type == \"'end'\" or t.type == \"'until'\" or t.type == \"')'\" or t.type == \"']'\" or t.type == \"'}'\" then\n"
"            local current = stack[#stack]\n"
"            if current.type == 'Block' then\n"
"                current.closer = t\n"
"                table.remove(stack)\n"
"            else\n"
"                table.insert(current.children, t)\n"
"            end\n"
"        else\n"
"            table.insert(stack[#stack].children, t)\n"
"        end\n"
"    end\n"
"    return root\n"
"end\n"
"function lexer.flatten_tree(node, out)\n"
"    out = out or {}\n"
"    if node.opener then table.insert(out, node.opener) end\n"
"    for _, child in ipairs(node.children or {}) do\n"
"        if child.type == 'Block' then\n"
"            lexer.flatten_tree(child, out)\n"
"        else\n"
"            table.insert(out, child)\n"
"        end\n"
"    end\n"
"    if node.closer then table.insert(out, node.closer) end\n"
"    return out\n"
"end\n";

#define REG_TK(L, tk) \
  lua_pushinteger(L, tk); \
  lua_setfield(L, -2, #tk)

LUAMOD_API int luaopen_lexer(lua_State *L) {
    luaL_newlib(L, lexer_lib);

    /* Register token constants */
    REG_TK(L, TK_ADDEQ);
    REG_TK(L, TK_AND);
    REG_TK(L, TK_ARROW);
    REG_TK(L, TK_ASM);
    REG_TK(L, TK_ASYNC);
    REG_TK(L, TK_AWAIT);
    REG_TK(L, TK_BANDEQ);
    REG_TK(L, TK_BOOL);
    REG_TK(L, TK_BOREQ);
    REG_TK(L, TK_BREAK);
    REG_TK(L, TK_BXOREQ);
    REG_TK(L, TK_CASE);
    REG_TK(L, TK_CATCH);
    REG_TK(L, TK_CHAR);
    REG_TK(L, TK_COMMAND);
    REG_TK(L, TK_CONCAT);
    REG_TK(L, TK_CONCATEQ);
    REG_TK(L, TK_CONCEPT);
    REG_TK(L, TK_CONST);
    REG_TK(L, TK_CONTINUE);
    REG_TK(L, TK_DBCOLON);
    REG_TK(L, TK_DEFAULT);
    REG_TK(L, TK_DEFER);
    REG_TK(L, TK_DIVEQ);
    REG_TK(L, TK_DO);
    REG_TK(L, TK_DOLLAR);
    REG_TK(L, TK_DOLLDOLL);
    REG_TK(L, TK_DOTS);
    REG_TK(L, TK_DOUBLE);
    REG_TK(L, TK_ELSE);
    REG_TK(L, TK_ELSEIF);
    REG_TK(L, TK_END);
    REG_TK(L, TK_ENUM);
    REG_TK(L, TK_EOS);
    REG_TK(L, TK_EQ);
    REG_TK(L, TK_EXPORT);
    REG_TK(L, TK_FALSE);
    REG_TK(L, TK_FINALLY);
    REG_TK(L, TK_FLT);
    REG_TK(L, TK_FOR);
    REG_TK(L, TK_FUNCTION);
    REG_TK(L, TK_GE);
    REG_TK(L, TK_GLOBAL);
    REG_TK(L, TK_GOTO);
    REG_TK(L, TK_IDIV);
    REG_TK(L, TK_IDIVEQ);
    REG_TK(L, TK_IF);
    REG_TK(L, TK_IN);
    REG_TK(L, TK_INT);
    REG_TK(L, TK_INTERPSTRING);
    REG_TK(L, TK_IS);
    REG_TK(L, TK_KEYWORD);
    REG_TK(L, TK_LAMBDA);
    REG_TK(L, TK_LE);
    REG_TK(L, TK_LET);
    REG_TK(L, TK_LOCAL);
    REG_TK(L, TK_LONG);
    REG_TK(L, TK_MEAN);
    REG_TK(L, TK_MODEQ);
    REG_TK(L, TK_MULEQ);
    REG_TK(L, TK_NAME);
    REG_TK(L, TK_NAMESPACE);
    REG_TK(L, TK_NE);
    REG_TK(L, TK_NIL);
    REG_TK(L, TK_NOT);
    REG_TK(L, TK_NULLCOAL);
    REG_TK(L, TK_OPERATOR);
    REG_TK(L, TK_OPTCHAIN);
    REG_TK(L, TK_OR);
    REG_TK(L, TK_PIPE);
    REG_TK(L, TK_PLUSPLUS);
    REG_TK(L, TK_RAWSTRING);
    REG_TK(L, TK_REPEAT);
    REG_TK(L, TK_REQUIRES);
    REG_TK(L, TK_RETURN);
    REG_TK(L, TK_REVPIPE);
    REG_TK(L, TK_SAFEPIPE);
    REG_TK(L, TK_SHL);
    REG_TK(L, TK_SHLEQ);
    REG_TK(L, TK_SHR);
    REG_TK(L, TK_SHREQ);
    REG_TK(L, TK_SPACESHIP);
    REG_TK(L, TK_STRING);
    REG_TK(L, TK_STRUCT);
    REG_TK(L, TK_SUBEQ);
    REG_TK(L, TK_SUPERSTRUCT);
    REG_TK(L, TK_SWITCH);
    REG_TK(L, TK_TAKE);
    REG_TK(L, TK_THEN);
    REG_TK(L, TK_TRUE);
    REG_TK(L, TK_TRY);
    REG_TK(L, TK_TYPE_FLOAT);
    REG_TK(L, TK_TYPE_INT);
    REG_TK(L, TK_UNTIL);
    REG_TK(L, TK_USING);
    REG_TK(L, TK_VOID);
    REG_TK(L, TK_WALRUS);
    REG_TK(L, TK_WHEN);
    REG_TK(L, TK_WHILE);
    REG_TK(L, TK_WITH);

    /* Load and execute Lua extension */
    if (luaL_loadstring(L, lexer_lua_ext) == LUA_OK) {
        lua_pushvalue(L, -2); /* push the lexer module table */
        lua_call(L, 1, 0);    /* call the extension script */
    } else {
        /* If there's a syntax error in the extension, raise it */
        return lua_error(L);
    }

    return 1;
}
