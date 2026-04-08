import { assertEquals, assertNotEquals } from "https://deno.land/std@0.224.0/assert/mod.ts";
import { Splinter, SPL_SLOT_TYPE } from "./splinter.ts";

const BUS_NAME = "splinter_debug";

Deno.test("Splinter: Basic Set/Get", () => {
    const store = Splinter.connect(BUS_NAME);
    
    const key = "test_key_01";
    const value = "Hello Splinter";
    
    const success = store.set(key, value);
    assertEquals(success, true, "Should successfully set key");
    
    const retrieved = store.getString(key);
    assertEquals(retrieved, value, "Value retrieved should match value set");
    
    store.close();
});

Deno.test("Splinter: Epoch Increment", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "epoch_trigger";
    
    const epoch1 = store.getEpoch(key);
    store.set(key, "data_v1");
    const epoch2 = store.getEpoch(key);
    
    assertNotEquals(epoch1, epoch2, "Epoch must change after a write operation");
    
    store.close();
});

Deno.test("Splinter: Named Types", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "lysis_target";
    
    store.set(key, "raw_input_text");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT); 
    store.close();
});

Deno.test("Splinter: Signal Group Monitoring", () => {
    const store = Splinter.connect(BUS_NAME);
    
    // Check signal group 2 (Incoming)
    const count = store.getSignalCount(2);
    
    // Note: If no one has pulsed group 2, this might be 0.
    // We just want to ensure the call doesn't crash and returns a bigint.
    assertEquals(typeof count, "bigint");
    
    store.close();
});

Deno.test("Splinter: Key Bumping With Signaling", () => {
    const store = Splinter.connect(BUS_NAME);

    const key = "bump_target";
    store.set(key, "bump test");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);
    store.setLabel(key, 0x4n);
    store.bumpSlot(key);

    store.close();
});

// Helper: generate a normalized random 768-d embedding
function makeFakeEmbedding(): Float32Array {
    const v = new Float32Array(768);
    let norm = 0;
    for (let i = 0; i < 768; i++) {
        v[i] = Math.random() * 2 - 1; // [-1, 1]
        norm += v[i] * v[i];
    }
    norm = Math.sqrt(norm);
    for (let i = 0; i < 768; i++) v[i] /= norm;
    return v;
}

Deno.test("Splinter: Set and Get Embedding round-trip", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_roundtrip_01";

    store.set(key, "The quick brown fox jumps over the lazy dog");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);

    const embedding = makeFakeEmbedding();
    const setOk = store.setEmbedding(key, embedding);
    assertEquals(setOk, true, "setEmbedding should return true on success");

    const retrieved = store.getEmbedding(key);
    assertNotEquals(retrieved, null, "getEmbedding should return a Float32Array, not null");

    assertEquals(retrieved!.length, 768, "Retrieved embedding must be exactly 768 dimensions");

    // Compare each component within float32 tolerance
    for (let i = 0; i < 768; i++) {
        const delta = Math.abs(retrieved![i] - embedding[i]);
        assertEquals(
            delta < 1e-6, true,
            `Component ${i} mismatch: stored ${embedding[i]}, got ${retrieved![i]}`
        );
    }

    store.close();
});

Deno.test("Splinter: Embedding is zeroed before first write", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_zero_check";

    // Fresh key — embedding slot should be all zeros
    store.set(key, "new slot, no embedding yet");
    const retrieved = store.getEmbedding(key);

    // getEmbedding may return null OR a zeroed array depending on impl —
    // both are acceptable for a slot that has never been embedded
    if (retrieved !== null) {
        assertEquals(retrieved.length, 768);
        const norm = retrieved.reduce((s, v) => s + v * v, 0);
        assertEquals(norm < 1e-9, true, "Pre-embedding slot should have a zero-norm vector");
    }

    store.close();
});

Deno.test("Splinter: Embedding update changes stored vector", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_update_01";

    store.set(key, "mutable embedding target");
    store.setNamedType(key, SPL_SLOT_TYPE.VARTEXT);

    const first = makeFakeEmbedding();
    const second = makeFakeEmbedding();

    store.setEmbedding(key, first);
    store.setEmbedding(key, second);

    const retrieved = store.getEmbedding(key);
    assertNotEquals(retrieved, null);

    // Verify it reflects second, not first
    let firstDelta = 0, secondDelta = 0;
    for (let i = 0; i < 768; i++) {
        firstDelta  += Math.abs(retrieved![i] - first[i]);
        secondDelta += Math.abs(retrieved![i] - second[i]);
    }

    assertEquals(
        secondDelta < firstDelta, true,
        "Stored embedding should match the second write, not the first"
    );

    store.close();
});

Deno.test("Splinter: setEmbedding rejects wrong dimension", () => {
    const store = Splinter.connect(BUS_NAME);
    const key = "embed_dim_guard";

    store.set(key, "dimension guard test");

    // DenoSplinter.setEmbedding throws on wrong size — verify the guard
    let threw = false;
    try {
        store.setEmbedding(key, new Float32Array(512));
    } catch (e) {
        threw = true;
        assertEquals(
            (e as Error).message.includes("768"),
            true,
            "Error message should mention the expected 768 dimensions"
        );
    }

    assertEquals(threw, true, "setEmbedding should throw on non-768 input");

    store.close();
});

Deno.test("Splinter: getEmbedding returns null for missing key", () => {
    const store = Splinter.connect(BUS_NAME);

    const result = store.getEmbedding("key_that_does_not_exist_xyz");
    assertEquals(result, null, "getEmbedding on a missing key should return null");

    store.close();
});

Deno.test("Splinter: Append extends value and returns correct length", () => {
    const store = Splinter.connect(BUS_NAME);

    const key = "append_test_01";
    const initial = "dog";
    const suffix  = "leash";

    store.set(key, initial);

    const newLen = store.append(key, suffix);

    assertNotEquals(newLen, null, "append should return a length, not null");
    assertEquals(newLen, BigInt(initial.length + suffix.length),
        `Expected new length ${initial.length + suffix.length}, got ${newLen}`);

    // Verify the actual stored content while we're here
    const result = store.getString(key);
    assertEquals(result, initial + suffix, "Stored value should be concatenation of both writes");

    store.close();
});

Deno.test("Splinter: Append returns null for missing key", () => {
    const store = Splinter.connect(BUS_NAME);

    const result = store.append("key_that_does_not_exist_append", "data");
    assertEquals(result, null, "append on missing key should return null");

    store.close();
});