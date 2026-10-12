#include <keeb/dsp.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace keeb {
namespace {

uint32_t reverse_bits(uint32_t x, uint32_t bits) {
    uint32_t y = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        y = (y << 1) | (x & 1u);
        x >>= 1;
    }
    return y;
}

uint32_t log2u(uint32_t n) {
    uint32_t b = 0;
    while (n > 1) {
        n >>= 1;
        ++b;
    }
    return b;
}

struct TwiddleCache {
    uint32_t n;
    std::vector<float> wr;
    std::vector<float> wi;
};

const TwiddleCache& twiddles(uint32_t n) {
    static thread_local TwiddleCache cache = {0, std::vector<float>(), std::vector<float>()};
    if (cache.n != n) {
        cache.n = n;
        cache.wr.resize(n / 2);
        cache.wi.resize(n / 2);
        for (uint32_t k = 0; k < n / 2; ++k) {
            const double ang = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(n);
            cache.wr[k] = static_cast<float>(std::cos(ang));
            cache.wi[k] = static_cast<float>(std::sin(ang));
        }
    }
    return cache;
}

void cfft_inplace(float* re, float* im, uint32_t n) {
    const uint32_t bits = log2u(n);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = reverse_bits(i, bits);
        if (j > i) {
            const float tr = re[i];
            re[i] = re[j];
            re[j] = tr;
            const float ti = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    }

    const TwiddleCache& tw = twiddles(n);
    for (uint32_t len = 2; len <= n; len <<= 1) {
        const uint32_t half = len / 2;
        const uint32_t step = n / len;
        for (uint32_t i = 0; i < n; i += len) {
            uint32_t tw_idx = 0;
            for (uint32_t j = 0; j < half; ++j) {
                const float wr = tw.wr[tw_idx];
                const float wi = tw.wi[tw_idx];
                const uint32_t u = i + j;
                const uint32_t v = u + half;
                const float tr = wr * re[v] - wi * im[v];
                const float ti = wr * im[v] + wi * re[v];
                re[v] = re[u] - tr;
                im[v] = im[u] - ti;
                re[u] = re[u] + tr;
                im[u] = im[u] + ti;
                tw_idx += step;
            }
        }
    }
}

} // namespace

bool is_power_of_two(uint32_t n) {
    return n >= 2 && (n & (n - 1)) == 0;
}

void rfft_mag(const float* in, float* mag_out, uint32_t n) {
    std::vector<float> re(n);
    std::vector<float> im(n, 0.0f);
    std::memcpy(re.data(), in, n * sizeof(float));
    cfft_inplace(re.data(), im.data(), n);
    const uint32_t bins = n / 2 + 1;
    for (uint32_t k = 0; k < bins; ++k) {
        mag_out[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
    }
}

} // namespace keeb
