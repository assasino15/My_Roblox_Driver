#include <windows.h>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "lua.hpp"

#define PIPE_NAME "\\\\.\\pipe\\LuaExecutorPipe"
#define BUFFER_SIZE 8192

lua_State* L;

static int l_get_module_base(lua_State* L) {
    const char* moduleName = luaL_checkstring(L, 1);
    HMODULE hMod = GetModuleHandleA(moduleName);
    lua_pushinteger(L, (lua_Integer)hMod);
    return 1;
}

static int l_read_memory(lua_State* L) {
    uintptr_t address = (uintptr_t)luaL_checkinteger(L, 1);
    const char* type = luaL_checkstring(L, 2);
    if (strcmp(type, "i32") == 0) lua_pushinteger(L, *(int32_t*)address);
    else if (strcmp(type, "f") == 0) lua_pushnumber(L, *(float*)address);
    else if (strcmp(type, "str") == 0) lua_pushstring(L, (char*)address);
    else lua_pushnil(L);
    return 1;
}

static int l_write_memory(lua_State* L) {
    uintptr_t address = (uintptr_t)luaL_checkinteger(L, 1);
    const char* type = luaL_checkstring(L, 3);
    if (strcmp(type, "i32") == 0) *(int32_t*)address = (int32_t)luaL_checkinteger(L, 2);
    else if (strcmp(type, "f") == 0) *(float*)address = (float)luaL_checknumber(L, 2);
    return 0;
}

static int l_exec_file(lua_State* L) {
    const char* filePath = luaL_checkstring(L, 1);
    std::ifstream file(filePath);
    if (!file.is_open()) return 0;
    std::stringstream buffer;
    buffer << file.rdbuf();
    luaL_dostring(L, buffer.str().c_str());
    return 0;
}

void PipeServerThread() {
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;
    
    HANDLE hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) return;

    ConnectNamedPipe(hPipe, NULL);

    while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) != FALSE) {
        if (bytesRead > 0) {
            luaL_dostring(L, buffer);
        }
    }

    CloseHandle(hPipe);
}

void Initialize() {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "get_module_base", l_get_module_base);
    lua_register(L, "read_memory", l_read_memory);
    lua_register(L, "write_memory", l_write_memory);
    lua_register(L, "execfile", l_exec_file);

    PipeServerThread();
    
    lua_close(L);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Initialize, NULL, 0, NULL);
    }
    return TRUE;
}