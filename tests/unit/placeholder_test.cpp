/**
 * @file placeholder_test.cpp
 * @brief Placeholder unit tests for basic band-math sanity checks.
 *
 * These tests verify fundamental invariants of the 8-band frequency model
 * (BandProcessor) so that CI can confirm the build and link pipeline works
 * before more comprehensive test suites are wired in.
 *
 * No external test framework is required — failures are reported via
 * std::abort() triggered by a failed assertion, which CTest treats as a
 * non-zero exit code (test failure).
 */

#include "../../native/src/render/BandProcessor.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace magnaundasoni;
using namespace magnaundasoni::BandProcessor;

// ---------------------------------------------------------------------------
// Tiny assertion helper that prints the test name before aborting.
// ---------------------------------------------------------------------------
static int g_passed = 0;
static int g_failed = 0;

#define MAGN_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failed; \
        } else { \
            ++g_passed; \
        } \
    } while (0)

#define MAGN_ASSERT_NEAR(a, b, eps) \
    MAGN_ASSERT(std::fabs((a) - (b)) <= (eps))

// ---------------------------------------------------------------------------
// Test: Band count and center frequencies
// ---------------------------------------------------------------------------
static void test_bandCenterFrequencies()
{
    // The canonical 8-band schema must cover 63 Hz – 8 kHz in octave steps.
    static constexpr float kExpected[8] = {
        63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
    };

    for (int i = 0; i < 8; ++i) {
        MAGN_ASSERT_NEAR(kBandCenterFrequencies[i], kExpected[i], 0.01f);
    }

    // Adjacent bands must be approximately one octave apart (ratio ≈ 2).
    for (int i = 1; i < 8; ++i) {
        float ratio = kBandCenterFrequencies[i] / kBandCenterFrequencies[i - 1];
        MAGN_ASSERT_NEAR(ratio, 2.0f, 0.05f);
    }
}

// ---------------------------------------------------------------------------
// Test: bandFill + bandSum — sum of unity gains equals number of bands
// ---------------------------------------------------------------------------
static void test_bandSumUnityGains()
{
    BandArray ones = bandFill(1.0f);
    float sum = bandSum(ones);
    // 8 bands × gain 1.0 = 8.0
    MAGN_ASSERT_NEAR(sum, 8.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: bandAdd is commutative and associative (basic check)
// ---------------------------------------------------------------------------
static void test_bandAddProperties()
{
    BandArray a = bandFill(0.5f);
    BandArray b = bandFill(0.3f);

    BandArray ab = bandAdd(a, b);
    BandArray ba = bandAdd(b, a);

    for (int i = 0; i < 8; ++i) {
        // Commutative: a + b == b + a
        MAGN_ASSERT_NEAR(ab[i], ba[i], 1e-6f);
        // Correct value: 0.5 + 0.3 = 0.8
        MAGN_ASSERT_NEAR(ab[i], 0.8f, 1e-5f);
    }
}

// ---------------------------------------------------------------------------
// Test: bandScale — scaling by 0.5 halves all elements
// ---------------------------------------------------------------------------
static void test_bandScale()
{
    BandArray a = bandFill(1.0f);
    BandArray half = bandScale(a, 0.5f);

    for (int i = 0; i < 8; ++i) {
        MAGN_ASSERT_NEAR(half[i], 0.5f, 1e-6f);
    }

    // Scaling by zero yields all zeros
    BandArray zero = bandScale(a, 0.0f);
    MAGN_ASSERT_NEAR(bandSum(zero), 0.0f, 1e-6f);
}

// ---------------------------------------------------------------------------
// Test: bandMultiply identity — multiplying by ones is a no-op
// ---------------------------------------------------------------------------
static void test_bandMultiplyIdentity()
{
    BandArray a    = {0.1f, 0.2f, 0.4f, 0.8f, 1.0f, 0.7f, 0.5f, 0.3f};
    BandArray ones = bandFill(1.0f);
    BandArray result = bandMultiply(a, ones);

    for (int i = 0; i < 8; ++i) {
        MAGN_ASSERT_NEAR(result[i], a[i], 1e-6f);
    }
}

// ---------------------------------------------------------------------------
// Test: dB round-trip — linearToDb(dbToLinear(x)) ≈ x
// ---------------------------------------------------------------------------
static void test_dbRoundTrip()
{
    static constexpr float kDbValues[] = { -60.0f, -20.0f, -6.0f, 0.0f, 6.0f, 20.0f };
    for (float db : kDbValues) {
        float roundTrip = linearToDb(dbToLinear(db));
        MAGN_ASSERT_NEAR(roundTrip, db, 1e-3f);
    }
}

// ---------------------------------------------------------------------------
// Test: bandClamp — all values clamped within [lo, hi]
// ---------------------------------------------------------------------------
static void test_bandClamp()
{
    BandArray a = {-1.0f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, -0.5f, 0.9f};
    BandArray clamped = bandClamp(a, 0.0f, 1.0f);

    for (int i = 0; i < 8; ++i) {
        MAGN_ASSERT(clamped[i] >= 0.0f);
        MAGN_ASSERT(clamped[i] <= 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Test: bandInterpolate — at t=0 returns a, at t=1 returns b
// ---------------------------------------------------------------------------
static void test_bandInterpolate()
{
    BandArray a = bandFill(0.0f);
    BandArray b = bandFill(1.0f);

    BandArray atZero = bandInterpolate(a, b, 0.0f);
    BandArray atOne  = bandInterpolate(a, b, 1.0f);
    BandArray atHalf = bandInterpolate(a, b, 0.5f);

    for (int i = 0; i < 8; ++i) {
        MAGN_ASSERT_NEAR(atZero[i], 0.0f, 1e-6f);
        MAGN_ASSERT_NEAR(atOne[i],  1.0f, 1e-6f);
        MAGN_ASSERT_NEAR(atHalf[i], 0.5f, 1e-6f);
    }
}

// ---------------------------------------------------------------------------
// Test: bandMax — maximum element is correctly identified
// ---------------------------------------------------------------------------
static void test_bandMax()
{
    BandArray a = {0.1f, 0.2f, 0.9f, 0.4f, 0.3f, 0.8f, 0.5f, 0.6f};
    float m = bandMax(a);
    MAGN_ASSERT_NEAR(m, 0.9f, 1e-6f);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_bandCenterFrequencies();
    test_bandSumUnityGains();
    test_bandAddProperties();
    test_bandScale();
    test_bandMultiplyIdentity();
    test_dbRoundTrip();
    test_bandClamp();
    test_bandInterpolate();
    test_bandMax();

    std::printf("Band math sanity: %d passed, %d failed\n", g_passed, g_failed);

    return (g_failed == 0) ? 0 : 1;
}
