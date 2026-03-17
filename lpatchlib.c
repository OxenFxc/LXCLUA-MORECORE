#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
#include <psapi.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

extern char lundump_vmp_start[];
extern char lundump_vmp_end[];
extern char lvm_vmp_start[];
extern char lvm_vmp_end[];
extern char ldump_vmp_start[];
extern char ldump_vmp_end[];

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
  } else if (strcmp(name, "ldump") == 0) {
    lua_pushlightuserdata(L, ldump_vmp_start);
    lua_pushinteger(L, ldump_vmp_end - ldump_vmp_start);
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

static int patch_get_symbol(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  void *address = NULL;

#if defined(_WIN32)
  HMODULE hMods[1024];
  DWORD cbNeeded;
  if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded)) {
    for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
      address = (void*)GetProcAddress(hMods[i], name);
      if (address) break;
    }
  }
#else
  address = dlsym(RTLD_DEFAULT, name);
#endif

  if (address == NULL) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushlightuserdata(L, address);
  return 1;
}

static int patch_read(lua_State *L) {
  void *address;
  size_t size;

  luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
  address = lua_touserdata(L, 1);
  size = (size_t)luaL_checkinteger(L, 2);

  if (address == NULL || size == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushlstring(L, (const char *)address, size);
  return 1;
}

static int patch_alloc(lua_State *L) {
  size_t size = (size_t)luaL_checkinteger(L, 1);
  void *address = NULL;

  if (size == 0) {
    lua_pushnil(L);
    return 1;
  }

#if defined(_WIN32)
  address = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
  address = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (address == MAP_FAILED) {
    address = NULL;
  }
#endif

  if (address == NULL) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushlightuserdata(L, address);
  return 1;
}

static int patch_free(lua_State *L) {
  void *address;
  size_t size;
  int result = 0;

  luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
  address = lua_touserdata(L, 1);
  size = (size_t)luaL_checkinteger(L, 2);

  if (address == NULL || size == 0) {
    lua_pushboolean(L, 0);
    return 1;
  }

#if defined(_WIN32)
  result = VirtualFree(address, 0, MEM_RELEASE) != 0;
#else
  result = munmap(address, size) == 0;
#endif

  lua_pushboolean(L, result);
  return 1;
}

static int patch_mprotect(lua_State *L) {
  void *address;
  size_t size;
  const char *flags_str;
  long page_size;
  uintptr_t start_page;
  uintptr_t end_page;
  size_t protect_len;
  int result = 0;

  luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
  address = lua_touserdata(L, 1);
  size = (size_t)luaL_checkinteger(L, 2);
  flags_str = luaL_checkstring(L, 3);

  if (address == NULL || size == 0) {
    lua_pushboolean(L, 0);
    return 1;
  }

#if defined(_WIN32)
  SYSTEM_INFO si;
  DWORD oldProtect;
  DWORD newProtect = PAGE_NOACCESS;

  GetSystemInfo(&si);
  page_size = si.dwPageSize;

  if (strchr(flags_str, 'r') && strchr(flags_str, 'w') && strchr(flags_str, 'x')) {
    newProtect = PAGE_EXECUTE_READWRITE;
  } else if (strchr(flags_str, 'r') && strchr(flags_str, 'x')) {
    newProtect = PAGE_EXECUTE_READ;
  } else if (strchr(flags_str, 'r') && strchr(flags_str, 'w')) {
    newProtect = PAGE_READWRITE;
  } else if (strchr(flags_str, 'r')) {
    newProtect = PAGE_READONLY;
  } else if (strchr(flags_str, 'x')) {
    newProtect = PAGE_EXECUTE;
  }
#else
  int newProtect = PROT_NONE;
  page_size = sysconf(_SC_PAGESIZE);

  if (strchr(flags_str, 'r')) newProtect |= PROT_READ;
  if (strchr(flags_str, 'w')) newProtect |= PROT_WRITE;
  if (strchr(flags_str, 'x')) newProtect |= PROT_EXEC;
#endif

  if (page_size <= 0) page_size = 4096;

  start_page = (uintptr_t)address & ~(page_size - 1);
  end_page   = ((uintptr_t)address + size + page_size - 1) & ~(page_size - 1);
  protect_len = end_page - start_page;

#if defined(_WIN32)
  result = VirtualProtect((void*)start_page, protect_len, newProtect, &oldProtect) != 0;
#else
  result = mprotect((void*)start_page, protect_len, newProtect) == 0;
#endif

  lua_pushboolean(L, result);
  return 1;
}

static int patch_call(lua_State *L) {
  void *address;
  luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
  address = lua_touserdata(L, 1);

  if (address == NULL) {
    return luaL_error(L, "Cannot call NULL pointer");
  }

  // Cast address to a void function pointer that takes no arguments
  void (*func)(void) = (void (*)(void))address;
  func();

  return 0;
}

static const luaL_Reg patchlib[] = {
  {"get_marker", patch_get_marker},
  {"get_symbol", patch_get_symbol},
  {"write", patch_write},
  {"read", patch_read},
  {"alloc", patch_alloc},
  {"free", patch_free},
  {"mprotect", patch_mprotect},
  {"call", patch_call},
  {NULL, NULL}
};

LUAMOD_API int luaopen_patch (lua_State *L) {
  luaL_newlib(L, patchlib);
  return 1;
}
