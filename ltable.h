#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/* 'const' to avoid wrong writings that can mess up field 'next' */
#define gkey(n)		cast(const TValue*, (&(n)->u.key_val))

/*
** writable version of 'gkey'; allows updates to individual fields,
** but not to the whole (which includes the 'next' field)
*/
#define wgkey(n)		(&(n)->u.key_val)

#define invalidateTMcache(t)	((t)->flags = 0)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the key, given the value of a table entry */
#define keyfromval(v) \
  (gkey(cast(Node *, cast(char *, (v)) - offsetof(Node, i_val))))

/* Macros for internal table handling */
#define getArrTag(t,i) (&((t)->array[i].tt_))
#define lenhint(t) (&((t)->alimit))


LUAI_FUNC const TValue *luaH_getint (Table *t, lua_Integer key);
LUAI_FUNC void luaH_setint (lua_State *L, Table *t, lua_Integer key,
                                                    TValue *value);
LUAI_FUNC const TValue *luaH_getshortstr (Table *t, TString *key);
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key);
LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key);
/* luaH_newkey is internal static in ltable.c */
/* LUAI_FUNC TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key); */
LUAI_FUNC void luaH_set (lua_State *L, Table *t, const TValue *key,
                                                    TValue *value);
LUAI_FUNC void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
LUAI_FUNC Table *luaH_new (lua_State *L);
LUAI_FUNC void luaH_resize (lua_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, unsigned int nasize);
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC lua_Unsigned luaH_getn (Table *t);
LUAI_FUNC unsigned int luaH_realasize (const Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
LUAI_FUNC int luaH_isdummy (const Table *t);
#endif

/* Custom Access Logging / Filtering API */
LUAI_FUNC void luaH_set_intelligent_mode(int enabled);
LUAI_FUNC int luaH_is_intelligent_mode_enabled(void);
LUAI_FUNC void luaH_set_filter_jnienv(int enabled);
LUAI_FUNC int luaH_is_filter_jnienv_enabled(void);
LUAI_FUNC void luaH_set_filter_userdata(int enabled);
LUAI_FUNC int luaH_is_filter_userdata_enabled(void);
LUAI_FUNC void luaH_clear_access_filters(void);
LUAI_FUNC void luaH_set_dedup_enabled(int enabled);
LUAI_FUNC void luaH_set_show_unique_only(int enabled);
LUAI_FUNC void luaH_reset_dedup_cache(void);
LUAI_FUNC int luaH_add_include_key_type_filter(const char *type);
LUAI_FUNC int luaH_add_exclude_key_type_filter(const char *type);
LUAI_FUNC int luaH_add_include_value_type_filter(const char *type);
LUAI_FUNC int luaH_add_exclude_value_type_filter(const char *type);
LUAI_FUNC void luaH_set_access_filter_enabled(int enabled);
LUAI_FUNC int luaH_add_include_key_filter(const char *pattern);
LUAI_FUNC int luaH_add_exclude_key_filter(const char *pattern);
LUAI_FUNC int luaH_add_include_value_filter(const char *pattern);
LUAI_FUNC int luaH_add_exclude_value_filter(const char *pattern);
LUAI_FUNC int luaH_add_include_op_filter(const char *pattern);
LUAI_FUNC int luaH_add_exclude_op_filter(const char *pattern);
LUAI_FUNC void luaH_set_key_int_range(int min_val, int max_val);
LUAI_FUNC void luaH_set_value_int_range(int min_val, int max_val);
LUAI_FUNC int luaH_enable_access_log (lua_State *L, int enable);
LUAI_FUNC const char *luaH_get_log_path (lua_State *L);

#endif
