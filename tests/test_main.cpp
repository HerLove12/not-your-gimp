#include <gtest/gtest.h>

// Example test
TEST(SampleTest, Addition) {
    EXPECT_EQ(2 + 2, 4);
}

// You can add more tests here
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
