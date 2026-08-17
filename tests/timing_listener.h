#pragma once

// GoogleTest event listener that writes per-test timings to a JSON report.
// The report is only written when the AR_TEST_REPORT environment variable is
// set (ctest's gtest_discover_tests spawns one process per test, so the
// report is produced by running the full suite binary directly, once).

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace TestUtil {

class TimingListener : public ::testing::EmptyTestEventListener {
public:
    void OnTestEnd(const ::testing::TestInfo& info) override
    {
        m_entries.push_back({ std::string(info.test_suite_name()) + "." + info.name(),
            info.result()->elapsed_time(),
            info.result()->Passed() });
    }

    void OnTestProgramEnd(const ::testing::UnitTest& unitTest) override
    {
        const char* pathEnv = std::getenv("AR_TEST_REPORT");
        if (nullptr == pathEnv)
            return;
        const std::string path = pathEnv;
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out.is_open())
            return;
        out << "{\n  \"total_time_ms\": " << unitTest.elapsed_time() << ",\n"
            << "  \"successful_tests\": " << unitTest.successful_test_count() << ",\n"
            << "  \"failed_tests\": " << unitTest.failed_test_count() << ",\n"
            << "  \"tests\": [\n";
        for (size_t i = 0; i < m_entries.size(); ++i) {
            out << "    {\"name\": \"" << m_entries[i].name << "\", "
                << "\"time_ms\": " << m_entries[i].timeMs << ", "
                << "\"passed\": " << (m_entries[i].passed ? "true" : "false") << "}"
                << (i + 1 < m_entries.size() ? "," : "") << "\n";
        }
        out << "  ]\n}\n";
        std::printf("[TimingListener] wrote %s\n", path.c_str());
    }

private:
    struct Entry {
        std::string name;
        long long timeMs;
        bool passed;
    };
    std::vector<Entry> m_entries;
};

inline void registerTimingListener()
{
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new TimingListener());
}

} // namespace TestUtil
