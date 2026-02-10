/*
** $Id: llex.c $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#define llex_c
#define LUA_CORE

#include "lprefix.h"


#include <locale.h>
#include <string.h>

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lzio.h"



#define next(ls) (ls->current = zgetc(ls->z))



#define currIsNewline(ls)	(ls->current == '\n' || ls->current == '\r')


/* ORDER RESERVED */
static const char *const luaX_tokens [] = {
    "and", "asm", "async", "await", "break", "case", "catch", "class", "command", "concept", "const", "continue",
    "default", "defer", "do", "else", "elseif", "end", "enum", "export",
    "extends", "false", "finally", "for", "function", "global", "goto", "if",
    "implements", "in", "interface", "is", "keyword", "lambda", "local", "namespace",
    "new", "nil", "not", "operator", "or", "repeat", "requires", "return", "struct",
    "super", "switch", "take", "then", "true", "try", "until", "when",
    "while", "with",
    "//", "..", "...", "==", ">=", "<=", "~=", "<<", ">>", "::",
    "<EOF>", "<number>", "<integer>", "<name>", "<string>", "<rawstring>", "<interpstring>",
    "|>", "<|", "|?>", "=>", "->", "??", "++",
    "+=", "-=", "*=", "/=", "//=", "%=", "&=", "|=", "~=", ">>=", "<<=", "..="
};


#define save_and_next(ls) (save(ls, ls->current), next(ls))


static void save (LexState *ls, int c) {
  Mbuffer *b = ls->buff;
  if (luaZ_bufflen(b) + 1 > luaZ_sizebuffer(b)) {
    size_t newsize;
    if (luaZ_sizebuffer(b) >= MAX_SIZE/2)
      luaX_syntaxerror(ls, "lexical element too long");
    newsize = luaZ_sizebuffer(b) * 2;
    luaZ_resizebuffer(ls->L, b, newsize);
  }
  b->buffer[luaZ_bufflen(b)++] = cast_char(c);
}


void luaX_init (lua_State *L) {
  int i;
  TString *e = luaS_newliteral(L, LUA_ENV);  /* create env name */
  luaC_fix(L, obj2gco(e));  /* never collect this name */
  for (i=0; i<NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, luaX_tokens[i]);
    luaC_fix(L, obj2gco(ts));  /* reserved words are never collected */
    ts->extra = cast_byte(i+1);  /* reserved word */
  }
}


const char *luaX_token2str (LexState *ls, int token) {
  if (token < FIRST_RESERVED) {  /* single-byte symbols */
    lua_assert(token == cast_uchar(token));
    return luaO_pushfstring(ls->L, "'%c'", token);
  }
  else {
    const char *s = luaX_tokens[token - FIRST_RESERVED];
    if (token < TK_EOS)  /* fixed format (symbols and reserved words)? */
      return luaO_pushfstring(ls->L, "'%s'", s);
    else  /* names, strings, and numerals */
      return s;
  }
}


static const char *txtToken (LexState *ls, int token) {
  switch (token) {
    case TK_NAME: case TK_STRING:
    case TK_FLT: case TK_INT:
      save(ls, '\0');
      return luaO_pushfstring(ls->L, "'%s'", luaZ_buffer(ls->buff));
    default:
      return luaX_token2str(ls, token);
  }
}


static l_noret lexerror (LexState *ls, const char *msg, int token) {
  msg = luaG_addinfo(ls->L, msg, ls->source, ls->linenumber);
  if (token)
    luaO_pushfstring(ls->L, "%s near %s", msg, txtToken(ls, token));
  luaD_throw(ls->L, LUA_ERRSYNTAX);
}


l_noret luaX_syntaxerror (LexState *ls, const char *msg) {
  lexerror(ls, msg, ls->t.token);
}


/*
** creates a new string and anchors it in scanner's table so that
** it will not be collected until the end of the compilation
** (by that time it should be anchored somewhere)
*/
TString *luaX_newstring (LexState *ls, const char *str, size_t l) {
  lua_State *L = ls->L;
  const TValue *o;  /* entry for 'str' */
  TString *ts = luaS_newlstr(L, str, l);  /* create new string */
  setsvalue2s(L, L->top.p++, ts);  /* temporarily anchor it in stack */
  o = luaH_get(ls->h, s2v(L->top.p - 1));
  if (ttisnil(o)) {  /* not in use yet? */
    /* boolean value does not need GC barrier;
       table is marked, so assignments to 'new' entries are checked */
    TValue val;
    setbtvalue(&val);
    luaH_set(L, ls->h, s2v(L->top.p - 1), &val);  /* t[string] = true */
    luaC_checkGC(L);
  }
  else {  /* string already present */
    ts = tsvalue(keyfromval(o));  /* re-use value saved in the table */
  }
  L->top.p--;  /* remove string from stack */
  return ts;
}


