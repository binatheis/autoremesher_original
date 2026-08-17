#include <gtest/gtest.h>

#include "timing_listener.h"

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    TestUtil::registerTimingListener();
    return RUN_ALL_TESTS();
}
