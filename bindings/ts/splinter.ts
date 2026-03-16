/**
 * TS FFI For Splinter
 * Auto-generated from splinter.h with Gemini 3
 * Minor adjustments applied
 * License: MIT 
 */

const LIB_NAME = "libsplinter";

function getLibFilename(): string {
    // @ts-ignore: Deno/Bun cross-compatibility
    const isWindows = typeof process !== "undefined" && process.platform === "win32" || 
                      // @ts-ignore
                      typeof Deno !== "undefined" && Deno.build.os === "windows";
    // @ts-ignore
    const isMac = typeof process !== "undefined" && process.platform === "darwin" || 
                  // @ts-ignore
                  typeof Deno !== "undefined" && Deno.build.os === "darwin";
    return isWindows ? `${LIB_NAME}.dll` : isMac ? `${LIB_NAME}.dylib` : `${LIB_NAME}.so`;
}

export const SPL_SLOT_TYPE = {
    VOID: 1 << 0,
    BIGINT: 1 << 1,
    BIGUINT: 1 << 2,
    JSON: 1 << 3,
    BINARY: 1 << 4,
    IMGDATA: 1 << 5,
    AUDIO: 1 << 6,
    VARTEXT: 1 << 7,
} as const;

export interface SplinterStore {
    open(name: string): boolean;
    close(): void;
    set(key: string, value: string | Uint8Array): boolean;
    get(key: string): Uint8Array | null;
    getString(key: string): string | null;
    unset(key: string): number;
    getEpoch(key: string): bigint;
    setLabel(key: string, mask: bigint): number;
    setNamedType(key: string, mask: number): number;
    getSignalCount(groupId: number): bigint;
    watchRegister(key: string, groupId: number): number;
    watchLabelRegister(bloomMask: bigint, groupId: number): number;
}

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// --- Bun Implementation ---

class BunSplinter implements SplinterStore {
    private ffi: any;

    constructor(libPath: string) {
        // @ts-ignore: Bun-specific
        const { dlopen, FFIType } = require("bun:ffi");

        this.ffi = dlopen(libPath, {
            splinter_open: { args: [FFIType.cstring], returns: FFIType.i32 },
            splinter_close: { args: [], returns: FFIType.void },
            splinter_set: { args: [FFIType.cstring, FFIType.ptr, FFIType.usize], returns: FFIType.i32 },
            splinter_get: { args: [FFIType.cstring, FFIType.ptr, FFIType.usize, FFIType.ptr], returns: FFIType.i32 },
            splinter_unset: { args: [FFIType.cstring], returns: FFIType.i32 },
            splinter_get_epoch: { args: [FFIType.cstring], returns: FFIType.u64 },
            splinter_get_signal_count: { args: [FFIType.u8], returns: FFIType.u64 },
            splinter_set_label: { args: [FFIType.cstring, FFIType.u64], returns: FFIType.i32 },
            splinter_set_named_type: { args: [FFIType.cstring, FFIType.u16], returns: FFIType.i32 },
            splinter_watch_register: { args: [FFIType.cstring, FFIType.u8], returns: FFIType.i32 },
            splinter_watch_label_register: { args: [FFIType.u64, FFIType.u8], returns: FFIType.i32 },
        });
    }

    open(name: string): boolean { return this.ffi.symbols.splinter_open(encoder.encode(name + "\0")) === 0; }
    close(): void { this.ffi.symbols.splinter_close(); }

    set(key: string, value: string | Uint8Array): boolean {
        const data = typeof value === "string" ? encoder.encode(value) : value;
        // @ts-ignore
        const { ptr } = require("bun:ffi");
        return this.ffi.symbols.splinter_set(encoder.encode(key + "\0"), ptr(data), data.length) === 0;
    }

    get(key: string): Uint8Array | null {
        const maxLen = 4096;
        const buffer = new Uint8Array(maxLen);
        const outLen = new BigUint64Array(1);
        // @ts-ignore
        const { ptr } = require("bun:ffi");
        const rc = this.ffi.symbols.splinter_get(encoder.encode(key + "\0"), ptr(buffer), maxLen, ptr(outLen));
        return rc !== 0 ? null : buffer.slice(0, Number(outLen[0]));
    }

    getString(key: string): string | null {
        const data = this.get(key);
        return data ? decoder.decode(data) : null;
    }

    unset(key: string): number { return this.ffi.symbols.splinter_unset(encoder.encode(key + "\0")); }
    getEpoch(key: string): bigint { return BigInt(this.ffi.symbols.splinter_get_epoch(encoder.encode(key + "\0"))); }
    getSignalCount(groupId: number): bigint { return BigInt(this.ffi.symbols.splinter_get_signal_count(groupId)); }
    setLabel(key: string, mask: bigint): number { return this.ffi.symbols.splinter_set_label(encoder.encode(key + "\0"), mask); }
    setNamedType(key: string, mask: number): number { return this.ffi.symbols.splinter_set_named_type(encoder.encode(key + "\0"), mask); }
    watchRegister(key: string, gid: number): number { return this.ffi.symbols.splinter_watch_register(encoder.encode(key + "\0"), gid); }
    watchLabelRegister(mask: bigint, gid: number): number { return this.ffi.symbols.splinter_watch_label_register(mask, gid); }
}

// --- Deno Implementation ---

