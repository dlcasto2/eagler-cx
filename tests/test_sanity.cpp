#include "doctest.h"
#include "fix.h"

TEST_CASE("GLFix multiplies whole numbers") {
    CHECK((Fix<8, int32_t>(3) * Fix<8, int32_t>(2)).floor() == 6);
}