/*
** increment line number and skips newline sequence (any of
** \n, \r, \n\r, or \r\n)
*/
static void inclinenumber (LexState *ls) {
  int old = ls->current;
  lua_assert(currIsNewline(ls));
  next(ls);  /* skip '\n' or '\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip '\n\r' or '\r\n' */
  ls->linenumber++;
}


void luaX_setinput (lua_State *L, LexState *ls, ZIO *z, TString *source,
                    int firstchar) {
  ls->t.token = 0;
  ls->L = L;
  ls->current = firstchar;
  ls->lookahead.token = TK_EOS;  /* no look-ahead token */
  ls->lookahead2.token = TK_EOS;
  ls->z = z;
  ls->fs = NULL;
  ls->linenumber = 1;
  ls->lastline = 1;
  ls->source = source;
  ls->envn = luaS_newliteral(L, LUA_ENV);  /* get env name */
  ls->glbn = luaS_newliteral(L, "global"); /* get global name */
  luaZ_resizebuffer(ls->L, ls->buff, LUA_MINBUFFER);  /* initialize buffer */
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


static int check_next1 (LexState *ls, int c) {
  if (ls->current == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check for 2-char token pattern
*/
static int check_next2 (LexState *ls, const char *set) {
  lua_assert(set[2] == '\0');
  if (ls->current == set[0]) {
    next(ls);
    return 1;
  }
  return 0;
}


/*
** Check for string with "long" delimiters "==...=".
*/
static int read_long_string (LexState *ls, SemInfo *seminfo, int sep) {
  int line = ls->linenumber;  /* initial line (for error message) */
  int n = 0;  /* how many equal signs */
  save_and_next(ls);  /* skip 2nd '[' */
  if (currIsNewline(ls))  /* string starts with a newline? */
    inclinenumber(ls);  /* skip it */
  for (;;) {
    switch (ls->current) {
      case EOZ: {  /* error */
        const char *what = (sep == 0) ? "string" : "comment";
        const char *msg = luaO_pushfstring(ls->L,
                     "unfinished long %s (starting at line %d)", what, line);
        lexerror(ls, msg, TK_EOS);
        break;  /* to avoid warnings */
      }
      case ']': {
        if (n == sep) {  /* end of string? */
          save_and_next(ls);  /* skip 2nd ']' */
          goto endloop;
        }
        break;
      }
      case '\n': case '\r': {
        save(ls, '\n');
        inclinenumber(ls);
        if (!seminfo) luaZ_resetbuffer(ls->buff);  /* avoid wasting space */
        break;
      }
      default: {
        if (seminfo) save_and_next(ls);
        else next(ls);
      }
    }
    if (ls->current == '=') {
      n = 0;
      do {
        save_and_next(ls);
        n++;
      } while (ls->current == '=');
    }
    else {
      n = -1;  /* not an ending delimiter */
    }
  }
  endloop:
  if (seminfo) {
    seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + (2 + sep),
                                     luaZ_bufflen(ls->buff) - 2 * (2 + sep));
  }
  return TK_STRING;
}


static void esccheck (LexState *ls, int c, const char *msg) {
  if (!c) {
    if (ls->current != EOZ)
      save_and_next(ls);  /* add current to buffer for error message */
    lexerror(ls, msg, TK_STRING);
  }
}


static int gethexa (LexState *ls) {
  save_and_next(ls);
  esccheck (ls, lisxdigit(ls->current), "hexadecimal digit expected");
  return luaO_hexavalue(ls->current);
}


static int readhexaesc (LexState *ls) {
  int r = gethexa(ls);
  r = (r << 4) + gethexa(ls);
  luaZ_buffremove(ls->buff, 2);  /* remove saved chars from buffer */
  return r;
}


static unsigned long readutf8esc (LexState *ls) {
  unsigned long r;
  int i = 4;  /* chars to be removed: \u{ */
  save_and_next(ls);  /* skip 'u' */
  esccheck(ls, ls->current == '{', "missing '{'");
  r = gethexa(ls);  /* must have at least one digit */
  while ((save_and_next(ls), lisxdigit(ls->current))) {
    i++;
    r = (r << 4) + luaO_hexavalue(ls->current);
    esccheck(ls, r <= 0x10FFFF, "UTF-8 value too large");
  }
  esccheck(ls, ls->current == '}', "missing '}'");
  next(ls);  /* skip '}' */
  luaZ_buffremove(ls->buff, i);  /* remove saved chars from buffer */
  return r;
}


static void utf8esc (LexState *ls) {
  char buff[UTF8BUFFSZ];
  int n = luaO_utf8esc(buff, readutf8esc(ls));
  for (; n > 0; n--)  /* add 'buff' to buffer */
    save(ls, buff[UTF8BUFFSZ - n]);
}


static int readdecesc (LexState *ls) {
  int i;
  int r = 0;  /* result accumulator */
  for (i = 0; i < 3 && lisdigit(ls->current); i++) {  /* read up to 3 digits */
    r = 10 * r + ls->current - '0';
    save_and_next(ls);
  }
  esccheck(ls, r <= UCHAR_MAX, "decimal escape too large");
  luaZ_buffremove(ls->buff, i);  /* remove read digits from buffer */
  return r;
}


static void read_string (LexState *ls, int del, SemInfo *seminfo) {
  int is_interp = 0;
  if (del == '`') { /* interpolated string */
     is_interp = 1;
  }
  save_and_next(ls);  /* keep delimiter (for error messages) */
  while (ls->current != del) {
    switch (ls->current) {
      case EOZ:
        lexerror(ls, "unfinished string", TK_EOS);
        break;  /* to avoid warnings */
      case '\n':
      case '\r':
        lexerror(ls, "unfinished string", TK_STRING);
        break;  /* to avoid warnings */
      case '\\': {  /* escape sequences */
        int c;  /* final character to be saved */
        save_and_next(ls);  /* keep '\\' for error messages */
        switch (ls->current) {
          case 'a': c = '\a'; goto read_save;
          case 'b': c = '\b'; goto read_save;
          case 'f': c = '\f'; goto read_save;
          case 'n': c = '\n'; goto read_save;
          case 'r': c = '\r'; goto read_save;
          case 't': c = '\t'; goto read_save;
          case 'v': c = '\v'; goto read_save;
          case 'x': c = readhexaesc(ls); goto read_save;
          case 'u': utf8esc(ls); goto no_save;
          case '\n': case '\r':
            inclinenumber(ls); c = '\n'; goto only_save;
          case '\\': case '\"': case '\'':
            c = ls->current; goto read_save;
          case EOZ: goto no_save;  /* will raise an error next loop */
          case 'z': {  /* zap following span of spaces */
            luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
            next(ls);  /* skip 'z' */
            while (lisspace(ls->current)) {
              if (currIsNewline(ls)) inclinenumber(ls);
              else next(ls);
            }
            goto no_save;
          }
          default: {
            esccheck(ls, lisdigit(ls->current), "invalid escape sequence");
            c = readdecesc(ls);  /* digital escape '\ddd' */
            goto only_save;
          }
        }
       read_save:
         next(ls);
         /* go through */
       only_save:
         luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
         save(ls, c);
         /* go through */
       no_save: break;
      }
      default:
        save_and_next(ls);
    }
  }
  save_and_next(ls);  /* skip delimiter */
  seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + 1,
                                   luaZ_bufflen(ls->buff) - 2);
}


static int read_numeral (LexState *ls, SemInfo *seminfo) {
  TValue obj;
  const char *expo = "Ee";
  int first = ls->current;
  lua_assert(lisdigit(ls->current));
  save_and_next(ls);
  if (first == '0' && check_next2(ls, "xX"))  /* hexadecimal? */
    expo = "Pp";
  for (;;) {
    if (check_next2(ls, expo))  /* exponent part? */
      check_next2(ls, "-+");  /* optional exponent sign */
    if (lisxdigit(ls->current))
      save_and_next(ls);
    else if (ls->current == '.')
      save_and_next(ls);
    else break;
  }
  if (check_next2(ls, "nN"))  /* 'n' or 'N'? */
    save_and_next(ls);  /* (for 'LL') */
  save(ls, '\0');
  if (luaO_str2num(luaZ_buffer(ls->buff), &obj) == 0)  /* format error? */
    lexerror(ls, "malformed number", TK_FLT);
  if (ttisinteger(&obj)) {
    seminfo->i = ivalue(&obj);
    return TK_INT;
  }
  else {
    seminfo->r = fltvalue(&obj);
    return TK_FLT;
  }
}


static int llex (LexState *ls, SemInfo *seminfo) {
  luaZ_resetbuffer(ls->buff);
  for (;;) {
    switch (ls->current) {
      case '\n': case '\r': {  /* line breaks */
        inclinenumber(ls);
        break;
      }
      case ' ': case '\f': case '\t': case '\v': {  /* spaces */
        next(ls);
        break;
      }
      case '/': {  /* '/' or '//' (comment) */
        next(ls);
        if (ls->current == '/') {  /* '//' comment? */
          next(ls);  /* skip 2nd '/' */
          while (!currIsNewline(ls) && ls->current != EOZ)
            next(ls);  /* skip until end of line (or end of file) */
          break;
        } else if (ls->current == '*') { /* block comment? */
          next(ls); /* skip '*' */
          while (1) {
             if (ls->current == EOZ) break;
             if (ls->current == '*') {
                next(ls);
                if (ls->current == '/') {
                   next(ls);
                   break;
                }
             } else if (currIsNewline(ls)) {
                inclinenumber(ls);
             } else {
                next(ls);
             }
          }
          break;
        } else if (ls->current == '=') { /* /= */
           next(ls);
           return TK_DIVEQ;
        }
        else {
          return '/';
        }
      }
      case '-': {  /* '-' or '--' (comment) */
        next(ls);
        if (ls->current == '-') {
          next(ls);
          if (ls->current == '[') {
            int sep = -1;
            do {
              next(ls);
              sep++;
            } while (ls->current == '=');
            if (ls->current == '[') {
              read_long_string(ls, NULL, sep);  /* long comment */
              luaZ_resetbuffer(ls->buff);  /* free buffer */
              break;
            }
          }
          /* else short comment */
          while (!currIsNewline(ls) && ls->current != EOZ)
            next(ls);  /* skip until end of line (or end of file) */
          break;
        }
        else if (ls->current == '>') {
           next(ls); return TK_ARROW;
        } else if (ls->current == '=') {
           next(ls); return TK_SUBEQ;
        }
        return '-';
      }
      case '[': {  /* long string or simply '[' */
        int sep = -1;
        save_and_next(ls);
        if (ls->current == '=') {
          do {
            save_and_next(ls);
            sep++;
          } while (ls->current == '=');
          if (ls->current == '[') {
            read_long_string(ls, seminfo, sep);
            return TK_STRING;
          }
        }
        else if (ls->current == '[') {
          read_long_string(ls, seminfo, sep);
          return TK_STRING;
        }
        return '[';
      }
      case '=': {
        next(ls);
        if (ls->current == '=') { next(ls); return TK_EQ; }
        else if (ls->current == '>') { next(ls); return TK_MEAN; }
        else return '=';
      }
      case '<': {
        next(ls);
        if (ls->current == '=') {
           next(ls);
           if (ls->current == '>') {
              next(ls); return TK_SPACESHIP; /* <=> */
           }
           return TK_LE;
        }
        else if (ls->current == '<') {
           next(ls);
           if (ls->current == '=') {
              next(ls); return TK_SHLEQ; /* <<= */
           }
           return TK_SHL;
        }
        else if (ls->current == '|') { next(ls); return TK_REVPIPE; }
        else return '<';
      }
      case '>': {
        next(ls);
        if (ls->current == '=') { next(ls); return TK_GE; }
        else if (ls->current == '>') {
           next(ls);
           if (ls->current == '=') {
              next(ls); return TK_SHREQ; /* >>= */
           }
           return TK_SHR;
        }
        else return '>';
      }
      case '~': {
        next(ls);
        if (ls->current == '=') { next(ls); return TK_NE; }
        else return '~';
      }
      case ':': {
        next(ls);
        if (ls->current == ':') { next(ls); return TK_DBCOLON; }
        else return ':';
      }
      case '"': case '\'': {  /* short literal strings */
        read_string(ls, ls->current, seminfo);
        return TK_STRING;
      }
      case '`': { /* interpolated string */
        read_string(ls, ls->current, seminfo);
        return TK_INTERPSTRING;
      }
      case '.': {  /* '.', '..', '...', or number */
        save_and_next(ls);
        if (check_next1(ls, '.')) {
          if (check_next1(ls, '.'))
            return TK_DOTS;   /* '...' */
          else if (ls->current == '=') { /* ..= */
             next(ls); return TK_CONCATEQ;
          }
          else return TK_CONCAT;   /* '..' */
        }
        else if (!lisdigit(ls->current)) return '.';
        else return read_numeral(ls, seminfo);
      }
      case '+': {
         next(ls);
         if (ls->current == '+') { next(ls); return TK_PLUSPLUS; }
         else if (ls->current == '=') { next(ls); return TK_ADDEQ; }
         else return '+';
      }
      case '*': {
         next(ls);
         if (ls->current == '=') { next(ls); return TK_MULEQ; }
         else return '*';
      }
      case '%': {
         next(ls);
         if (ls->current == '=') { next(ls); return TK_MODEQ; }
         else return '%';
      }
      case '&': {
         next(ls);
         if (ls->current == '=') { next(ls); return TK_BANDEQ; }
         else return '&';
      }
      case '|': {
         next(ls);
         if (ls->current == '>') { next(ls); return TK_PIPE; }
         else if (ls->current == '?') {
            next(ls);
            if (ls->current == '>') { next(ls); return TK_SAFEPIPE; }
            else return '|';
         }
         else if (ls->current == '=') { next(ls); return TK_BOREQ; }
         else return '|';
      }
      case '^': {
         next(ls);
         if (ls->current == '=') { next(ls); return TK_BXOREQ; }
         else return '^';
      }
      case '?': {
         next(ls);
         if (ls->current == '?') { next(ls); return TK_NULLCOAL; }
         else if (ls->current == '.') { next(ls); return TK_OPTCHAIN; }
         else return '?';
      }
      case '$': {
         next(ls);
         if (ls->current == '$') { next(ls); return TK_DOLLDOLL; }
         else return TK_DOLLAR;
      }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9': {
        return read_numeral(ls, seminfo);
      }
      case EOZ: {
        return TK_EOS;
      }
      default: {
        if (lislalpha(ls->current)) {  /* identifier or reserved word? */
          TString *ts;
          do {
            save_and_next(ls);
          } while (lislalnum(ls->current));
          ts = luaX_newstring(ls, luaZ_buffer(ls->buff),
                                  luaZ_bufflen(ls->buff));
          seminfo->ts = ts;
          if (isreserved(ts))  /* reserved word? */
            return ts->extra - 1 + FIRST_RESERVED;
          else {
            return TK_NAME;
          }
        }
        else {  /* single-char tokens (+ - / ...) */
          int c = ls->current;
          next(ls);
          return c;
        }
      }
    }
  }
}


