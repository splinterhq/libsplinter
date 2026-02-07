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

/**
 * @brief Batch-retrieves a tandem set (key, key.1, key.2, etc) into a Lua table.
 */
static int lua_splinter_get_tandem(lua_State *L) {
    const char *base_key = luaL_checkstring(L, 1);
    int max_orders = (int)luaL_optinteger(L, 2, 64); // Safety cap
    char tandem_name[SPLINTER_KEY_MAX];
    char buf[4096]; // Use a larger buffer or H->max_val_sz if accessible
    size_t received = 0;

    lua_newtable(L);

    for (int i = 0; i < max_orders; i++) {
        if (i == 0) {
            strncpy(tandem_name, base_key, SPLINTER_KEY_MAX - 1);
        } else {
            snprintf(tandem_name, sizeof(tandem_name), "%s.%d", base_key, i);
        }

        if (splinter_get(tandem_name, buf, sizeof(buf), &received) == 0) {
            // Push the index (1-based for Lua) and the value
            lua_pushinteger(L, i + 1);
            lua_pushlstring(L, buf, received);
            lua_settable(L, -3);
        } else {
            // Stop at the first missing order
            break;
        }
    }

    return 1;
}

/**
 * @brief Batch-pushes a Lua table of values as a tandem set.
 * Expects a table where index 1 is the base key, index 2 is order 1, etc.
 */
static int lua_splinter_set_tandem(lua_State *L) {
    const char *base_key = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    
    char tandem_name[SPLINTER_KEY_MAX];
    int n = (int)lua_rawlen(L, 2);

    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 2, i);
        size_t len;
        const char *val = lua_tolstring(L, -1, &len);

        if (i == 1) {
            strncpy(tandem_name, base_key, SPLINTER_KEY_MAX - 1);
        } else {
            snprintf(tandem_name, sizeof(tandem_name), "%s.%d", base_key, i - 1);
        }

        if (val && splinter_set(tandem_name, val, len) != 0) {
            lua_pushboolean(L, 0);
            return 1;
        }
        lua_pop(L, 1);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_splinter_math(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *op_str = luaL_checkstring(L, 2);
    uint64_t val = (uint64_t)luaL_optinteger(L, 3, 0);
    splinter_integer_op_t op;

    if (strcasecmp(op_str, "inc") == 0) op = SPL_OP_INC;
    else if (strcasecmp(op_str, "dec") == 0) op = SPL_OP_DEC;
    else if (strcasecmp(op_str, "and") == 0) op = SPL_OP_AND;
    else if (strcasecmp(op_str, "or") == 0)  op = SPL_OP_OR;
    else if (strcasecmp(op_str, "xor") == 0) op = SPL_OP_XOR;
    else if (strcasecmp(op_str, "not") == 0) op = SPL_OP_NOT;
    else return luaL_error(L, "invalid math operation: %s", op_str);

    if (splinter_integer_op(key, op, &val) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    }

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

static const char *modname = "lua";

void help_cmd_lua(unsigned int level) {
    (void) level;
    printf("Usage: %s <script.lua>\n", modname);
    puts("");
}

// This may end up being shared across modules
// Hence, not static. But not prototyped publicly either, yet.
int luaopen_splinter(lua_State *L) {
    static const struct luaL_Reg bus_funcs[] = {
        {"get",        lua_splinter_get},
        {"get_tandem", lua_splinter_get_tandem},
        {"set",        lua_splinter_set},
        {"set_tandem", lua_splinter_set_tandem},
        {"math",       lua_splinter_math},
        {"label",      lua_splinter_label},
        {"unset",      lua_splinter_unset},
        {NULL, NULL}
    };
    luaL_newlib(L, bus_funcs);
    return 1;
}

int cmd_lua(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: lua <script.lua>\n");
        return 1;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_requiref(L, "splinter", luaopen_splinter, 1);
    lua_pop(L, 1);

    if (luaL_dofile(L, argv[1]) != LUA_OK) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_close(L);
    return 0;
}
