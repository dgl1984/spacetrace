#include "spacetrace/NativePackage.h"
#include <iostream>

using namespace spacetrace;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: spacetrace_package_info <file.sthrtf>\n";
        return 2;
    }
    auto result = NativePackage::loadFile(argv[1]);
    if (!result) {
        std::cerr << "SpaceTrace package error: " << result.error << "\n";
        return 1;
    }
    const auto& p = *result.package;
    const auto& md = p.hrtf.metadata();
    std::cout << "Name: " << md.name << "\n"
              << "Database: " << md.database << "\n"
              << "Subject: " << md.subject << "\n"
              << "Sample rate: " << p.hrtf.sampleRate() << "\n"
              << "Measurements: " << p.hrtf.measurements().size() << "\n"
              << "IR samples: " << p.hrtf.measurements().front().left.size() << "\n"
              << "Source: " << md.sourceUrl << "\n"
              << "SHA-256: " << md.contentSha256 << "\n"
              << "Processing: " << (md.processing.empty() ? "raw / unspecified" : md.processing) << "\n"
              << "Dataset correction: " << (p.hasDatasetCompensation ? p.datasetCompensation.name : "none") << "\n";
    return 0;
}
