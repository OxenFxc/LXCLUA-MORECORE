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
extern char lapi_vmp_start[];
extern char lapi_vmp_end[];
extern char llex_vmp_start[];
extern char llex_vmp_end[];
extern char lparser_vmp_start[];
extern char lparser_vmp_end[];
extern char lcode_vmp_start[];
extern char lcode_vmp_end[];
extern char ldo_vmp_start[];
extern char ldo_vmp_end[];
extern char lgc_vmp_start[];
extern char lgc_vmp_end[];

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
  } else if (strcmp(name, "lapi") == 0) {
    lua_pushlightuserdata(L, lapi_vmp_start);
    lua_pushinteger(L, lapi_vmp_end - lapi_vmp_start);
    return 2;
  } else if (strcmp(name, "llex") == 0) {
    lua_pushlightuserdata(L, llex_vmp_start);
    lua_pushinteger(L, llex_vmp_end - llex_vmp_start);
    return 2;
  } else if (strcmp(name, "lparser") == 0) {
    lua_pushlightuserdata(L, lparser_vmp_start);
    lua_pushinteger(L, lparser_vmp_end - lparser_vmp_start);
    return 2;
  } else if (strcmp(name, "lcode") == 0) {
    lua_pushlightuserdata(L, lcode_vmp_start);
    lua_pushinteger(L, lcode_vmp_end - lcode_vmp_start);
    return 2;
  } else if (strcmp(name, "ldo") == 0) {
    lua_pushlightuserdata(L, ldo_vmp_start);
    lua_pushinteger(L, ldo_vmp_end - ldo_vmp_start);
    return 2;
  } else if (strcmp(name, "lgc") == 0) {
    lua_pushlightuserdata(L, lgc_vmp_start);
    lua_pushinteger(L, lgc_vmp_end - lgc_vmp_start);
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

  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
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

  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
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

  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
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

  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
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

static int patch_to_num(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uintptr_t ptr = (uintptr_t)lua_topointer(L, 1);
  lua_pushinteger(L, (lua_Integer)ptr);
  return 1;
}

static int patch_to_ptr(lua_State *L) {
  uintptr_t ptr = (uintptr_t)luaL_checkinteger(L, 1);
  lua_pushlightuserdata(L, (void *)ptr);
  return 1;
}

static int patch_read_u8(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint8_t *ptr = (uint8_t *)lua_topointer(L, 1);
  lua_pushinteger(L, (lua_Integer)(*ptr));
  return 1;
}

static int patch_read_u32(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint32_t *ptr = (uint32_t *)lua_topointer(L, 1);
  lua_pushinteger(L, (lua_Integer)(*ptr));
  return 1;
}

static int patch_read_u64(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint64_t *ptr = (uint64_t *)lua_topointer(L, 1);
  lua_pushinteger(L, (lua_Integer)(*ptr));
  return 1;
}

static int patch_write_u8(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint8_t *ptr = (uint8_t *)lua_topointer(L, 1);
  uint8_t val = (uint8_t)luaL_checkinteger(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_write_u32(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint32_t *ptr = (uint32_t *)lua_topointer(L, 1);
  uint32_t val = (uint32_t)luaL_checkinteger(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_write_u64(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint64_t *ptr = (uint64_t *)lua_topointer(L, 1);
  uint64_t val = (uint64_t)luaL_checkinteger(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_memcpy(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid dest pointer");
  luaL_argcheck(L, lua_topointer(L, 2) != NULL, 2, "invalid src pointer");
  void *dst = (void *)lua_topointer(L, 1);
  void *src = (void *)lua_topointer(L, 2);
  size_t size = (size_t)luaL_checkinteger(L, 3);
  memcpy(dst, src, size);
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_memcmp(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid ptr1");
  luaL_argcheck(L, lua_topointer(L, 2) != NULL, 2, "invalid ptr2");
  void *ptr1 = (void *)lua_topointer(L, 1);
  void *ptr2 = (void *)lua_topointer(L, 2);
  size_t size = (size_t)luaL_checkinteger(L, 3);
  int res = memcmp(ptr1, ptr2, size);
  lua_pushinteger(L, res);
  return 1;
}

static int patch_memset(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid dest pointer");
  void *dst = (void *)lua_topointer(L, 1);
  int c = (int)luaL_checkinteger(L, 2);
  size_t size = (size_t)luaL_checkinteger(L, 3);
  memset(dst, c, size);
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_write_cstring(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  void *dst = (void *)lua_topointer(L, 1);
  size_t len;
  const char *str = luaL_checklstring(L, 2, &len);
  memcpy(dst, str, len + 1);
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_alloc_aligned(lua_State *L) {
  size_t size = (size_t)luaL_checkinteger(L, 1);
  size_t alignment = (size_t)luaL_checkinteger(L, 2);
  void *address = NULL;

  if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
    lua_pushnil(L);
    return 1;
  }

#if defined(_WIN32)
  address = _aligned_malloc(size, alignment);
#else
  if (alignment < sizeof(void *)) {
    alignment = sizeof(void *);
  }
  if (posix_memalign(&address, alignment, size) != 0) {
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

static int patch_free_aligned(lua_State *L) {
  void *address;

  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);

  if (address == NULL) {
    lua_pushboolean(L, 0);
    return 1;
  }

#if defined(_WIN32)
  _aligned_free(address);
#else
  free(address);
#endif

  lua_pushboolean(L, 1);
  return 1;
}

static int patch_read_struct(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint8_t *ptr = (uint8_t *)lua_topointer(L, 1);
  const char *fmt = luaL_checkstring(L, 2);

  lua_newtable(L);
  int i = 1;
  while (*fmt) {
    if (*fmt == 'i') {
      fmt++;
      if (*fmt == '8') { lua_pushinteger(L, *(int8_t*)ptr); ptr += 1; }
      else if (*fmt == '1' && *(fmt+1) == '6') { fmt++; lua_pushinteger(L, *(int16_t*)ptr); ptr += 2; }
      else if (*fmt == '3' && *(fmt+1) == '2') { fmt++; lua_pushinteger(L, *(int32_t*)ptr); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; lua_pushinteger(L, *(int64_t*)ptr); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'u') {
      fmt++;
      if (*fmt == '8') { lua_pushinteger(L, *(uint8_t*)ptr); ptr += 1; }
      else if (*fmt == '1' && *(fmt+1) == '6') { fmt++; lua_pushinteger(L, *(uint16_t*)ptr); ptr += 2; }
      else if (*fmt == '3' && *(fmt+1) == '2') { fmt++; lua_pushinteger(L, *(uint32_t*)ptr); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; lua_pushinteger(L, *(uint64_t*)ptr); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'f') {
      fmt++;
      if (*fmt == '3' && *(fmt+1) == '2') { fmt++; lua_pushnumber(L, *(float*)ptr); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; lua_pushnumber(L, *(double*)ptr); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'p') {
      lua_pushlightuserdata(L, *(void**)ptr); ptr += sizeof(void*);
    } else if (*fmt == 'x') {
      ptr += 1;
      fmt++;
      continue;
    } else {
      return luaL_error(L, "invalid struct format character: %c", *fmt);
    }
    lua_rawseti(L, -2, i++);
    fmt++;
  }
  return 1;
}

static int patch_write_struct(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  luaL_checktype(L, 3, LUA_TTABLE);
  uint8_t *ptr = (uint8_t *)lua_topointer(L, 1);
  const char *fmt = luaL_checkstring(L, 2);

  int i = 1;
  while (*fmt) {
    lua_rawgeti(L, 3, i);
    if (*fmt == 'i') {
      fmt++;
      if (*fmt == '8') { *(int8_t*)ptr = (int8_t)luaL_checkinteger(L, -1); ptr += 1; }
      else if (*fmt == '1' && *(fmt+1) == '6') { fmt++; *(int16_t*)ptr = (int16_t)luaL_checkinteger(L, -1); ptr += 2; }
      else if (*fmt == '3' && *(fmt+1) == '2') { fmt++; *(int32_t*)ptr = (int32_t)luaL_checkinteger(L, -1); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; *(int64_t*)ptr = (int64_t)luaL_checkinteger(L, -1); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'u') {
      fmt++;
      if (*fmt == '8') { *(uint8_t*)ptr = (uint8_t)luaL_checkinteger(L, -1); ptr += 1; }
      else if (*fmt == '1' && *(fmt+1) == '6') { fmt++; *(uint16_t*)ptr = (uint16_t)luaL_checkinteger(L, -1); ptr += 2; }
      else if (*fmt == '3' && *(fmt+1) == '2') { fmt++; *(uint32_t*)ptr = (uint32_t)luaL_checkinteger(L, -1); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; *(uint64_t*)ptr = (uint64_t)luaL_checkinteger(L, -1); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'f') {
      fmt++;
      if (*fmt == '3' && *(fmt+1) == '2') { fmt++; *(float*)ptr = (float)luaL_checknumber(L, -1); ptr += 4; }
      else if (*fmt == '6' && *(fmt+1) == '4') { fmt++; *(double*)ptr = (double)luaL_checknumber(L, -1); ptr += 8; }
      else return luaL_error(L, "invalid struct format");
    } else if (*fmt == 'p') {
      luaL_argcheck(L, lua_topointer(L, -1) != NULL || lua_isnil(L, -1) || lua_islightuserdata(L, -1), 3, "expected pointer-compatible type or nil for 'p'");
      *(void**)ptr = (void*)lua_topointer(L, -1);
      ptr += sizeof(void*);
    } else if (*fmt == 'x') {
      ptr += 1;
      lua_pop(L, 1);
      fmt++;
      continue;
    } else {
      return luaL_error(L, "invalid struct format character: %c", *fmt);
    }
    lua_pop(L, 1);
    i++;
    fmt++;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_hook(lua_State *L) {
  void *target = (void *)lua_topointer(L, 1);
  void *replacement = (void *)lua_topointer(L, 2);

  if (!target || !replacement) {
    return luaL_error(L, "invalid target or replacement address");
  }

#if defined(__x86_64__) || defined(_M_X64)
  // x86_64: mov rax, replacement; jmp rax
  // 48 B8 [8 bytes addr] FF E0
  uint8_t jmp_code[12] = {
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xE0
  };
  *(uint64_t *)(&jmp_code[2]) = (uint64_t)replacement;

  // Protect memory, write, and restore
  long page_size;
  uintptr_t start_page;
  uintptr_t end_page;
  size_t protect_len;

#if defined(_WIN32)
  SYSTEM_INFO si;
  DWORD oldProtect;
  GetSystemInfo(&si);
  page_size = si.dwPageSize;
#else
  page_size = sysconf(_SC_PAGESIZE);
#endif
  if (page_size <= 0) page_size = 4096;

  start_page = (uintptr_t)target & ~(page_size - 1);
  end_page   = ((uintptr_t)target + sizeof(jmp_code) + page_size - 1) & ~(page_size - 1);
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

  memcpy(target, jmp_code, sizeof(jmp_code));

#if defined(_WIN32)
  FlushInstructionCache(GetCurrentProcess(), (void*)start_page, protect_len);
  VirtualProtect((void*)start_page, protect_len, oldProtect, &oldProtect);
#else
  __builtin___clear_cache((char*)start_page, (char*)start_page + protect_len);
  mprotect((void*)start_page, protect_len, PROT_READ | PROT_EXEC);
#endif

  lua_pushboolean(L, 1);
  return 1;

#else
  return luaL_error(L, "hooking not supported on this architecture");
#endif
}

static long long get_arg_as_int(lua_State *L, int index) {
  if (lua_isnoneornil(L, index)) {
    return 0;
  }
  if (lua_isnumber(L, index)) {
    return (long long)lua_tointeger(L, index);
  }
  return (long long)(uintptr_t)lua_topointer(L, index);
}

static int patch_call(lua_State *L) {
  void *address;
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);

  if (address == NULL) {
    return luaL_error(L, "Cannot call NULL pointer");
  }

  long long a1 = get_arg_as_int(L, 2);
  long long a2 = get_arg_as_int(L, 3);
  long long a3 = get_arg_as_int(L, 4);
  long long a4 = get_arg_as_int(L, 5);
  long long a5 = get_arg_as_int(L, 6);
  long long a6 = get_arg_as_int(L, 7);

  // Cast address to a void function pointer that takes 6 arguments
  void (*func)(long long, long long, long long, long long, long long, long long) =
    (void (*)(long long, long long, long long, long long, long long, long long))address;

  func(a1, a2, a3, a4, a5, a6);

  return 0;
}

static int patch_call_ret(lua_State *L) {
  void *address;
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);

  if (address == NULL) {
    return luaL_error(L, "Cannot call NULL pointer");
  }

  long long a1 = get_arg_as_int(L, 2);
  long long a2 = get_arg_as_int(L, 3);
  long long a3 = get_arg_as_int(L, 4);
  long long a4 = get_arg_as_int(L, 5);
  long long a5 = get_arg_as_int(L, 6);
  long long a6 = get_arg_as_int(L, 7);

  // Cast address to a function pointer that returns a 64-bit integer
  long long (*func)(long long, long long, long long, long long, long long, long long) =
    (long long (*)(long long, long long, long long, long long, long long, long long))address;

  long long result = func(a1, a2, a3, a4, a5, a6);

  lua_pushinteger(L, result);
  return 1;
}

static int patch_get_arch(lua_State *L) {
#if defined(__x86_64__) || defined(_M_X64)
  lua_pushstring(L, "x86_64");
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
  lua_pushstring(L, "x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
  lua_pushstring(L, "arm64");
#elif defined(__arm__) || defined(_M_ARM)
  lua_pushstring(L, "arm");
#else
  lua_pushstring(L, "unknown");
#endif
  return 1;
}

static int patch_read_u16(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint16_t *ptr = (uint16_t *)lua_topointer(L, 1);
  lua_pushinteger(L, (lua_Integer)(*ptr));
  return 1;
}

static int patch_write_u16(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  uint16_t *ptr = (uint16_t *)lua_topointer(L, 1);
  uint16_t val = (uint16_t)luaL_checkinteger(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_read_f32(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  float *ptr = (float *)lua_topointer(L, 1);
  lua_pushnumber(L, (lua_Number)(*ptr));
  return 1;
}

static int patch_write_f32(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  float *ptr = (float *)lua_topointer(L, 1);
  float val = (float)luaL_checknumber(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_read_f64(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  double *ptr = (double *)lua_topointer(L, 1);
  lua_pushnumber(L, (lua_Number)(*ptr));
  return 1;
}

static int patch_write_f64(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  double *ptr = (double *)lua_topointer(L, 1);
  double val = (double)luaL_checknumber(L, 2);
  *ptr = val;
  lua_pushboolean(L, 1);
  return 1;
}

static int patch_read_cstring(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  const char *ptr = (const char *)lua_topointer(L, 1);
  lua_pushstring(L, ptr);
  return 1;
}

static int patch_search(lua_State *L) {
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  const uint8_t *start = (const uint8_t *)lua_topointer(L, 1);
  size_t size = (size_t)luaL_checkinteger(L, 2);
  size_t pat_len;
  const char *pattern = luaL_checklstring(L, 3, &pat_len);

  if (pat_len == 0 || size < pat_len) {
    lua_pushnil(L);
    return 1;
  }

  for (size_t i = 0; i <= size - pat_len; ++i) {
    if (memcmp(start + i, pattern, pat_len) == 0) {
      lua_pushlightuserdata(L, (void *)(start + i));
      return 1;
    }
  }

  lua_pushnil(L);
  return 1;
}

static double get_arg_as_float(lua_State *L, int index) {
  if (lua_isnoneornil(L, index)) {
    return 0.0;
  }
  return (double)lua_tonumber(L, index);
}

static int patch_call_f(lua_State *L) {
  void *address;
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
  if (address == NULL) return luaL_error(L, "Cannot call NULL pointer");
  double a1 = get_arg_as_float(L, 2); double a2 = get_arg_as_float(L, 3);
  double a3 = get_arg_as_float(L, 4); double a4 = get_arg_as_float(L, 5);
  void (*func)(double, double, double, double) = (void (*)(double, double, double, double))address;
  func(a1, a2, a3, a4);
  return 0;
}

static int patch_call_ret_f(lua_State *L) {
  void *address;
  luaL_argcheck(L, lua_topointer(L, 1) != NULL, 1, "invalid pointer");
  address = (void *)lua_topointer(L, 1);
  if (address == NULL) return luaL_error(L, "Cannot call NULL pointer");
  double a1 = get_arg_as_float(L, 2); double a2 = get_arg_as_float(L, 3);
  double a3 = get_arg_as_float(L, 4); double a4 = get_arg_as_float(L, 5);
  double (*func)(double, double, double, double) = (double (*)(double, double, double, double))address;
  double result = func(a1, a2, a3, a4);
  lua_pushnumber(L, result);
  return 1;
}

static int patch_get_state(lua_State *L) {
  lua_pushlightuserdata(L, L);
  return 1;
}

static const luaL_Reg patchlib[] = {
  {"get_arch", patch_get_arch},
  {"get_marker", patch_get_marker},
  {"get_symbol", patch_get_symbol},
  {"write", patch_write},
  {"read", patch_read},
  {"alloc", patch_alloc},
  {"free", patch_free},
  {"mprotect", patch_mprotect},
  {"call", patch_call},
  {"call_ret", patch_call_ret},
  {"to_num", patch_to_num},
  {"to_ptr", patch_to_ptr},
  {"read_u8", patch_read_u8},
  {"read_u16", patch_read_u16},
  {"read_u32", patch_read_u32},
  {"read_u64", patch_read_u64},
  {"write_u8", patch_write_u8},
  {"write_u16", patch_write_u16},
  {"write_u32", patch_write_u32},
  {"write_u64", patch_write_u64},
  {"read_f32", patch_read_f32},
  {"read_f64", patch_read_f64},
  {"write_f32", patch_write_f32},
  {"write_f64", patch_write_f64},
  {"read_cstring", patch_read_cstring},
  {"search", patch_search},
  {"call_f", patch_call_f},
  {"call_ret_f", patch_call_ret_f},
  {"get_state", patch_get_state},
  {"memcpy", patch_memcpy},
  {"memcmp", patch_memcmp},
  {"memset", patch_memset},
  {"write_cstring", patch_write_cstring},
  {"alloc_aligned", patch_alloc_aligned},
  {"free_aligned", patch_free_aligned},
  {"read_struct", patch_read_struct},
  {"write_struct", patch_write_struct},
  {"hook", patch_hook},
  {NULL, NULL}
};

LUAMOD_API int luaopen_patch (lua_State *L) {
  luaL_newlib(L, patchlib);
  return 1;
}
