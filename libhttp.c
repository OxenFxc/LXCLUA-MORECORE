/*
** libhttp.c
** HTTP library for Lua
*/

#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Helper to parse URL into host, port, path */
static int parse_url(const char *url, char *host, size_t host_len, int *port, char *path, size_t path_len, int *is_https) {
    const char *p = url;
    const char *host_start;
    size_t h_len;

    *port = 80;
    *is_https = 0;

    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
        *is_https = 1;
    } else {
        return 0; /* Invalid scheme */
    }

    host_start = p;
    while (*p && *p != ':' && *p != '/') {
        p++;
    }

    h_len = p - host_start;
    if (h_len >= host_len) return 0; /* Host too long */
    memcpy(host, host_start, h_len);
    host[h_len] = '\0';

    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') {
            *port = (*port * 10) + (*p - '0');
            p++;
        }
    }

    if (*p == '\0') {
        if (path_len < 2) return 0;
        strcpy(path, "/");
    } else if (*p == '/') {
        if (strlen(p) >= path_len) return 0;
        strcpy(path, p);
    } else {
        return 0; /* Invalid URL format */
    }

    return 1;
}


#if defined(_WIN32)

#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet")

static int http_request(lua_State *L, const char *method) {
    const char *url = luaL_checkstring(L, 1);
    const char *body = NULL;
    size_t body_len = 0;
    const char *headers = NULL;

    if (strcmp(method, "POST") == 0) {
        body = luaL_optlstring(L, 2, NULL, &body_len);
        headers = luaL_optstring(L, 3, NULL);
    } else {
        headers = luaL_optstring(L, 2, NULL);
    }

    HINTERNET hInternet = InternetOpenA("LuaHTTPClient/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        lua_pushnil(L);
        lua_pushstring(L, "InternetOpen failed");
        return 2;
    }

    /* Use InternetOpenUrl for simple GET, but HttpOpenRequest for more control/POST */
    /* To handle both easily, we'll parse the URL and use Connect/Request pattern */

    char host[256];
    char path[1024];
    int port;
    int is_https;

    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path), &is_https)) {
        InternetCloseHandle(hInternet);
        lua_pushnil(L);
        lua_pushstring(L, "Invalid URL");
        return 2;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        lua_pushnil(L);
        lua_pushstring(L, "InternetConnect failed");
        return 2;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (is_https) {
        flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, method, path, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        lua_pushnil(L);
        lua_pushstring(L, "HttpOpenRequest failed");
        return 2;
    }

    BOOL res = HttpSendRequestA(hRequest, headers, headers ? (DWORD)strlen(headers) : 0, (LPVOID)body, (DWORD)body_len);
    if (!res) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        lua_pushnil(L);
        lua_pushstring(L, "HttpSendRequest failed");
        return 2;
    }

    /* Get Status Code */
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &size, NULL);

    /* Read Body */
    luaL_Buffer b;
    luaL_buffinit(L, &b);

    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        luaL_addlstring(&b, buffer, bytesRead);
    }

    lua_pushinteger(L, statusCode);
    luaL_pushresult(&b);

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return 2;
}

#elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

static int http_request(lua_State *L, const char *method) {
    const char *url = luaL_checkstring(L, 1);
    const char *body = NULL;
    size_t body_len = 0;
    const char *headers = NULL;

    if (strcmp(method, "POST") == 0) {
        body = luaL_optlstring(L, 2, NULL, &body_len);
        headers = luaL_optstring(L, 3, NULL);
    } else {
        headers = luaL_optstring(L, 2, NULL);
    }

    char host[256];
    char path[1024];
    int port;
    int is_https;

    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path), &is_https)) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid URL");
        return 2;
    }

    if (is_https) {
        lua_pushnil(L);
        lua_pushstring(L, "HTTPS not supported on this platform without external libraries");
        return 2;
    }

    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, "DNS resolution failed");
        return 2;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Socket creation failed");
        return 2;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        lua_pushnil(L);
        lua_pushstring(L, "Connection failed");
        return 2;
    }

    /* Construct HTTP Request */
    /* Using HTTP/1.0 to simplify response handling (no chunked encoding usually) */
    luaL_Buffer req;
    luaL_buffinit(L, &req);
    luaL_addstring(&req, method);
    luaL_addstring(&req, " ");
    luaL_addstring(&req, path);
    luaL_addstring(&req, " HTTP/1.0\r\n");
    luaL_addstring(&req, "Host: ");
    luaL_addstring(&req, host);
    luaL_addstring(&req, "\r\n");
    luaL_addstring(&req, "User-Agent: LuaHTTPClient/1.0\r\n");
    luaL_addstring(&req, "Connection: close\r\n");

    if (body) {
        char len_str[32];
        snprintf(len_str, sizeof(len_str), "%lu", (unsigned long)body_len);
        luaL_addstring(&req, "Content-Length: ");
        luaL_addstring(&req, len_str);
        luaL_addstring(&req, "\r\n");
    }

    if (headers) {
        luaL_addstring(&req, headers);
        if (headers[strlen(headers)-1] != '\n') {
            luaL_addstring(&req, "\r\n");
        }
    }

    luaL_addstring(&req, "\r\n");
    if (body) {
        luaL_addlstring(&req, body, body_len);
    }

    luaL_pushresult(&req);
    const char *req_str = lua_tostring(L, -1);
    size_t req_len = lua_rawlen(L, -1);

    ssize_t sent = 0;
    while (sent < req_len) {
        ssize_t n = send(sockfd, req_str + sent, req_len - sent, 0);
        if (n < 0) {
            close(sockfd);
            lua_pushnil(L);
            lua_pushstring(L, "Send failed");
            return 2;
        }
        sent += n;
    }
    lua_pop(L, 1); /* pop request string */

    /* Read Response */
    luaL_Buffer resp;
    luaL_buffinit(L, &resp);
    char buffer[4096];
    ssize_t n;
    while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        luaL_addlstring(&resp, buffer, n);
    }
    close(sockfd);

    luaL_pushresult(&resp);
    const char *full_resp = lua_tostring(L, -1);
    size_t full_len = lua_rawlen(L, -1);

    /* Parse Status Code */
    int status_code = 0;
    const char *status_line_end = strstr(full_resp, "\r\n");
    if (status_line_end) {
        const char *p = full_resp;
        while (p < status_line_end && *p != ' ') p++;
        if (p < status_line_end) {
            status_code = atoi(p + 1);
        }
    }

    /* Find Body */
    const char *body_start = strstr(full_resp, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        lua_pushinteger(L, status_code);
        lua_pushlstring(L, body_start, full_len - (body_start - full_resp));
    } else {
        /* No headers end found? Return full response as body or error? */
        /* Assuming simple HTTP 1.0, maybe body is empty or malformed */
        lua_pushinteger(L, status_code);
        lua_pushlstring(L, "", 0);
    }

    return 2;
}

#else

/* Placeholder for other platforms */
static int http_request(lua_State *L, const char *method) {
    lua_pushnil(L);
    lua_pushstring(L, "Platform not supported");
    return 2;
}

#endif

static int l_http_get(lua_State *L) {
    return http_request(L, "GET");
}

static int l_http_post(lua_State *L) {
    return http_request(L, "POST");
}

static const luaL_Reg httplib[] = {
    {"get", l_http_get},
    {"post", l_http_post},
    {NULL, NULL}
};

LUAMOD_API int luaopen_http(lua_State *L) {
    luaL_newlib(L, httplib);
    return 1;
}
