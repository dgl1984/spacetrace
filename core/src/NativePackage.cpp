#include "spacetrace/NativePackage.h"

#include <array>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <limits>
#include <type_traits>

namespace spacetrace {
namespace {
constexpr std::array<char, 8> kMagic {'S','T','H','R','T','F','1','\0'};
constexpr std::uint32_t kEndianMarker = 0x01020304u;
constexpr std::uint32_t kFlagCompensation = 1u;
constexpr std::size_t kMaxStringBytes = 1u << 20;
constexpr std::size_t kMaxMeasurements = 200000;
constexpr std::size_t kMaxIRLength = 1u << 20;

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    template <typename T>
    bool read(T& out) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (remaining() < sizeof(T)) return false;
        std::memcpy(&out, bytes_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }

    bool readBytes(void* dst, std::size_t n) {
        if (remaining() < n) return false;
        std::memcpy(dst, bytes_.data() + pos_, n);
        pos_ += n;
        return true;
    }

    bool readString(std::string& out) {
        std::uint32_t n = 0;
        if (!read(n) || n > kMaxStringBytes || remaining() < n) return false;
        out.assign(reinterpret_cast<const char*>(bytes_.data() + pos_), n);
        pos_ += n;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - pos_; }

private:
    std::span<const std::byte> bytes_;
    std::size_t pos_ = 0;
};

template <typename T>
bool writeValue(std::ofstream& f, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    f.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(f);
}

bool writeString(std::ofstream& f, const std::string& s) {
    if (s.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    const auto n = static_cast<std::uint32_t>(s.size());
    return writeValue(f, n) && (f.write(s.data(), static_cast<std::streamsize>(s.size())), static_cast<bool>(f));
}

PackageResult fail(std::string text) {
    PackageResult r;
    r.error = std::move(text);
    return r;
}
}

PackageResult NativePackage::loadFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return fail("could not open native HRTF package");
    f.seekg(0, std::ios::end);
    const auto end = f.tellg();
    if (end <= 0) return fail("native HRTF package is empty");
    f.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) return fail("failed reading native HRTF package");
    return loadMemory(bytes);
}

PackageResult NativePackage::loadMemory(std::span<const std::byte> data) {
    Reader r(data);
    std::array<char, 8> magic {};
    if (!r.readBytes(magic.data(), magic.size()) || magic != kMagic)
        return fail("not a SpaceTrace native HRTF package");

    std::uint32_t version = 0, endian = 0, flags = 0, count = 0, irLength = 0, compLength = 0;
    double sampleRate = 0.0, compSampleRate = 0.0;
    if (!r.read(version) || (version != 1u && version != formatVersion))
        return fail("unsupported native HRTF package version");
    if (!r.read(endian) || endian != kEndianMarker) return fail("native HRTF package endian mismatch");
    if (!r.read(flags) || !r.read(sampleRate) || !r.read(count) || !r.read(irLength) ||
        !r.read(compSampleRate) || !r.read(compLength))
        return fail("truncated native HRTF package header");

    if (count == 0 || count > kMaxMeasurements || irLength == 0 || irLength > kMaxIRLength || compLength > kMaxIRLength)
        return fail("native HRTF package dimensions are unreasonable");

    HRTFMetadata md;
    CompensationProfile cp;
    if (!r.readString(md.name) || !r.readString(md.subject) || !r.readString(md.sourceFormat) ||
        !r.readString(md.sourcePath) || !r.readString(md.sourceUrl) || !r.readString(md.database) ||
        !r.readString(md.licence) || !r.readString(md.attribution) || !r.readString(md.contentSha256))
        return fail("truncated native HRTF package string table");

    if (version >= 2u && !r.readString(md.processing))
        return fail("truncated native HRTF package processing metadata");

    if (!r.readString(cp.name) || !r.readString(cp.version))
        return fail("truncated native HRTF package string table");

    std::vector<Measurement> measurements;
    measurements.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        float az = 0, el = 0, dist = 0, ld = 0, rd = 0;
        if (!r.read(az) || !r.read(el) || !r.read(dist) || !r.read(ld) || !r.read(rd))
            return fail("truncated native HRTF measurement header");
        Measurement m;
        m.position = {az, el, dist};
        m.leftDelaySamples = ld;
        m.rightDelaySamples = rd;
        m.left.resize(irLength);
        m.right.resize(irLength);
        if (!r.readBytes(m.left.data(), irLength * sizeof(float)) ||
            !r.readBytes(m.right.data(), irLength * sizeof(float)))
            return fail("truncated native HRTF measurement data");
        measurements.push_back(std::move(m));
    }

    auto package = std::make_unique<DatasetPackage>();
    package->hrtf = HRTFSet(sampleRate, std::move(md), std::move(measurements));
    std::string reason;
    if (!package->hrtf.valid(&reason)) return fail("invalid HRTF in native package: " + reason);

    if ((flags & kFlagCompensation) != 0) {
        if (compLength == 0) return fail("compensation flag is set but impulse is empty");
        cp.sampleRate = compSampleRate;
        cp.impulse.resize(compLength);
        if (!r.readBytes(cp.impulse.data(), compLength * sizeof(float)))
            return fail("truncated dataset compensation impulse");
        if (!cp.valid(&reason)) return fail("invalid compensation in native package: " + reason);
        package->datasetCompensation = std::move(cp);
        package->hasDatasetCompensation = true;
    }

    PackageResult out;
    out.package = std::move(package);
    return out;
}

