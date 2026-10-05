#pragma once

#include "spacetrace/HRTFSet.h"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace spacetrace {

struct ImportResult {
    std::unique_ptr<HRTFSet> set;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(set); }
};

class HRTFImporter {
public:
    virtual ~HRTFImporter() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool supports(const std::filesystem::path& file) const = 0;
    [[nodiscard]] virtual ImportResult importFile(const std::filesystem::path& file) const = 0;
};

} // namespace spacetrace
