/**
 * @file splinter_cli_cmd_label.c
 * @brief Implements the CLI 'label' command to tag keys via Bloom filter.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <lua5.4/lauxlib.h>
#include "splinter_cli.h"

static int lua_splinter_math(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *op_str = luaL_checkstring(L, 2);
    uint64_t val = (uint64_t)luaL_optinteger(L, 3, 0);
    splinter_integer_op_t op;

    // 1. Resolve Operation (Surgical string-to-enum)
    if (strcasecmp(op_str, "inc") == 0) op = SPL_OP_INC;
    else if (strcasecmp(op_str, "dec") == 0) op = SPL_OP_DEC;
    else if (strcasecmp(op_str, "and") == 0) op = SPL_OP_AND;
    else if (strcasecmp(op_str, "or") == 0)  op = SPL_OP_OR;
    else if (strcasecmp(op_str, "xor") == 0) op = SPL_OP_XOR;
    else if (strcasecmp(op_str, "not") == 0) op = SPL_OP_NOT;
    else return luaL_error(L, "invalid math operation: %s", op_str);

    // 2. Execute Atomic Operation
    // The C core handles the 64-bit fast-track and seqlock.
    if (splinter_integer_op(key, op, &val) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }

    // 3. Handle Failure Modes (Schema mismatch / Missing key)
    if (errno == EPROTOTYPE) {
        return luaL_error(L, "key '%s' is not a BIGUINT slot", key);
    }
    
    lua_pushboolean(L, 0);
    return 1;
}

static int lua_splinter_get(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    char buf[SPLINTER_KEY_MAX + 8]; // Support key + alignment pad
    size_t received = 0;
    splinter_slot_snapshot_t snap = {0};

    // 1. Fetch data from SHM
    if (splinter_get(key, buf, sizeof(buf), &received) != 0) {
        lua_pushnil(L);
        return 1;
    }

    // 2. Determine semantic type via snapshot
    if (splinter_get_slot_snapshot(key, &snap) == 0) {
        if (snap.type_flag & SPL_SLOT_TYPE_BIGUINT) {
            // Return as a Lua integer for math
            uint64_t val = *(uint64_t *)buf;
            lua_pushinteger(L, (lua_Integer)val);
            return 1;
        }
    }

    // 3. Fallback to string (VARTEXT/JSON/VOID)
    lua_pushlstring(L, buf, received);
    return 1;
}

static int lua_splinter_set(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    int type = lua_type(L, 2);

    if (type == LUA_TNUMBER) {
        // 1. Auto-promote to BIGUINT and expand if needed
        uint64_t val = (uint64_t)lua_tointeger(L, 2);
        
        // Prepare the slot for 8-byte integer operations
        if (splinter_set_named_type(key, SPL_SLOT_TYPE_BIGUINT) != 0) {
            // If key doesn't exist, we create it first as VOID then upgrade
            // This maintains the "silent" experience
            splinter_set(key, &val, 8); 
            splinter_set_named_type(key, SPL_SLOT_TYPE_BIGUINT);
        } else {
            // Key existed, was expanded, now store the integer
            splinter_set(key, &val, 8);
        }
    } else {
        // 2. Standard string/binary set
        size_t len;
        const char *val = luaL_checklstring(L, 2, &len);
        if (splinter_set(key, val, len) != 0) {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_splinter_unset(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    
    // reset everything: hash, epoch, bloom, and type
    int deleted_len = splinter_unset(key);
    if (deleted_len >= 0) {
        lua_pushinteger(L, deleted_len);
        return 1;
    }
    
    lua_pushboolean(L, 0);
    return 1;
}

static int lua_splinter_label(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    uint64_t mask = 0;

    if (lua_isnumber(L, 2)) {
        mask = (uint64_t)lua_tointeger(L, 2);
    } else {
        // Here you could link into your label resolution logic if desired
        return luaL_error(L, "Label must be a numeric mask");
    }

    if (splinter_set_label(key, mask) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

static const char *modname = "label";

void help_cmd_lua(unsigned int level) {
    (void) level;
    printf("Usage: %s <key> <label_name>\n", modname);
    printf("Labels are defined in ~/.splinterrc and apply to the 64-bit Bloom filter.\n");
    puts("");
}

/**
 * @brief Entry point for the Lua module.
 * Maps C functions to the 'bus' global.
 */
int luaopen_splinter(lua_State *L) {
    // 1. Define the library methods
    static const struct luaL_Reg bus_funcs[] = {
        {"get",    lua_splinter_get},
        {"set",    lua_splinter_set},
        {"math",   lua_splinter_math},
        {"label",  lua_splinter_label},
        {"unset",  lua_splinter_unset},
        {NULL, NULL}
    };

    // 2. Create the 'bus' table
    luaL_newlib(L, bus_funcs);
    
    // 3. Return the table to Lua
    return 1;
}

int cmd_lua(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: lua <script.lua>\n");
        return 1;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Register our surgical splinter module
    luaL_requiref(L, "splinter", luaopen_splinter, 1);
    lua_pop(L, 1);

    // Run the script
    if (luaL_dofile(L, argv[1]) != LUA_OK) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
