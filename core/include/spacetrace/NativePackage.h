#pragma once

#include "spacetrace/Compensation.h"
#include "spacetrace/HRTFSet.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace spacetrace {

struct DatasetPackage {
    HRTFSet hrtf;
    CompensationProfile datasetCompensation;
    bool hasDatasetCompensation = false;
};

struct PackageResult {
    std::unique_ptr<DatasetPackage> package;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(package); }
};

class NativePackage {
public:
    static constexpr unsigned int formatVersion = 2;

    [[nodiscard]] static PackageResult loadFile(const std::filesystem::path& path);
    [[nodiscard]] static PackageResult loadMemory(std::span<const std::byte> data);
    [[nodiscard]] static bool writeFile(const std::filesystem::path& path,
                                        const DatasetPackage& package,
                                        std::string* error = nullptr);
};

} // namespace spacetrace
