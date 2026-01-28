/**
 * Test suite for Splinter class (not yet comprehensive, but getting there!)
 * Could use more comprehensive (assertThrows style) tests, too.
 * Run with: deno test --allow-ffi
 */
import { assertEquals, assert, assertLessOrEqual } from "https://deno.land/std@0.208.0/assert/mod.ts";
import { Splinter } from "./splinter.class.ts";
import { assertGreaterOrEqual } from "https://deno.land/std@0.208.0/assert/assert_greater_or_equal.ts";

const TEST_STORE = "splinter_debug";
const TEST_SLOTS = 100;
const TEST_MAX_VALUE_SIZE = 1024;

// Helper to clean up test stores
function cleanup() {
  try {
    // in case persistent
    Deno.removeSync(TEST_STORE);
  } catch {
    // Ignore if file doesn't exist
  }
}

// cropen / set / get / 
Deno.test({
  name: "Splinter CRUD Battery (4 Operations / 3 Tests)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    assert(splinter.opened);
    splinter.set("__test", "stage:1");
    const stageOne = splinter.getString("__test");
    assertEquals(stageOne, "stage:1");
    const removed = splinter.unset("__test");
    assertEquals(removed, 7);
    splinter.close();  
  },
});

// get splinter bus header 
Deno.test({
  name: "Get global atomic bus config snapshot - returns header information (2 Operations / 5 Tests)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    
    const snapshot = splinter.getBusHeaderSnapshot();
    assert(snapshot.magic !== undefined);
    assert(snapshot.version !== undefined);
    assertEquals(snapshot.slots, TEST_SLOTS);
    assertEquals(snapshot.max_val_sz, TEST_MAX_VALUE_SIZE);
    assert(snapshot.epoch !== undefined);
    
    splinter.close();
    cleanup();
  },
});

// Set a key and get the slot metadata
Deno.test({
  name: "Get slot metadata - returns slot metadata (2 Operations / 4 Tests)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    
    splinter.set("__test", "stage:2");
    const snapshot = splinter.getSlotSnapshot("__test");
    
    assert(snapshot.hash !== undefined);
    assert(snapshot.epoch !== undefined);
    assertEquals(snapshot.key, "__test");
    assert(snapshot.val_len > 0);
    
    splinter.close();
    cleanup();
  },
});

Deno.test({
  name: "Let a watch expire - returns false on timeout (2 operations, 1 test)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    
    splinter.set("__test", "Stage:3");
    const changed = splinter.poll("__test", 100); // 100ms standard
    assertEquals(changed, false);
    
    splinter.close();
    cleanup();
  },
});

Deno.test({
  name: "List keys in a store (4 operations, 1 test)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    
    splinter.set("__test", "Stage:4");
    splinter.set("foo", "Stage:4");
    const keys = splinter.list(1024);
    assertEquals(keys.length, 2);

    splinter.close();
    cleanup();
  }
});

// Monkeying around with auto_scrub while doing stuff
Deno.test({
  name: "Flip AV/Scrub Mode (9 Operations / 7 Tests)",
  fn: () => {
    cleanup();
    const splinter = Splinter.createOrOpen(TEST_STORE, TEST_SLOTS, TEST_MAX_VALUE_SIZE);
    const oldav = splinter.getAV();
    assertLessOrEqual(oldav, 1);
    assertGreaterOrEqual(oldav, 0);

    splinter.set("__test", "stage:5");
    const stageFive = splinter.getString("__test");
    assertEquals(stageFive, "stage:5");
    
    splinter.setAV(0);
    assertEquals(splinter.getAV(), 0);
    splinter.set("__test", "stage:6");
    const stageSix = splinter.getString("__test");
    assertEquals(stageSix, "stage:6");
    splinter.setAV(oldav);
    assertEquals(splinter.getAV(), oldav);

    const removed = splinter.unset("__test");
    assertEquals(removed, 7);
    
    splinter.close();

    cleanup();  
  },
});
