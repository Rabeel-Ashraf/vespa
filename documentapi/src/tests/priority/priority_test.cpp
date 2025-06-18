// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/documentapi/messagebus/priority.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/test/test_path.h>
#include <fstream>
#include <algorithm>

using namespace documentapi;

TEST(PriorityTest, priority_test) {

    std::vector<int32_t> expected;
    expected.push_back(Priority::PRI_HIGHEST);
    expected.push_back(Priority::PRI_VERY_HIGH);
    expected.push_back(Priority::PRI_HIGH_1);
    expected.push_back(Priority::PRI_HIGH_2);
    expected.push_back(Priority::PRI_HIGH_3);
    expected.push_back(Priority::PRI_NORMAL_1);
    expected.push_back(Priority::PRI_NORMAL_2);
    expected.push_back(Priority::PRI_NORMAL_3);
    expected.push_back(Priority::PRI_NORMAL_4);
    expected.push_back(Priority::PRI_NORMAL_5);
    expected.push_back(Priority::PRI_NORMAL_6);
    expected.push_back(Priority::PRI_LOW_1);
    expected.push_back(Priority::PRI_LOW_2);
    expected.push_back(Priority::PRI_LOW_3);
    expected.push_back(Priority::PRI_VERY_LOW);
    expected.push_back(Priority::PRI_LOWEST);

    std::ifstream in;
    in.open(TEST_PATH("../../../test/crosslanguagefiles/5.1-Priority.txt").c_str());
    ASSERT_TRUE(in.good());
    while (in) {
        std::string str;
        in >> str;
        if (str.empty()) {
            continue;
        }
        size_t pos = str.find(":");
        ASSERT_TRUE(pos != std::string::npos);
        int32_t pri = atoi(str.substr(pos + 1).c_str());
        ASSERT_EQ(Priority::getPriority(str.substr(0, pos)), pri);

        std::vector<int32_t>::iterator it =
            std::find(expected.begin(), expected.end(), pri);
        ASSERT_TRUE(it != expected.end());
        expected.erase(it);
    }
    ASSERT_TRUE(expected.empty());
}

GTEST_MAIN_RUN_ALL_TESTS()
