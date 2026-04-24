#include "query_handler.h"
#include <cmath>
#include <iostream>
#include <cassert>

static int failures = 0;

#define ASSERT_NEAR(actual, expected, tol, msg)                          \
    do {                                                                  \
        double _a = (actual), _e = (expected), _t = (tol);              \
        if (std::abs(_a - _e) > _t) {                                    \
            std::cerr << "FAIL [" << msg << "]: got " << _a             \
                      << ", expected " << _e << " ±" << _t << "\n";     \
            ++failures;                                                   \
        } else {                                                          \
            std::cout << "PASS [" << msg << "]\n";                       \
        }                                                                 \
    } while (0)

#define ASSERT_THROWS(expr, ExcType, msg)                                \
    do {                                                                  \
        bool _threw = false;                                              \
        try { (expr); } catch (const ExcType&) { _threw = true; }       \
        if (!_threw) {                                                    \
            std::cerr << "FAIL [" << msg << "]: expected " #ExcType "\n";\
            ++failures;                                                   \
        } else {                                                          \
            std::cout << "PASS [" << msg << "]\n";                       \
        }                                                                 \
    } while (0)

int main()
{
    // ── Test 0: query before setRadar() must throw ────────────────────────────
    {
        QueryHandler h(50000.0, "./tiles/");
        ASSERT_THROWS(h.handle({ 1000.0, 270.0, 0.0 }), RadarNotSetError, "query before setRadar throws");
    }

    QueryHandler handler(50000.0, "./tiles/");
    bool ok = handler.setRadar(32.08, 34.76, 10.0); // Tel Aviv coast, 10 m MSL
    if (!ok) {
        std::cerr << "FAIL: setRadar() returned false — no tiles loaded. "
                     "Run tests from server/ so ./tiles/ resolves.\n";
        return 1;
    }

    // ── Test 1: sea target, expect ground elevation = 0 m ────────────────────
    {
        TargetResult r = handler.handle({ 1000.0, 270.0, 0.0 });
        ASSERT_NEAR(r.ground_elevation_m, 0.0, 2.0, "sea target ground elev ~0");
        ASSERT_NEAR(r.position.alt_m, 10.0, 0.1, "sea target alt = radar alt (0 elev angle)");
        ASSERT_NEAR(r.target_height_agl_m, 10.0, 2.0, "sea target AGL = radar alt - ground");
        ASSERT_NEAR(r.horizontal_range_m, 1000.0, 0.5, "sea target horiz range = slant range");
    }

    // ── Test 2: positive elevation angle lifts target above radar altitude ────
    {
        TargetResult r = handler.handle({ 1000.0, 270.0, 2.0 });
        double expected_alt = 10.0 + 1000.0 * std::sin(2.0 * M_PI / 180.0);
        ASSERT_NEAR(r.position.alt_m, expected_alt, 0.5, "elevated target alt MSL");
        ASSERT_NEAR(r.target_height_agl_m, r.position.alt_m - r.ground_elevation_m, 0.01,
                    "AGL = alt_msl - ground_elev");
    }

    // ── Test 3: validation — range out of bounds ──────────────────────────────
    ASSERT_THROWS(handler.handle({ -1.0, 0.0, 0.0 }),   ValidationError, "negative range rejected");
    ASSERT_THROWS(handler.handle({ 60000.0, 0.0, 0.0 }), ValidationError, "range > max rejected");

    // ── Test 4: validation — azimuth out of bounds ────────────────────────────
    ASSERT_THROWS(handler.handle({ 100.0, -1.0, 0.0 }),  ValidationError, "negative azimuth rejected");
    ASSERT_THROWS(handler.handle({ 100.0, 360.0, 0.0 }), ValidationError, "azimuth=360 rejected");

    // ── Test 5: validation — elevation out of bounds ──────────────────────────
    ASSERT_THROWS(handler.handle({ 100.0, 0.0, -11.0 }), ValidationError, "elevation < -10 rejected");
    ASSERT_THROWS(handler.handle({ 100.0, 0.0,  46.0 }), ValidationError, "elevation > 45 rejected");

    // ── Test 6: azimuth boundary values accepted ──────────────────────────────
    {
        TargetResult r = handler.handle({ 500.0, 0.0, 0.0 });
        ASSERT_NEAR(r.horizontal_range_m, 500.0, 0.5, "azimuth=0 accepted");
    }
    {
        TargetResult r = handler.handle({ 500.0, 359.9, 0.0 });
        ASSERT_NEAR(r.horizontal_range_m, 500.0, 0.5, "azimuth=359.9 accepted");
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << failures << " failure(s))\n";
    return failures;
}
