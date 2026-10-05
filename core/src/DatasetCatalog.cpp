#include "spacetrace/DatasetCatalog.h"

#include <stdexcept>

namespace spacetrace {

const std::vector<DatasetDescriptor>& builtInDatasetCatalog() {
    static const std::vector<DatasetDescriptor> catalog {
        {
            BuiltInDatasetId::IrcamListen1050,
            "ircam.listen.1050.spacetrace-mp128.44100",
            "IRCAM LISTEN 1050",
            "IRCAM LISTEN",
            "1050",
            "https://sofacoustics.org/data/database/listen%20%28hrtf%29/IRC_1050_R_44100.sofa",
            "IRC_1050_R_44100.sthrtf",
            true,
        },
        {
            BuiltInDatasetId::MitKemarNormalPinna,
            "mit.kemar.normal-pinna.44100",
            "MIT KEMAR Normal Pinna",
            "MIT KEMAR",
            "KEMAR normal pinna",
            "https://sofacoustics.org/data/database/mit/mit_kemar_normal_pinna.sofa",
            "mit_kemar_normal_pinna.sthrtf",
            false,
        },
        {
            BuiltInDatasetId::Sadie2D1Ku100,
            "sadie2.d1.ku100.44100",
            "SADIE II D1 / Neumann KU100",
            "SADIE II",
            "D1 (Neumann KU100)",
            "https://zenodo.org/records/12092466/files/D1_HRIR_SOFA.zip?download=1",
            "sadie2_d1_ku100_44100.sthrtf",
            false,
        },
        {
            BuiltInDatasetId::ThkKu100Full2Deg,
            "thk.ku100.full2deg.48000",
            "TH Köln KU100 FULL2DEG",
            "TH Köln / Bernschütz",
            "Neumann KU100 FULL2DEG",
            "https://zenodo.org/records/3928297/files/HRIR_FULL2DEG.sofa?download=1",
            "thk_ku100_full2deg_48000.sthrtf",
            false,
        },
        {
            BuiltInDatasetId::FabianHato0,
            "fabian.hato0.44100",
            "FABIAN HATO 0",
            "FABIAN HRTF database",
            "HATO 0 degrees",
            "https://depositonce.tu-berlin.de/handle/11303/6153",
            "fabian_hato0_44100.sthrtf",
            false,
        },
    };
    return catalog;
}

const DatasetDescriptor& defaultBuiltInDataset() {
    for (const auto& d : builtInDatasetCatalog())
        if (d.isDefault) return d;
    throw std::logic_error("SpaceTrace built-in dataset catalog has no default");
}

const DatasetDescriptor& datasetDescriptor(BuiltInDatasetId id) {
    for (const auto& d : builtInDatasetCatalog())
        if (d.id == id) return d;
    throw std::out_of_range("unknown SpaceTrace built-in dataset id");
}

} // namespace spacetrace
