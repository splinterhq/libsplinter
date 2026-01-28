/**
 * TS Lib to make access to Splinter more convenient from Deno.
 * License: MIT
 */
import { Libsplinter } from "./splinter_deno_ffi.ts";
import { SplinterHeaderSnapshot, SplinterSlotSnapshot } from "./ffi_types.ts";

export class Splinter {
  private isOpen = false;

  /**
   * Creates and initializes a new splinter store.
   * @param nameOrPath The name of the shared memory object or path to the file
   * @param slots The total number of key-value slots to allocate
   * @param maxValueSize The maximum size in bytes for any single value
   * @throws Error if creation fails (e.g., store already exists)
   */
  static create(nameOrPath: string, slots: number, maxValueSize: number): Splinter {
    const nameBuffer = new TextEncoder().encode(nameOrPath + '\0');
    const result = Libsplinter.symbols.splinter_create(nameBuffer, BigInt(slots), BigInt(maxValueSize));
    
    if (result !== 0) {
      throw new Error(`Failed to create splinter store: ${nameOrPath}`);
    }
    
    const splinter = new Splinter();
    splinter.isOpen = true;
    return splinter;
  }

  /**
   * Opens an existing splinter store.
   * @param nameOrPath The name of the shared memory object or path to the file
   * @throws Error if opening fails (e.g., store does not exist)
   */
  static open(nameOrPath: string): Splinter {
    const nameBuffer = new TextEncoder().encode(nameOrPath + '\0');
    const result = Libsplinter.symbols.splinter_open(nameBuffer);
    
    if (result !== 0) {
      throw new Error(`Failed to open splinter store: ${nameOrPath}`);
    }
    
    const splinter = new Splinter();
    splinter.isOpen = true;
    return splinter;
  }

  /**
   * Opens an existing splinter store, or creates it if it does not exist.
   * @param nameOrPath The name of the shared memory object or path to the file
   * @param slots The total number of key-value slots if creating
   * @param maxValueSize The maximum value size in bytes if creating
   * @throws Error if operation fails
   */
  static openOrCreate(nameOrPath: string, slots: number, maxValueSize: number): Splinter {
    const nameBuffer = new TextEncoder().encode(nameOrPath + '\0');
    const result = Libsplinter.symbols.splinter_open_or_create(nameBuffer, BigInt(slots), BigInt(maxValueSize));
    
    if (result !== 0) {
      throw new Error(`Failed to open or create splinter store: ${nameOrPath}`);
    }
    
    const splinter = new Splinter();
    splinter.isOpen = true;
    return splinter;
  }

  /**
   * Creates a new splinter store, or opens it if it already exists.
   * @param nameOrPath The name of the shared memory object or path to the file
   * @param slots The total number of key-value slots if creating
   * @param maxValueSize The maximum value size in bytes if creating
   * @throws Error if operation fails
   */
  static createOrOpen(nameOrPath: string, slots: number, maxValueSize: number): Splinter {
    const nameBuffer = new TextEncoder().encode(nameOrPath + '\0');
    const result = Libsplinter.symbols.splinter_create_or_open(nameBuffer, BigInt(slots), BigInt(maxValueSize));
    
    if (result !== 0) {
      throw new Error(`Failed to create or open splinter store: ${nameOrPath}`);
    }
    
    const splinter = new Splinter();
    splinter.isOpen = true;
    return splinter;
  }

  private constructor() {}

  private checkOpen(): void {
    if (!this.isOpen) {
      throw new Error("Splinter store is not open");
    }
  }

  /**
   * Toggle auto-scrub mode on or off
   * @param mode integer value (0 or 1) of what it should be
   * @retuns void
   */
  setAV(mode: number) : void {
    if (! this.isOpen) {
      throw new Error("You must be connected to set AV mode");
    }
    Libsplinter.symbols.splinter_set_av(mode);
    return;
  }

  /**
   * Get the current auto-scrub mode of the connected bus
   * @returns number
   * @throws if not connected
   */
  getAV() : number {
    if (! this.isOpen) {
      throw  new Error("You must be connected to get AV status");
    }
    const ret = Libsplinter.symbols.splinter_get_av();
    if (ret < 0) {
      throw new Error("Error getting AV status");
    } 
    return ret;
  }

