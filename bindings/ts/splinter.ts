/**
 * splinter.ts
 * Auto-sensing FFI bindings for libsplinter across Bun and Deno.
 * (Generated - Can be edited or fed back into context to be regenerated)
 */

const LIB_NAME = "libsplinter";

// Helper to determine the correct dynamic library extension
function getLibFilename(): string {
    const isWindows = typeof process !== "undefined" && process.platform === "win32" || 
                      typeof Deno !== "undefined" && Deno.build.os === "windows";
    const isMac = typeof process !== "undefined" && process.platform === "darwin" || 
                  typeof Deno !== "undefined" && Deno.build.os === "darwin";
    return isWindows ? `${LIB_NAME}.dll` : isMac ? `${LIB_NAME}.dylib` : `${LIB_NAME}.so`;
}

// --- Unified Interface ---

export interface SplinterStore {
    open(name: string): boolean;
    close(): void;
    set(key: string, value: string | Uint8Array): boolean;
    get(key: string): Uint8Array | null;
    getString(key: string): string | null;
    getEpoch(key: string): bigint;
    getSignalCount(groupId: number): bigint;
}

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// --- Bun Implementation ---

class BunSplinter implements SplinterStore {
    private ffi: any;

    constructor(libPath: string) {
        // We use dynamic import/require concepts safely hidden inside the class
        // to avoid Deno panicking on Bun-specific syntax.
        const { dlopen, FFIType, ptr } = require("bun:ffi");

        this.ffi = dlopen(libPath, {
            splinter_open: { args: [FFIType.cstring], returns: FFIType.i32 },
            splinter_close: { args: [], returns: FFIType.void },
            splinter_set: { args: [FFIType.cstring, FFIType.ptr, FFIType.usize], returns: FFIType.i32 },
            splinter_get: { args: [FFIType.cstring, FFIType.ptr, FFIType.usize, FFIType.ptr], returns: FFIType.i32 },
            splinter_get_epoch: { args: [FFIType.cstring], returns: FFIType.u64 },
            splinter_get_signal_count: { args: [FFIType.u8], returns: FFIType.u64 },
        });
    }

    open(name: string): boolean {
        return this.ffi.symbols.splinter_open(Buffer.from(name + "\0")) === 0;
    }

    close(): void {
        this.ffi.symbols.splinter_close();
    }

    set(key: string, value: string | Uint8Array): boolean {
        const data = typeof value === "string" ? encoder.encode(value) : value;
        const { ptr } = require("bun:ffi");
        return this.ffi.symbols.splinter_set(Buffer.from(key + "\0"), ptr(data), data.length) === 0;
    }

    get(key: string): Uint8Array | null {
        // Splinter default slot size is 4096. Adjust if you configured larger arenas.
        const maxLen = 4096; 
        const buffer = new Uint8Array(maxLen);
        const outLen = new BigUint64Array(1); // To receive the actual size_t length
        const { ptr } = require("bun:ffi");

        const rc = this.ffi.symbols.splinter_get(
            Buffer.from(key + "\0"), 
            ptr(buffer), 
            maxLen, 
            ptr(outLen)
        );

        if (rc !== 0) return null;
        return buffer.slice(0, Number(outLen[0]));
    }

    getString(key: string): string | null {
        const data = this.get(key);
        return data ? decoder.decode(data) : null;
    }

    getEpoch(key: string): bigint {
        return BigInt(this.ffi.symbols.splinter_get_epoch(Buffer.from(key + "\0")));
    }

    getSignalCount(groupId: number): bigint {
        return BigInt(this.ffi.symbols.splinter_get_signal_count(groupId));
    }
}

// --- Deno Implementation ---

class DenoSplinter implements SplinterStore {
    private dylib: any;

    constructor(libPath: string) {
        // @ts-ignore - Deno namespace might not exist in Bun type checks
        this.dylib = Deno.dlopen(libPath, {
            splinter_open: { parameters: ["buffer"], result: "i32" },
            splinter_close: { parameters: [], result: "void" },
            splinter_set: { parameters: ["buffer", "buffer", "usize"], result: "i32" },
            splinter_get: { parameters: ["buffer", "buffer", "usize", "buffer"], result: "i32" },
            splinter_get_epoch: { parameters: ["buffer"], result: "u64" },
            splinter_get_signal_count: { parameters: ["u8"], result: "u64" },
        });
    }

    open(name: string): boolean {
        return this.dylib.symbols.splinter_open(encoder.encode(name + "\0")) === 0;
    }

    close(): void {
        this.dylib.symbols.splinter_close();
    }

    set(key: string, value: string | Uint8Array): boolean {
        const data = typeof value === "string" ? encoder.encode(value) : value;
        return this.dylib.symbols.splinter_set(encoder.encode(key + "\0"), data, data.length) === 0;
    }

    get(key: string): Uint8Array | null {
        const maxLen = 4096;
        const buffer = new Uint8Array(maxLen);
        const outLen = new BigUint64Array(1);

        const rc = this.dylib.symbols.splinter_get(
            encoder.encode(key + "\0"),
            buffer,
            maxLen,
            new Uint8Array(outLen.buffer) // Deno passes pointers as Uint8Arrays
        );

        if (rc !== 0) return null;
        return buffer.slice(0, Number(outLen[0]));
    }

    getString(key: string): string | null {
        const data = this.get(key);
        return data ? decoder.decode(data) : null;
    }

    getEpoch(key: string): bigint {
        return BigInt(this.dylib.symbols.splinter_get_epoch(encoder.encode(key + "\0")));
    }

    getSignalCount(groupId: number): bigint {
        return BigInt(this.dylib.symbols.splinter_get_signal_count(groupId));
    }
}

// --- Auto-Sensing Factory ---

export class Splinter {
    static connect(busName: string, customLibPath?: string): SplinterStore {
        const libPath = customLibPath || `./${getLibFilename()}`;
        let store: SplinterStore;

        // Auto-detect the runtime environment
        const isBun = typeof process !== "undefined" && process.versions && process.versions.bun;
        // @ts-ignore
        const isDeno = typeof Deno !== "undefined";

        if (isBun) {
            store = new BunSplinter(libPath);
        } else if (isDeno) {
            store = new DenoSplinter(libPath);
        } else {
            throw new Error("Unsupported runtime. Splinter requires Bun or Deno.");
        }

        if (!store.open(busName)) {
            throw new Error(`Failed to attach to Splinter bus: ${busName}`);
        }

        return store;
    }
}
