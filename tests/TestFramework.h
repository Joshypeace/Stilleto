#pragma once

// Minimal self-contained test harness (no external dependency). Each test registers itself;
// CHECK records failures without aborting, REQUIRE aborts the current test.

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace stilleto::test
{
struct TestCase
{
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount()
{
    static int failures = 0;
    return failures;
}

struct RequireFailed {};

struct Registrar
{
    Registrar (const char* name, std::function<void()> body) { registry().push_back ({ name, std::move (body) }); }
};

inline void reportFailure (const char* file, int line, const std::string& message)
{
    ++failureCount();
    std::printf ("    FAILED %s(%d): %s\n", file, line, message.c_str());
}

inline int runAll (const char* filter = nullptr)
{
    int failedTests = 0, ran = 0;

    for (const auto& t : registry())
    {
        if (filter != nullptr && std::string (t.name).find (filter) == std::string::npos)
            continue;

        ++ran;
        const int before = failureCount();
        std::printf ("[ RUN  ] %s\n", t.name);

        try
        {
            t.body();
        }
        catch (const RequireFailed&) {}
        catch (const std::exception& e)
        {
            reportFailure (__FILE__, __LINE__, std::string ("unexpected exception: ") + e.what());
        }

        const bool ok = failureCount() == before;
        std::printf ("[ %s ] %s\n", ok ? " OK " : "FAIL", t.name);
        if (! ok) ++failedTests;
    }

    std::printf ("\n%d test(s) run, %d failed\n", ran, failedTests);
    return failedTests == 0 ? 0 : 1;
}
} // namespace stilleto::test

#define STILLETO_CONCAT_INNER(a, b) a##b
#define STILLETO_CONCAT(a, b) STILLETO_CONCAT_INNER (a, b)

#define TEST_CASE(name)                                                                           \
    static void STILLETO_CONCAT (testBody_, __LINE__)();                                          \
    static ::stilleto::test::Registrar STILLETO_CONCAT (testReg_, __LINE__) (name, &STILLETO_CONCAT (testBody_, __LINE__)); \
    static void STILLETO_CONCAT (testBody_, __LINE__)()

#define CHECK(expr)                                                                               \
    do { if (! (expr)) ::stilleto::test::reportFailure (__FILE__, __LINE__, #expr); } while (false)

#define REQUIRE(expr)                                                                             \
    do { if (! (expr)) { ::stilleto::test::reportFailure (__FILE__, __LINE__, #expr);             \
                         throw ::stilleto::test::RequireFailed {}; } } while (false)

#define CHECK_EQ(a, b)                                                                            \
    do { const auto& va_ = (a); const auto& vb_ = (b);                                            \
         if (! (va_ == vb_)) { std::ostringstream os_; os_ << #a << " == " << #b                  \
                               << "\n      actual:   [" << va_ << "]\n      expected: [" << vb_ << "]"; \
             ::stilleto::test::reportFailure (__FILE__, __LINE__, os_.str()); } } while (false)

#define CHECK_NEAR(a, b, tol)                                                                     \
    do { const double va_ = (a), vb_ = (b);                                                       \
         if (std::abs (va_ - vb_) > (tol)) { std::ostringstream os_;                              \
             os_ << #a << " ~= " << #b << " (" << va_ << " vs " << vb_ << ")";                    \
             ::stilleto::test::reportFailure (__FILE__, __LINE__, os_.str()); } } while (false)