  /**
   * Sets or updates a key-value pair in the store.
   * @param key The key string
   * @param value The value data (string, Uint8Array, or any serializable object)
   * @throws Error if operation fails (e.g., store is full)
   */
  set(key: string, value: string | Uint8Array | unknown): void {
    this.checkOpen();
    
    const keyBuffer = new TextEncoder().encode(key + '\0');
    let valueData: Uint8Array;
    
    if (typeof value === 'string') {
      valueData = new TextEncoder().encode(value);
    } else if (value instanceof Uint8Array) {
      valueData = value;
    } else {
      // Serialize as JSON for other types
      valueData = new TextEncoder().encode(JSON.stringify(value));
    }
    
    const result = Libsplinter.symbols.splinter_set(
      keyBuffer,
      Deno.UnsafePointer.of(<BufferSource> valueData),
      BigInt(valueData.length)
    );
    
    if (result !== 0) {
      throw new Error(`Failed to set key: ${key}`);
    }
  }

  /**
   * Removes a key-value pair from the store.
   * @param key The key to remove
   * @returns The length of the deleted value, or null if key not found
   * @throws Error if operation fails
   */
  unset(key: string): number | null {
    this.checkOpen();
    
    const keyBuffer = new TextEncoder().encode(key + '\0');
    const result = Libsplinter.symbols.splinter_unset(keyBuffer);
    
    if (result === -2) {
      throw new Error("Invalid key or store not open");
    }
    
    return result === -1 ? null : result;
  }

  /**
   * Retrieves the raw bytes for a key.
   * @param key The key to look up
   * @returns The raw value data as Uint8Array, or null if key not found
   * @throws Error if operation fails
   */
  getRaw(key: string): Uint8Array | null {
    this.checkOpen();
    
    const keyBuffer = new TextEncoder().encode(key + '\0');
    const outSizePtr = new BigUint64Array(1);
    
    // First call to get the size
    let result = Libsplinter.symbols.splinter_get(
      keyBuffer,
      null,
      BigInt(0),
      Deno.UnsafePointer.of(outSizePtr)
    );
    
    if (result !== 0) {
      return null; // Key not found or other error
    }
    
    const size = Number(outSizePtr[0]);
    if (size === 0) {
      return new Uint8Array(0);
    }
    
    // Second call to get the actual data
    const buffer = new Uint8Array(size);
    result = Libsplinter.symbols.splinter_get(
      keyBuffer,
      Deno.UnsafePointer.of(buffer),
      BigInt(size),
      Deno.UnsafePointer.of(outSizePtr)
    );
    
    if (result !== 0) {
      throw new Error(`Failed to get value for key: ${key}`);
    }
    
    return buffer;
  }

  /**
   * Retrieves a value as a string.
   * @param key The key to look up
   * @returns The value as a UTF-8 string, or null if key not found
   */
  getString(key: string): string | null {
    const data = this.getRaw(key);
    return data ? new TextDecoder().decode(data) : null;
  }

  /**
   * Retrieves a value and parses it as JSON.
   * @param key The key to look up
   * @returns The parsed JSON value, or null if key not found
   * @throws Error if the value is not valid JSON
   */
  getJSON<T = unknown>(key: string): T | null {
    const str = this.getString(key);
    return str ? JSON.parse(str) : null;
  }

  /**
   * Lists all keys currently in the store.
   * @param maxKeys Maximum number of keys to return (default: 1000)
   * @returns Array of key strings
   * @throws Error if operation fails
   */
  list(maxKeys = 1000): string[] {
    this.checkOpen();
    
    const outKeysPtr = new BigUint64Array(maxKeys);
    const outCountPtr = new BigUint64Array(1);
    
    const result = Libsplinter.symbols.splinter_list(
      Deno.UnsafePointer.of(outKeysPtr),
      BigInt(maxKeys),
      Deno.UnsafePointer.of(outCountPtr)
    );
    
    if (result !== 0) {
      throw new Error("Failed to list keys");
    }
    
    const count = Number(outCountPtr[0]);
    const keys: string[] = [];
    
    for (let i = 0; i < count; i++) {
      const strPtr = Deno.UnsafePointer.create(outKeysPtr[i]);
      if (strPtr === null) {
        throw new Error(`Invalid pointer at index ${i}`);
      }
      const cString = new Deno.UnsafePointerView(strPtr).getCString();
      keys.push(cString);
    }
    
    return keys;
  }

