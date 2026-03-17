#define lpatchlib_c
#define LUA_LIB

#include "lprefix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern char lundump_vmp_start[];
extern char lundump_vmp_end[];
extern char lvm_vmp_start[];
extern char lvm_vmp_end[];

static int patch_get_marker(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  if (strcmp(name, "lundump") == 0) {
    lua_pushlightuserdata(L, lundump_vmp_start);
    lua_pushinteger(L, lundump_vmp_end - lundump_vmp_start);
    return 2;
  } else if (strcmp(name, "lvm") == 0) {
    lua_pushlightuserdata(L, lvm_vmp_start);
    lua_pushinteger(L, lvm_vmp_end - lvm_vmp_start);
    return 2;
  }
  return luaL_error(L, "Unknown marker name: %s", name);
}

static int patch_write(lua_State *L) {
  void *address;
  size_t len;
  const char *bytes;
  long page_size;
  uintptr_t start_page;
  uintptr_t end_page;
  size_t protect_len;
#if defined(_WIN32)
  SYSTEM_INFO si;
  DWORD oldProtect;
#endif

  luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
  address = lua_touserdata(L, 1);
  bytes = luaL_checklstring(L, 2, &len);
  if (address == NULL || len == 0) { lua_pushboolean(L, 0); return 1; }

#if defined(_WIN32)
  GetSystemInfo(&si);
  page_size = si.dwPageSize;
#else
  page_size = sysconf(_SC_PAGESIZE);
#endif
  if (page_size <= 0) page_size = 4096;

  start_page = (uintptr_t)address & ~(page_size - 1);
  end_page   = ((uintptr_t)address + len + page_size - 1) & ~(page_size - 1);
  protect_len = end_page - start_page;

#if defined(_WIN32)
  if (!VirtualProtect((void*)start_page, protect_len, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    lua_pushboolean(L, 0); return 1;
  }
#else
  if (mprotect((void*)start_page, protect_len, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
    lua_pushboolean(L, 0); return 1;
  }
#endif

  memcpy(address, bytes, len);
  __builtin___clear_cache((char*)start_page, (char*)start_page + protect_len);

#if defined(_WIN32)
  VirtualProtect((void*)start_page, protect_len, oldProtect, &oldProtect);
#else
  mprotect((void*)start_page, protect_len, PROT_READ | PROT_EXEC);
#endif
  lua_pushboolean(L, 1);
  return 1;
}

static const luaL_Reg patchlib[] = {
  {"get_marker", patch_get_marker},
  {"write", patch_write},
  {NULL, NULL}
};

LUAMOD_API int luaopen_patch (lua_State *L) {
  luaL_newlib(L, patchlib);
  return 1;
}
