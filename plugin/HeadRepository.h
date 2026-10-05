#pragma once

#include "spacetrace/DatasetCatalog.h"
#include "spacetrace/NativePackage.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace spacetrace::plugin {

struct StereoPairResources {
    CompensationProfile leftCompensation;
    CompensationProfile rightCompensation;
    bool hasCompensation = false;
    float leftGainDb = 0.0f;
    float rightGainDb = 0.0f;
    float trimDb = 0.0f;
};

struct PreparedHeadData {
    BuiltInDatasetId id {BuiltInDatasetId::IrcamListen1050};
    juce::String stableId;
    juce::String displayName;
    juce::String headHash;
    HRTFSet hrtf;
    CompensationProfile compensation;
    bool hasCompensation = false;
    StereoPairResources stereoPair;
    float levelTrimDb = 0.0f;
    float monoTrimDb = 0.0f;
    float leftGainDb = 0.0f;
    float rightGainDb = 0.0f;
};

class HeadRepository final {
public:
    static HeadRepository& instance();

    std::shared_ptr<const PreparedHeadData> prepare(BuiltInDatasetId id,
                                                    double hostSampleRate,
                                                    juce::String& error);
    juce::File headsDirectory() const;
    juce::String contentHash(BuiltInDatasetId id, juce::String& error);

    void clearExpiredForTests();

private:
    struct Manifest {
        BuiltInDatasetId id {BuiltInDatasetId::IrcamListen1050};
        juce::String stableId;
        juce::String displayName;
        juce::File folder;
        juce::File headFile;
        juce::String headSha256;
        juce::File correctionFile;
        juce::String correctionSha256;
        juce::String correctionName;
        juce::String correctionVersion;
        bool hasCorrection = false;
        juce::File stereoPairLeftCorrectionFile;
        juce::String stereoPairLeftCorrectionSha256;
        juce::File stereoPairRightCorrectionFile;
        juce::String stereoPairRightCorrectionSha256;
        juce::String stereoPairCorrectionName;
        juce::String stereoPairCorrectionVersion;
        bool hasStereoPairCorrection = false;
        float levelTrimDb = 0.0f;
        float monoTrimDb = 0.0f;
        float leftGainDb = 0.0f;
        float rightGainDb = 0.0f;
        float stereoPairLeftGainDb = 0.0f;
        float stereoPairRightGainDb = 0.0f;
        float stereoPairTrimDb = 0.0f;
    };

    HeadRepository() = default;

    static juce::File locateHeadsDirectory();
    static juce::String sha256File(const juce::File& file, juce::String& error);
    static bool loadCorrectionWav(const juce::File& file,
                                  const juce::String& name,
                                  const juce::String& version,
                                  CompensationProfile& out,
                                  juce::String& error);
    bool readManifest(BuiltInDatasetId id, Manifest& out, juce::String& error) const;

    std::shared_ptr<const DatasetPackage> loadSource(const Manifest& manifest, juce::String& error);

    mutable std::mutex mutex_;
    std::mutex buildMutex_;
    std::unordered_map<std::string, std::shared_ptr<const DatasetPackage>> sourceCache_;
    std::unordered_map<std::string, std::weak_ptr<const PreparedHeadData>> preparedCache_;
};

} // namespace spacetrace::plugin
