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
