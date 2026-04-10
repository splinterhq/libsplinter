/**
 * @file splinter_cli_cmd_wasm.c
 * @brief Implements the CLI 'wasm' command to execute WASM modules with Splinter host functions.
 */

#ifdef HAVE_WASM
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wasmedge/wasmedge.h>
#include "splinter_cli.h"


static WasmEdge_Result host_splinter_get(void *Data, 
                                        const WasmEdge_CallingFrameContext *CallFrameCtx,
                                        const WasmEdge_Value *In, 
                                        WasmEdge_Value *Out) {
    (void)Data; // Silence unused parameter warning

    // Use Getters instead of .I32
    uint32_t key_ptr = WasmEdge_ValueGetI32(In[0]);
    uint32_t key_len = WasmEdge_ValueGetI32(In[1]);
    // uint32_t out_ptr = WasmEdge_ValueGetI32(In[2]);

    // Use the shortened function name
    const WasmEdge_MemoryInstanceContext *MemCtx = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCtx, 0);
    
    char key[SPLINTER_KEY_MAX];
    // Cast away const for SetData if needed, or ensure MemCtx is compatible
    WasmEdge_MemoryInstanceGetData(MemCtx, (uint8_t *)key, key_ptr, 
                                   key_len < SPLINTER_KEY_MAX ? key_len : SPLINTER_KEY_MAX - 1);
    .
    Out[0] = WasmEdge_ValueGenI32(0);
    return WasmEdge_Result_Success;
}

static WasmEdge_Result host_splinter_set(void *Data, 
                                        const WasmEdge_CallingFrameContext *CallFrameCtx,
                                        const WasmEdge_Value *In, 
                                        WasmEdge_Value *Out) {
    (void)Data;

    // 1. Use Getters for input arguments
    uint32_t key_ptr = WasmEdge_ValueGetI32(In[0]);
    uint32_t key_len = WasmEdge_ValueGetI32(In[1]);
    uint32_t val_ptr = WasmEdge_ValueGetI32(In[2]);
    uint32_t val_len = WasmEdge_ValueGetI32(In[3]);

    // 2. Updated context retrieval name
    const WasmEdge_MemoryInstanceContext *MemCtx = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCtx, 0);
    
    // 3. Extract Key from guest memory
    char key[SPLINTER_KEY_MAX];
    uint32_t actual_key_len = (key_len < SPLINTER_KEY_MAX) ? key_len : SPLINTER_KEY_MAX - 1;
    WasmEdge_MemoryInstanceGetData(MemCtx, (uint8_t *)key, key_ptr, actual_key_len);
    key[actual_key_len] = '\0';

    // 4. Extract Value from guest memory
    uint8_t *val = malloc(val_len);
    if (!val) return WasmEdge_Result_Terminate; // Handle allocation failure

    WasmEdge_MemoryInstanceGetData(MemCtx, val, val_ptr, val_len);

    // 5. Interact with the Splinter bus
    int rc = splinter_set(key, val, val_len);
    free(val);
    
    // 6. Use the generator for the return value
    Out[0] = WasmEdge_ValueGenI32(rc == 0 ? 1 : 0);
    
    return WasmEdge_Result_Success;
}

void help_cmd_wasm(unsigned int level) {
    (void)level;
    printf("Usage: wasm <plugin.wasm> [function_name]\n");
    printf("Executes a WASM module with access to the Splinter bus.\n");
}

int cmd_wasm(int argc, char *argv[]) {
    if (argc < 2) {
        help_cmd_wasm(0);
        return 1;
    }

    const char *wasm_path = argv[1];
    const char *func_name = (argc > 2) ? argv[2] : "_start";

    /* 1. Setup VM and Configuration */
    WasmEdge_ConfigureContext *ConfCtx = WasmEdge_ConfigureCreate();
    WasmEdge_ConfigureAddProposal(ConfCtx, WasmEdge_Proposal_SIMD); 
    WasmEdge_VMContext *VMCtx = WasmEdge_VMCreate(ConfCtx, NULL);

    /* 2. Create the 'splinter' Host Module */
    WasmEdge_String ExportName = WasmEdge_StringCreateByCString("splinter");
    WasmEdge_ModuleInstanceContext *HostMod = WasmEdge_ModuleInstanceCreate(ExportName);
    WasmEdge_StringDelete(ExportName);

    /* 3. Register 'get' host function: (i32, i32, i32) -> i32 */
    WasmEdge_ValType GetParams[] = { WasmEdge_ValTypeGenI32(), WasmEdge_ValTypeGenI32(), WasmEdge_ValTypeGenI32() };
    WasmEdge_ValType GetReturns[] = { WasmEdge_ValTypeGenI32() };
    WasmEdge_FunctionTypeContext *FTypGet = WasmEdge_FunctionTypeCreate(GetParams, 3, GetReturns, 1);
    WasmEdge_FunctionInstanceContext *HostFuncGet = WasmEdge_FunctionInstanceCreate(FTypGet, host_splinter_get, NULL, 0);
    
    WasmEdge_String GetName = WasmEdge_StringCreateByCString("get");
    WasmEdge_ModuleInstanceAddFunction(HostMod, GetName, HostFuncGet);
    WasmEdge_StringDelete(GetName);
    WasmEdge_FunctionTypeDelete(FTypGet);

    /* 4. Register 'set' host function: (i32, i32, i32, i32) -> i32 */
    WasmEdge_ValType SetParams[] = { WasmEdge_ValTypeGenI32(), WasmEdge_ValTypeGenI32(), WasmEdge_ValTypeGenI32(), WasmEdge_ValTypeGenI32() };
    WasmEdge_ValType SetReturns[] = { WasmEdge_ValTypeGenI32() };
    WasmEdge_FunctionTypeContext *FTypSet = WasmEdge_FunctionTypeCreate(SetParams, 4, SetReturns, 1);
    WasmEdge_FunctionInstanceContext *HostFuncSet = WasmEdge_FunctionInstanceCreate(FTypSet, host_splinter_set, NULL, 0);
    
    WasmEdge_String SetName = WasmEdge_StringCreateByCString("set");
    WasmEdge_ModuleInstanceAddFunction(HostMod, SetName, HostFuncSet);
    WasmEdge_StringDelete(SetName);
    WasmEdge_FunctionTypeDelete(FTypSet);

    /* 5. Register the module into the VM */
    WasmEdge_VMRegisterModuleFromImport(VMCtx, HostMod);

    /* 6. Run the target function from the WASM file */
    WasmEdge_String FuncStr = WasmEdge_StringCreateByCString(func_name);
    WasmEdge_Result Res = WasmEdge_VMRunWasmFromFile(VMCtx, wasm_path, FuncStr, NULL, 0, NULL, 0);

    if (!WasmEdge_ResultOK(Res)) {
        fprintf(stderr, "WASM Execution failed: %s\n", WasmEdge_ResultGetMessage(Res));
    }

    /* Cleanup */
    WasmEdge_StringDelete(FuncStr);
    WasmEdge_VMDelete(VMCtx);
    WasmEdge_ConfigureDelete(ConfCtx);
    
    return WasmEdge_ResultOK(Res) ? 0 : 1;
}
#endif // HAVE_WASM