#include "doctest.h"
#include "lighting.h"

TEST_CASE("full brightness is unchanged") { CHECK(darkenColor(0x1234, 7, 0x0000) == 0x1234); }
TEST_CASE("half brightness halves each channel") { CHECK(darkenColor(0xFFFF, 3, 0x0000) == 0x7BEF); }
TEST_CASE("darkening never produces the transparent key") { CHECK(darkenColor(0x0821, 0, 0x0000) == 0x0020); }
TEST_CASE("the transparent key stays transparent") { CHECK(darkenColor(0x0000, 3, 0x0000) == 0x0000); }
TEST_CASE("brightness rises with level") {
    for (int l = 0; l < 7; ++l)
        CHECK((darkenColor(0xF800, l, 0) >> 11) <= (darkenColor(0xF800, l + 1, 0) >> 11));
}
