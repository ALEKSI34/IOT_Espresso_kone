

#ifndef _FILTER_H_
#define _FILTER_H_

#include <Arduino.h>

template <uint8_t K, class uint_t = uint16_t>
class ExponentialAveragingFilter
{
private:
    uint_t state = 0;
public:
    uint_t operator()(uint_t input) {
        state += input;
        uint_t output = (state + half) >> K;
        state -= output;
        return output;
    }

    static_assert(
        uint_t(0) < uint_t(-1),  // Check that `uint_t` is an unsigned type
        "The `uint_t` type should be an unsigned integer, otherwise, "
        "the division using bit shifts is invalid.");

    /// Fixed point representation of one half, used for rounding.
    constexpr static uint_t half = 1 << (K - 1);
    static_assert(K<7);
};


#endif