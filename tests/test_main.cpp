#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "fastmath.h"

int main(int argc, char **argv)
{
    init_fastmath();   // player_motion uses fast_sin/fast_cos
    const int result = doctest::Context(argc, argv).run();
    uninit_fastmath();
    return result;
}
