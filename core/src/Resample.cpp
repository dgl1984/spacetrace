#include "spacetrace/Resample.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>

namespace spacetrace {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

double sinc(double x) {
    if (std::abs(x) < 1.0e-12) return 1.0;
    const auto p = kPi * x;
    return std::sin(p) / p;
}

std::size_t nextPowerOfTwo(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1u;
    return p;
}

void fft(std::vector<std::complex<double>>& a, bool inverse) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1u;
        for (; j & bit; bit >>= 1u) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1u) {
        const double angle = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t j = 0; j < len / 2u; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2u] * w;
                a[i + j] = u + v;
                a[i + j + len / 2u] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        const double inv = 1.0 / static_cast<double>(n);
        for (auto& v : a) v *= inv;
    }
}

std::vector<float> resampleMinimumPhaseImpulse(const std::vector<float>& input,
                                                double sourceRate,
                                                double targetRate) {
    if (input.empty()) return {};
    if (std::abs(sourceRate - targetRate) < 1.0e-9) return input;

    const double ratio = targetRate / sourceRate;
    const std::size_t outN = static_cast<std::size_t>(std::ceil(input.size() * ratio));
    const std::size_t sourceFftN = nextPowerOfTwo(std::max<std::size_t>(2048, input.size() * 8u));
    const std::size_t targetFftN = nextPowerOfTwo(std::max<std::size_t>(2048, outN * 8u));

    std::vector<std::complex<double>> sourceSpectrum(sourceFftN);
    for (std::size_t i = 0; i < input.size(); ++i) sourceSpectrum[i] = input[i];
    fft(sourceSpectrum, false);

    const std::size_t sourceNyquistBin = sourceFftN / 2u;
    std::vector<double> logMagnitude(targetFftN, std::log(1.0e-12));
    for (std::size_t k = 0; k <= targetFftN / 2u; ++k) {
        const double f = static_cast<double>(k) * targetRate / static_cast<double>(targetFftN);
        double mag = 1.0e-12;
        if (f <= sourceRate * 0.5) {
            const double sourceBin = f * static_cast<double>(sourceFftN) / sourceRate;
            const auto i0 = static_cast<std::size_t>(std::floor(sourceBin));
            const auto i1 = std::min(sourceNyquistBin, i0 + 1u);
            const double frac = sourceBin - static_cast<double>(i0);
            const double m0 = std::max(1.0e-12, std::abs(sourceSpectrum[std::min(i0, sourceNyquistBin)]));
            const double m1 = std::max(1.0e-12, std::abs(sourceSpectrum[i1]));
            // Interpolate log magnitude so narrow HRTF notches remain multiplicative
            // rather than being filled by linear-magnitude interpolation.
            mag = std::exp(std::log(m0) + (std::log(m1) - std::log(m0)) * frac);
        }
        logMagnitude[k] = std::log(std::max(1.0e-12, mag));
        if (k > 0 && k < targetFftN / 2u)
            logMagnitude[targetFftN - k] = logMagnitude[k];
    }

    std::vector<std::complex<double>> cepstrum(targetFftN);
    for (std::size_t i = 0; i < targetFftN; ++i) cepstrum[i] = logMagnitude[i];
    fft(cepstrum, true);

    // Real-cepstrum minimum-phase reconstruction.
    for (std::size_t n = 1; n < targetFftN / 2u; ++n) cepstrum[n] *= 2.0;
    for (std::size_t n = targetFftN / 2u + 1u; n < targetFftN; ++n) cepstrum[n] = 0.0;
    fft(cepstrum, false);
    for (auto& v : cepstrum) v = std::exp(v);
    fft(cepstrum, true);

    std::vector<float> out(outN, 0.0f);
    for (std::size_t i = 0; i < outN; ++i) out[i] = static_cast<float>(cepstrum[i].real());
    return out;
}

bool usesSeparatedMinimumPhaseShape(const HRTFSet& input) {
    if (input.metadata().processing.find("minimum-phase") != std::string::npos) return true;
    for (const auto& m : input.measurements())
        if (std::abs(m.leftDelaySamples) > 1.0e-12 || std::abs(m.rightDelaySamples) > 1.0e-12)
            return true;
    return false;
}
}

std::vector<float> resampleImpulse(const std::vector<float>& input,
                                   double sourceRate,
                                   double targetRate,
                                   int halfKernel) {
    if (input.empty()) return {};
    if (!(sourceRate > 0.0) || !(targetRate > 0.0))
        throw std::invalid_argument("resampleImpulse requires positive sample rates");
    if (std::abs(sourceRate - targetRate) < 1.0e-9) return input;
    halfKernel = std::max(4, halfKernel);

    const double ratio = targetRate / sourceRate;
    const std::size_t outN = static_cast<std::size_t>(std::ceil(input.size() * ratio));
    std::vector<float> out(outN, 0.0f);
    const double cutoff = std::min(1.0, ratio);

    for (std::size_t n = 0; n < outN; ++n) {
        const double srcPos = static_cast<double>(n) / ratio;
        const auto center = static_cast<long long>(std::floor(srcPos));
        double acc = 0.0;
        for (int k = -halfKernel + 1; k <= halfKernel; ++k) {
            const auto idx = center + k;
            if (idx < 0 || idx >= static_cast<long long>(input.size())) continue;
            const double d = srcPos - static_cast<double>(idx);
            if (std::abs(d) >= halfKernel) continue;
            const double window = 0.5 + 0.5 * std::cos(kPi * d / halfKernel);
            const double w = cutoff * sinc(cutoff * d) * window;
            acc += input[static_cast<std::size_t>(idx)] * w;
        }
        // FIR coefficients need inverse rate scaling so convolution gain stays
        // invariant when the sample density changes. Samples beyond the finite
        // FIR are zero; edge renormalisation would reshape the filter.
        acc *= sourceRate / targetRate;
        out[n] = static_cast<float>(acc);
    }
    return out;
}

HRTFSet resampleHRTFSet(const HRTFSet& input, double targetRate) {
    if (std::abs(input.sampleRate() - targetRate) < 1.0e-9) return input;
    std::vector<Measurement> measurements;
    measurements.reserve(input.measurements().size());
    const double delayScale = targetRate / input.sampleRate();
    const bool minimumPhaseShape = usesSeparatedMinimumPhaseShape(input);
    for (const auto& src : input.measurements()) {
        Measurement m = src;
        if (minimumPhaseShape) {
            m.left = resampleMinimumPhaseImpulse(src.left, input.sampleRate(), targetRate);
            m.right = resampleMinimumPhaseImpulse(src.right, input.sampleRate(), targetRate);
        } else {
            m.left = resampleImpulse(src.left, input.sampleRate(), targetRate);
            m.right = resampleImpulse(src.right, input.sampleRate(), targetRate);
        }
        m.leftDelaySamples *= delayScale;
        m.rightDelaySamples *= delayScale;
        measurements.push_back(std::move(m));
    }
    return HRTFSet(targetRate, input.metadata(), std::move(measurements));
}

CompensationProfile resampleCompensation(const CompensationProfile& input, double targetRate) {
    if (std::abs(input.sampleRate - targetRate) < 1.0e-9) return input;
    CompensationProfile out = input;
    out.impulse = resampleMinimumPhaseImpulse(input.impulse, input.sampleRate, targetRate);
    out.sampleRate = targetRate;
    return out;
}

} // namespace spacetrace
