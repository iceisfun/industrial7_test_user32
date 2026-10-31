#include "t.h"
TEST(smoke_getsystemmetrics) {
    REQUIRE(U(GetSystemMetrics));
    CHECK(U(GetSystemMetrics)(SM_CXSCREEN) > 0);
}