bool NativePackage::writeFile(const std::filesystem::path& path,
                              const DatasetPackage& package,
                              std::string* error) {
    auto setError = [&](std::string s) {
        if (error) *error = std::move(s);
        return false;
    };

    std::string reason;
    if (!package.hrtf.valid(&reason)) return setError("invalid HRTF: " + reason);
    if (package.hasDatasetCompensation && !package.datasetCompensation.valid(&reason))
        return setError("invalid dataset compensation: " + reason);

    const auto& ms = package.hrtf.measurements();
    const auto irLength = ms.front().left.size();
    for (const auto& m : ms)
        if (m.left.size() != irLength || m.right.size() != irLength)
            return setError("native package requires a common IR length for all measurements");

    if (ms.size() > std::numeric_limits<std::uint32_t>::max() ||
        irLength > std::numeric_limits<std::uint32_t>::max())
        return setError("native package dimensions exceed format limits");

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return setError("could not create native HRTF package");

    f.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    const std::uint32_t flags = package.hasDatasetCompensation ? kFlagCompensation : 0u;
    const std::uint32_t count = static_cast<std::uint32_t>(ms.size());
    const std::uint32_t n = static_cast<std::uint32_t>(irLength);
    const std::uint32_t compN = package.hasDatasetCompensation
        ? static_cast<std::uint32_t>(package.datasetCompensation.impulse.size()) : 0u;
    const double compRate = package.hasDatasetCompensation ? package.datasetCompensation.sampleRate : 0.0;

    if (!writeValue(f, formatVersion) || !writeValue(f, kEndianMarker) || !writeValue(f, flags) ||
        !writeValue(f, package.hrtf.sampleRate()) || !writeValue(f, count) || !writeValue(f, n) ||
        !writeValue(f, compRate) || !writeValue(f, compN))
        return setError("failed writing native HRTF package header");

    const auto& md = package.hrtf.metadata();
    const auto& cp = package.datasetCompensation;
    if (!writeString(f, md.name) || !writeString(f, md.subject) || !writeString(f, md.sourceFormat) ||
        !writeString(f, md.sourcePath) || !writeString(f, md.sourceUrl) || !writeString(f, md.database) ||
        !writeString(f, md.licence) || !writeString(f, md.attribution) || !writeString(f, md.contentSha256) ||
        !writeString(f, md.processing) || !writeString(f, cp.name) || !writeString(f, cp.version))
        return setError("failed writing native HRTF package string table");

    for (const auto& m : ms) {
        const float az = static_cast<float>(wrapAzimuthDegrees(m.position.azimuthDegrees));
        const float el = static_cast<float>(m.position.elevationDegrees);
        const float dist = static_cast<float>(m.position.distanceMetres);
        const float ld = static_cast<float>(m.leftDelaySamples);
        const float rd = static_cast<float>(m.rightDelaySamples);
        if (!writeValue(f, az) || !writeValue(f, el) || !writeValue(f, dist) || !writeValue(f, ld) || !writeValue(f, rd))
            return setError("failed writing native HRTF measurement header");
        f.write(reinterpret_cast<const char*>(m.left.data()), static_cast<std::streamsize>(m.left.size() * sizeof(float)));
        f.write(reinterpret_cast<const char*>(m.right.data()), static_cast<std::streamsize>(m.right.size() * sizeof(float)));
        if (!f) return setError("failed writing native HRTF measurement data");
    }

    if (package.hasDatasetCompensation) {
        f.write(reinterpret_cast<const char*>(cp.impulse.data()),
                static_cast<std::streamsize>(cp.impulse.size() * sizeof(float)));
        if (!f) return setError("failed writing native HRTF compensation data");
    }

    if (error) error->clear();
    return true;
}

} // namespace spacetrace
