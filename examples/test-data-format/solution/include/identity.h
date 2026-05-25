#pragma once

// Empty student API. The test-data-format example exists purely to exercise
// the TestData binary format end-to-end through the runner subprocess: the
// teacher's main.cpp reads keys from input, writes them back to output, and
// the verify side checks equality. No "student algorithm" is involved.
namespace student {
    void noop();
}