class DenoSplinter implements SplinterStore {
    private dylib: Deno.DynamicLibrary<any>;
    private symbols: Record<string, (...args: any[]) => any>;

    constructor(libPath: string) {
        this.dylib = Deno.dlopen(libPath, {
            splinter_open: { parameters: ["buffer"], result: "i32" },
            splinter_close: { parameters: [], result: "void" },
            splinter_set: { parameters: ["buffer", "buffer", "usize"], result: "i32" },
            splinter_get: { parameters: ["buffer", "buffer", "usize", "buffer"], result: "i32" },
            splinter_unset: { parameters: ["buffer"], result: "i32" },
            splinter_get_epoch: { parameters: ["buffer"], result: "u64" },
            splinter_get_signal_count: { parameters: ["u8"], result: "u64" },
            splinter_set_label: { parameters: ["buffer", "u64"], result: "i32" },
            splinter_set_named_type: { parameters: ["buffer", "u16"], result: "i32" },
            splinter_watch_register: { parameters: ["buffer", "u8"], result: "i32" },
            splinter_watch_label_register: { parameters: ["u64", "u8"], result: "i32" },
        });
        this.symbols = this.dylib.symbols as Record<string, (...args: any[]) => any>;
    }

    /**
     * Helper to ensure C-compatible null-terminated keys.
     * Uses a Uint8Array to ensure Deno passes a raw pointer.
     */
    private cstr(str: string): Uint8Array {
        const buf = new Uint8Array(str.length + 1);
        encoder.encodeInto(str, buf);
        buf[str.length] = 0; // Explicit null terminator
        return buf;
    }

    open(name: string): boolean { 
        return this.symbols.splinter_open(this.cstr(name)) === 0; 
    }
    
    close(): void { 
        this.symbols.splinter_close();
        this.dylib.close(); 
    }

    set(key: string, value: string | Uint8Array): boolean {
        const data = typeof value === "string" ? encoder.encode(value) : value;
        // Ensure we handle the return code: 0 is success in C
        const rc = this.symbols.splinter_set(this.cstr(key), data, data.length);
        return rc === 0;
    }

    get(key: string): Uint8Array | null {
        const maxLen = 4096;
        const buffer = new Uint8Array(maxLen);
        const outLen = new BigUint64Array(1);
        
        const rc = this.symbols.splinter_get(
            this.cstr(key), 
            buffer, 
            maxLen, 
            new Uint8Array(outLen.buffer)
        );

        if (rc !== 0) return null;
        return buffer.slice(0, Number(outLen[0]));
    }

    getString(key: string): string | null {
        const data = this.get(key);
        return data ? decoder.decode(data) : null;
    }

    unset(key: string): number { return this.symbols.splinter_unset(this.cstr(key)); }
    
    getEpoch(key: string): bigint { 
        // Force the return to be a BigInt to handle u64 correctly
        return BigInt(this.symbols.splinter_get_epoch(this.cstr(key))); 
    }

    getSignalCount(groupId: number): bigint { 
        return BigInt(this.symbols.splinter_get_signal_count(groupId)); 
    }

    setLabel(key: string, mask: bigint): number { 
        return this.symbols.splinter_set_label(this.cstr(key), mask); 
    }

    setNamedType(key: string, mask: number): number { 
        return this.symbols.splinter_set_named_type(this.cstr(key), mask); 
    }

    watchRegister(key: string, gid: number): number { 
        return this.symbols.splinter_watch_register(this.cstr(key), gid); 
    }

    watchLabelRegister(mask: bigint, gid: number): number { 
        return this.symbols.splinter_watch_label_register(mask, gid); 
    }
}

export class Splinter {
    static connect(busName: string, customLibPath?: string): SplinterStore {
        const libPath = customLibPath || `./${getLibFilename()}`;
        // @ts-ignore
        const isBun = typeof process !== "undefined" && process.versions?.bun;
        // @ts-ignore
        const isDeno = typeof Deno !== "undefined";

        if (isBun) return new BunSplinter(libPath);
        if (isDeno) return new DenoSplinter(libPath);
        throw new Error("Runtime not supported");
    }
}

/**
 * SplinterWatcher
 * An event-driven bridge for Deno isolates to react to Splinter Signal Groups.
 */
export class SplinterWatcher {
    private store: SplinterStore;
    private lastCounts: Map<number, bigint> = new Map();

    constructor(store: SplinterStore) {
        this.store = store;
    }

    /**
     * Blocks (async) until a signal group count increments.
     * Use this in a Deno.cron or a long-running while loop.
     */
    async nextSignal(groupId: number, pollMs = 50): Promise<bigint> {
        let current = this.store.getSignalCount(groupId);
        
        // Initialize if first time seeing this group
        if (!this.lastCounts.has(groupId)) {
            this.lastCounts.set(groupId, current);
        }

        const previous = this.lastCounts.get(groupId)!;

        while (current <= previous) {
            await new Promise(resolve => setTimeout(resolve, pollMs));
            current = this.store.getSignalCount(groupId);
        }

        this.lastCounts.set(groupId, current);
        return current;
    }

    /**
     * Starts a background loop that fires a callback on signal.
     */
    subscribe(groupId: number, callback: (count: bigint) => void, pollMs = 100) {
        (async () => {
            while (true) {
                const newCount = await this.nextSignal(groupId, pollMs);
                callback(newCount);
            }
        })();
    }
}
