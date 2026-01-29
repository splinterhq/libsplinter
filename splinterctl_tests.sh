#!/bin/sh

# This little scripts tests the splinterctl *interface* to make
# sure we don't break the CLI. It doesn't (and shouldn't) be relied
# on to make sure the underlying library works; that's what the unit
# tests are for. For instance, we don't check to make sure the key
# we get is the key we set *because extensive unit tests already
# did that*, so we're just testing the "workflow" UX.

TEST_STORE="${$}_splinterctl_test"

tests_run=0
tests_passed=0
tests_failed=0

fail()
{
	echo "FAIL: ${@}"
	tests_failed=$(($tests_failed + 1))
	# Could exit on any error here if needed
}

pass()
{
	tests_passed=$(($tests_passed + 1))
}

test()
{
	echo "TEST: ${@}"
	tests_run=$(($tests_run + 1))
}

report()
{
	echo "run: ${tests_run} | passed: ${tests_passed} | failed ${tests_failed}"
	[ $tests_run != $tests_passed ] && {
		echo "${tests_failed} tests failed, exiting abnormally."
		# leave the store just in case the CLI introduced a state issue
		exit 1
	}

	echo "Tests passed. Exiting normally."
	# /dev/shm will get very cluttered if you delete the next line:
	rm -f /dev/shm/${TEST_STORE}
	exit 0
}

test "Initialize store"
./splinterctl init $TEST_STORE || fail "Could not initialize store."
pass

test "Set a key"
./splinterctl --use $TEST_STORE set test_key test_value || fail "Could not set a key."
pass

test "Get a key"
./splinterctl --use $TEST_STORE get test_key || fail "Could not get a key."
pass

test "Get key metadata"
./splinterctl --use $TEST_STORE head test_key || fail "Could not get key metadata."
pass

test "List keys"
./splinterctl --use $TEST_STORE list || fail "Could not list keys"
pass

test "Type name a key"
./splinterctl --use $TEST_STORE type test_key vartext || fail "Could not type test_key as vartext"
pass

test "Get key type"
./splinterctl --use $TEST_STORE type test_key || fail "Could not get type of test_key"
pass

test "Unset a key"
./splinterctl --use $TEST_STORE unset test_key || fail "Could not unset test key"
pass

test "Get global config"
./splinterctl --use $TEST_STORE config || fail "Could not get global config."
pass

test "Set a global config flag"
./splinterctl --use $TEST_STORE config av 1 || fail "Could not set global config flag"
pass

test "Export store metadata"
./splinterctl --use $TEST_STORE export || fail "Could not export JSON"
pass

report