  /**
   * Get a snapshot of a key-slot header by key name
   * @param key Name of the key owning the slot to snapshot
   * @returns SplinterSlotSnapshot<>
   * @throws on Splinter error or if disconnected
   */
  getSlotSnapshot(key: string): SplinterSlotSnapshot {
    this.checkOpen();
    // Must mirror whatever is in splinter.h
    const KEY_MAX = 64;
    
    // Calculate struct size:
    // uint64_t (8) + uint64_t (8) + uint32_t (4) + uint32_t (4) + char[KEY_MAX]
    const STRUCT_SIZE = 8 + 8 + 4 + 4 + KEY_MAX;
    
    const snapshotBuffer = new Uint8Array(STRUCT_SIZE);
    const snapshotPtr = Deno.UnsafePointer.of(snapshotBuffer);
    
    // Convert the key string to a null-terminated C string buffer
    const encoder = new TextEncoder();
    const keyBytes = encoder.encode(key + '\0'); // Add null terminator
    const keyBuffer = new Uint8Array(keyBytes);
    
    const result = Libsplinter.symbols.splinter_get_slot_snapshot(
      keyBuffer,
      snapshotPtr
    );
    
    if (result !== 0) {
      throw new Error(`splinter_get_slot_snapshot failed with code: ${result}`);
    }
    
    // Create a DataView to read the numeric fields
    const view = new DataView(snapshotBuffer.buffer);
    
    // Read each field according to the C struct layout
    let offset = 0;
    const hash = view.getBigUint64(offset, true);
    offset += 8;
    const epoch = view.getBigUint64(offset, true);
    offset += 8;
    const val_off = view.getUint32(offset, true);
    offset += 4;
    const val_len = view.getUint32(offset, true);
    offset += 4;
    
    // Read the key string from the char array
    // Find the null terminator to get the actual string length
    let keyEndIndex = offset;
    while (keyEndIndex < snapshotBuffer.length && snapshotBuffer[keyEndIndex] !== 0) {
      keyEndIndex++;
    }
    
    // Decode the key string (excluding the null terminator)
    const decoder = new TextDecoder();
    const keyString = decoder.decode(snapshotBuffer.slice(offset, keyEndIndex));
    
    // Return the snapshot as a typed object
    return {
      hash,
      epoch,
      val_off,
      val_len,
      key: keyString
    };
  }

  /**
   * Get a snapshot of the atomic bus status and configuration structire
   * @returns SplinterHeaderSnapshot<>
   * @throws on Splinter error or if disconnected
   */
  getBusHeaderSnapshot(): SplinterHeaderSnapshot {
    this.checkOpen();
    // Calculate the size of the C struct
    // uint32_t (4 bytes) * 4 + uint64_t (8 bytes) * 3 + uint32_t (4 bytes) * 1
    // = 16 + 24 + 4 = 44 bytes
    const STRUCT_SIZE = 44;
    const buffer = new Uint8Array(STRUCT_SIZE);
    const ptr = Deno.UnsafePointer.of(buffer);
    const result = Libsplinter.symbols.splinter_get_header_snapshot(ptr);
    
    if (result !== 0) {
      throw new Error(`splinter_get_header_snapshot failed with code: ${result}`);
    }
    
    // Create a DataView to read the struct fields
    const view = new DataView(buffer.buffer);
    
    // Read each field according to the C struct layout
    let offset = 0;
    const magic = view.getUint32(offset, true);
    offset += 4;
    const version = view.getUint32(offset, true);
    offset += 4;
    const slots = view.getUint32(offset, true);
    offset += 4;
    const max_val_sz = view.getUint32(offset, true);
    offset += 4;
    const epoch = view.getBigUint64(offset, true);
    offset += 8;
    const auto_scrub = view.getUint32(offset, true);
    offset += 4;
    const parse_failures = view.getBigUint64(offset, true);
    offset += 8;
    const last_failure_epoch = view.getBigUint64(offset, true);
    
    // Return the snapshot as a typed object
    return {
      magic,
      version,
      slots,
      max_val_sz,
      epoch,
      auto_scrub,
      parse_failures,
      last_failure_epoch
    };
  }

  /**
   * Waits for a key's value to be changed.
   * @param key The key to monitor for changes
   * @param timeoutMs The maximum time to wait in milliseconds
   * @returns true if the value changed, false on timeout
   * @throws Error if the key doesn't exist or other error occurs
   */
  poll(key: string, timeoutMs: number): boolean {
    this.checkOpen();
    
    const keyBuffer = new TextEncoder().encode(key + '\0');
    const result = Libsplinter.symbols.splinter_poll(keyBuffer, BigInt(timeoutMs));
    
    if (result === 0) {
      return true; // Value changed
    } else if (result === -1) {
      return false; // Timeout or key doesn't exist
    } else if (result === -2) {
      return false; // EAGAIN (hot write)
    } else {
      throw new Error(`Poll failed for key: ${key}`);
    }
  }

  /**
   * Closes the splinter store and unmaps the shared memory region.
   * After calling this, the instance cannot be used anymore.
   */
  close(): void {
    if (this.isOpen) {
      Libsplinter.symbols.splinter_close();
      this.isOpen = false;
    }
  }

  /**
   * Checks if the store is currently open.
   */
  get opened(): boolean {
    return this.isOpen;
  }
}