void luaX_next (LexState *ls) {
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
    if (ls->lookahead2.token != TK_EOS) {
       ls->lookahead = ls->lookahead2;
       ls->lookahead2.token = TK_EOS;
    }
  }
  else
    ls->t.token = llex(ls, &ls->t.seminfo);  /* read next token */
}


int luaX_lookahead (LexState *ls) {
  lua_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = llex(ls, &ls->lookahead.seminfo);
  return ls->lookahead.token;
}

int luaX_lookahead2 (LexState *ls) {
  lua_assert(ls->lookahead2.token == TK_EOS);
  if (ls->lookahead.token == TK_EOS) {
     ls->lookahead.token = llex(ls, &ls->lookahead.seminfo);
  }
  ls->lookahead2.token = llex(ls, &ls->lookahead2.seminfo);
  return ls->lookahead2.token;
}


void luaX_warning (LexState *ls, const char *msg, WarningType wt) {
  if (ls->warnings.states[wt] == WS_OFF) return;
  if (ls->warnings.states[wt] == WS_ERROR) {
    luaX_syntaxerror(ls, msg);
  }
  lua_warning(ls->L, msg, 0);
}

void luaX_pushincludefile(LexState *ls, const char *filename) {
  /* simplified stub: include files not supported in this fix context */
  luaX_warning(ls, "include file not supported", WT_ALL);
}

void luaX_addalias(LexState *ls, TString *name, Token *tokens, int n) {
  /* simplified stub: aliases not supported in this fix context */
  (void)ls; (void)name; (void)tokens; (void)n;
}
