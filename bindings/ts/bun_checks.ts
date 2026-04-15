/**
 * Bun test suite for libsplinter FFI bindings
 * Run with: bun test bun_checks.ts
 */
import { test, expect } from "bun:test";
import { Splinter, SPL_SLOT_TYPE } from "./splinter.ts";

const BUS_NAME = "splinter_debug";

function makeFakeEmbedding(): Float32Array {
    const v = new Float32Array(768);
    let norm = 0;
    for (let i = 0; i < 768; i++) {
        v[i] = Math.random() * 2 - 1;
        norm += v[i] * v[i];
    }
    norm = Math.sqrt(norm);
    for (let i = 0; i < 768; i++) v[i] /= norm;
    return v;
}

test("Splinter: Basic Set/Get", () => {
    const store = Splinter.connect(BUS_NAME);

    const key = "test_key_01";
    const value = "Hello Splinter";

    expect(store.set(key, value)).toBe(true);
    expect(store.getString(key)).toBe(value);

    store.close();
});

test("Splinter: Epoch Increment", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "epoch_trigger";

    const epoch1 = store.getEpoch(key);
    store.set(key, "data_v1");
    const epoch2 = store.getEpoch(key);

    expect(epoch1).not.toBe(epoch2);

    store.close();
});

test("Splinter: Named Types", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "lysis_target";

    store.set(key, "raw_input_text");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);

    store.close();
});

test("Splinter: Signal Group Monitoring", () => {
    const store = Splinter.connect(BUS_NAME);

    const count = store.getSignalCount(2);
    expect(typeof count).toBe("bigint");

    store.close();
});

test("Splinter: Key Bumping With Signaling", () => {
    const store = Splinter.connect(BUS_NAME);

    const key = "bump_target";
    store.set(key, "bump test");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);
    store.setLabel(key, 0x4n);
    store.bumpSlot(key);

    store.close();
});

test("Splinter: Append extends value and returns correct length", () => {
    const store = Splinter.connect(BUS_NAME);

    const key = "append_test_01";
    const initial = "dog";
    const suffix = "leash";

    store.set(key, initial);

    const newLen = store.append(key, suffix);

    expect(newLen).not.toBeNull();
    expect(newLen).toBe(BigInt(initial.length + suffix.length));
    expect(store.getString(key)).toBe(initial + suffix);

    store.close();
});

test("Splinter: Append returns null for missing key", () => {
    const store = Splinter.connect(BUS_NAME);

    expect(store.append("key_that_does_not_exist_append", "data")).toBeNull();

    store.close();
});

test("Splinter: Set and Get Embedding round-trip", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_roundtrip_01";

    store.set(key, "The quick brown fox jumps over the lazy dog");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);

    const embedding = makeFakeEmbedding();
    expect(store.setEmbedding(key, embedding)).toBe(true);

    const retrieved = store.getEmbedding(key);
    expect(retrieved).not.toBeNull();
    expect(retrieved!.length).toBe(768);

    for (let i = 0; i < 768; i++) {
        expect(Math.abs(retrieved![i] - embedding[i])).toBeLessThan(1e-6);
    }

    store.close();
});

test("Splinter: Embedding is zeroed before first write", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_zero_check";

    store.set(key, "new slot, no embedding yet");
    const retrieved = store.getEmbedding(key);

    if (retrieved !== null) {
        expect(retrieved.length).toBe(768);
        const norm = retrieved.reduce((s, v) => s + v * v, 0);
        expect(norm).toBeLessThan(1e-9);
    }

    store.close();
});

test("Splinter: Embedding update changes stored vector", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_update_01";

    store.set(key, "mutable embedding target");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);

    const first = makeFakeEmbedding();
    const second = makeFakeEmbedding();

    store.setEmbedding(key, first);
    store.setEmbedding(key, second);

    const retrieved = store.getEmbedding(key);
    expect(retrieved).not.toBeNull();

    let firstDelta = 0, secondDelta = 0;
    for (let i = 0; i < 768; i++) {
        firstDelta  += Math.abs(retrieved![i] - first[i]);
        secondDelta += Math.abs(retrieved![i] - second[i]);
    }

    expect(secondDelta).toBeLessThan(firstDelta);

    store.close();
});

test("Splinter: setEmbedding rejects wrong dimension", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_dim_guard";

    store.set(key, "dimension guard test");

    expect(() => store.setEmbedding(key, new Float32Array(512))).toThrow("768");

    store.close();
});

test("Splinter: getEmbedding returns null for missing key", () => {
    const store = Splinter.connect(BUS_NAME);

    expect(store.getEmbedding("key_that_does_not_exist_xyz")).toBeNull();

    store.close();
});

test("Splinter: list() yields all set keys with correct values", () => {
    const store = Splinter.connect(BUS_NAME);

    const fixtures: Record<string, string> = {
        list_key_alpha: "value_alpha",
        list_key_beta:  "value_beta",
        list_key_gamma: "value_gamma",
    };

    for (const [k, v] of Object.entries(fixtures)) {
        expect(store.set(k, v)).toBe(true);
    }

    const seen: Record<string, string> = {};
    for (const entry of store.list()) {
        if (entry.key in fixtures) {
            seen[entry.key] = new TextDecoder().decode(entry.value);
        }
    }

    for (const [k, v] of Object.entries(fixtures)) {
        expect(seen[k]).toBe(v);
    }

    store.close();
});

test("Splinter: list() is iterable like a standard DB cursor", () => {
    const store = Splinter.connect(BUS_NAME);

    store.set("cursor_key_01", "cursor_val_01");
    store.set("cursor_key_02", "cursor_val_02");

    // Collect via spread — the canonical TS iterator pattern
    const entries = [...store.list()];

    expect(entries.length).toBeGreaterThan(0);

    for (const entry of entries) {
        expect(typeof entry.key).toBe("string");
        expect(entry.value).toBeInstanceOf(Uint8Array);
    }

    // Verify the iterator is re-entrant (fresh iterator each call)
    const second = [...store.list()];
    expect(second.length).toBe(entries.length);

    store.close();
});

test("Splinter: list() entry values match individual get()", () => {
    const store = Splinter.connect(BUS_NAME);

    store.set("list_verify_01", "check_value_01");
    store.set("list_verify_02", "check_value_02");

    for (const { key, value } of store.list()) {
        const direct = store.get(key);
        expect(direct).not.toBeNull();
        expect(value.every((b, i) => b === direct![i])).toBe(true);
    }

    store.close();
});
