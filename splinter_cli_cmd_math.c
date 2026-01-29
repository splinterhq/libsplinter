/**
 * @file splinter_cli_cmd_math.c
 * @brief Implements the CLI 'math' command for atomic integer/bitwise ops.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "splinter_cli.h"

static const char *modname = "math";

void help_cmd_math(unsigned int level) {
    (void) level;
    printf("Usage: %s <key> <op> [value]\n", modname);
    printf("Operations: inc, dec, and, or, xor, not\n");
    printf("Value can be a number (hex/dec) or a label from ~/.splinterrc\n");
    puts("");
}

int cmd_math(int argc, char *argv[]) {
    if (argc < 3) {
        help_cmd_math(1);
        return 1;
    }

    const char *key = argv[1];
    const char *op_str = argv[2];
    uint64_t val = 0;
    splinter_integer_op_t op;

    // 1. Resolve Operation Enum
    if (!strcasecmp(op_str, "inc")) op = SPL_OP_INC;
    else if (!strcasecmp(op_str, "dec")) op = SPL_OP_DEC;
    else if (!strcasecmp(op_str, "and")) op = SPL_OP_AND;
    else if (!strcasecmp(op_str, "or"))  op = SPL_OP_OR;
    else if (!strcasecmp(op_str, "xor")) op = SPL_OP_XOR;
    else if (!strcasecmp(op_str, "not")) op = SPL_OP_NOT;
    else {
        fprintf(stderr, "%s: unknown operation '%s'\n", modname, op_str);
        return 1;
    }

    // 2. Resolve Value (if not a unary 'not')
    if (op != SPL_OP_NOT) {
        if (argc < 4) {
            fprintf(stderr, "%s: operation '%s' requires a value\n", modname, op_str);
            return 1;
        }

        bool found = false;
        // Check ~/.splinterrc labels first
        for (int i = 0; i < thisuser.label_count; i++) {
            if (!strcasecmp(argv[3], thisuser.labels[i].name)) {
                val = thisuser.labels[i].mask;
                found = true;
                break;
            }
        }

        if (!found) {
            char *endptr;
            val = strtoull(argv[3], &endptr, 0);
            if (*endptr != '\0') {
                fprintf(stderr, "%s: invalid value or label '%s'\n", modname, argv[3]);
                return 1;
            }
        }
    }

    // 3. Execute the atomic transformation
    if (splinter_integer_op(key, op, &val) == 0) {
        printf("Operation '%s' applied to '%s' successfully.\n", op_str, key);
        return 0;
    } else {
        if (errno == EPROTOTYPE) {
            fprintf(stderr, "%s: key '%s' is not a BIGUINT slot.\n", modname, key);
        } else if (errno == EAGAIN) {
            fprintf(stderr, "%s: collision detected, try again.\n", modname);
        } else {
            fprintf(stderr, "%s: failed (errno: %d)\n", modname, errno);
        }
        return 1;
    }
}
