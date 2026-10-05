#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace spacetrace {

enum class BuiltInDatasetId {
    IrcamListen1050,
    MitKemarNormalPinna,
    Sadie2D1Ku100,
    ThkKu100Full2Deg,
    FabianHato0,
};

inline constexpr std::size_t builtInDatasetCount = 5;

struct DatasetDescriptor {
    BuiltInDatasetId id;
    std::string_view stableId;
    std::string_view displayName;
    std::string_view database;
    std::string_view subject;
    std::string_view sourceUrl;
    std::string_view packageFile;
    bool isDefault = false;
};

[[nodiscard]] const std::vector<DatasetDescriptor>& builtInDatasetCatalog();
[[nodiscard]] const DatasetDescriptor& defaultBuiltInDataset();
[[nodiscard]] const DatasetDescriptor& datasetDescriptor(BuiltInDatasetId id);

} // namespace spacetrace
