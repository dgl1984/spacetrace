#include "HeadRepository.h"

#include "spacetrace/Resample.h"

#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <sstream>

namespace spacetrace::plugin {
namespace {

juce::String manifestFolderName(BuiltInDatasetId id) {
    switch (id) {
        case BuiltInDatasetId::IrcamListen1050: return "IRCAM_1050";
        case BuiltInDatasetId::MitKemarNormalPinna: return "MIT_KEMAR_Normal";
        case BuiltInDatasetId::Sadie2D1Ku100: return "KU100_SADIE_D1";
        case BuiltInDatasetId::ThkKu100Full2Deg: return "KU100_FULL2DEG";
        case BuiltInDatasetId::FabianHato0: return "FABIAN_HATO0";
    }
    return {};
}

std::string cacheKey(const juce::String& stableId,
                     const juce::String& headHash,
                     const juce::String& correctionHash,
                     const juce::String& stereoPairLeftCorrectionHash,
                     const juce::String& stereoPairRightCorrectionHash,
                     float levelTrimDb,
                     float monoTrimDb,
                     float leftGainDb,
                     float rightGainDb,
                     float stereoPairLeftGainDb,
                     float stereoPairRightGainDb,
                     float stereoPairTrimDb,
                     double sampleRate) {
    std::ostringstream s;
    s.precision(17);
    s << stableId.toStdString() << '|' << headHash.toStdString() << '|'
      << correctionHash.toStdString() << '|'
      << stereoPairLeftCorrectionHash.toStdString() << '|'
      << stereoPairRightCorrectionHash.toStdString() << '|'
      << levelTrimDb << '|' << monoTrimDb << '|' << leftGainDb << '|' << rightGainDb << '|'
      << stereoPairLeftGainDb << '|' << stereoPairRightGainDb << '|' << stereoPairTrimDb << '|'
      << sampleRate;
    return s.str();
}

bool parseRequiredString(const juce::DynamicObject& obj, const char* key,
                         juce::String& out, juce::String& error) {
    const juce::Identifier keyId(key);
    const auto value = obj.getProperty(keyId);
    if (!value.isString() || value.toString().isEmpty()) {
        error = "manifest field '" + juce::String(key) + "' is missing or empty";
        return false;
    }
    out = value.toString();
    return true;
}

} // namespace

HeadRepository& HeadRepository::instance() {
    static HeadRepository repository;
    return repository;
}

juce::File HeadRepository::locateHeadsDirectory() {
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("SPACETRACE_HEADS_DIR", {});
    if (overridePath.isNotEmpty()) {
        const juce::File f(overridePath);
        if (f.isDirectory()) return f;
    }

    auto base = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (int depth = 0; depth < 8 && base != juce::File(); ++depth) {
        const auto candidate = base.getChildFile("Heads");
        if (candidate.isDirectory()) return candidate;
        const auto parent = base.getParentDirectory();
        if (parent == base) break;
        base = parent;
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory().getChildFile("Heads");
    return cwd.isDirectory() ? cwd : juce::File();
}

juce::File HeadRepository::headsDirectory() const {
    return locateHeadsDirectory();
}

juce::String HeadRepository::sha256File(const juce::File& file, juce::String& error) {
    if (!file.existsAsFile()) {
        error = "file does not exist: " + file.getFullPathName();
        return {};
    }
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) {
        error = "could not open file for hashing: " + file.getFullPathName();
        return {};
    }
    const juce::SHA256 hash(stream);
    error.clear();
    return hash.toHexString();
}

bool HeadRepository::readManifest(BuiltInDatasetId id, Manifest& out, juce::String& error) const {
    const auto root = locateHeadsDirectory();
    if (!root.isDirectory()) {
        error = "SpaceTrace Heads folder was not found beside the portable plug-in folder.";
        return false;
    }

    const auto folder = root.getChildFile(manifestFolderName(id));
    const auto manifestFile = folder.getChildFile("manifest.json");
    if (!manifestFile.existsAsFile()) {
        error = "Head package is missing: " + juce::String::fromUTF8(datasetDescriptor(id).displayName.data()) +
                ". Expected " + manifestFile.getFullPathName();
        return false;
    }

    juce::var rootVar;
    const auto parseResult = juce::JSON::parse(manifestFile.loadFileAsString(), rootVar);
    if (parseResult.failed()) {
        error = "Invalid head manifest " + manifestFile.getFullPathName() + ": " + parseResult.getErrorMessage();
        return false;
    }
    auto* obj = rootVar.getDynamicObject();
    if (obj == nullptr) {
        error = "Head manifest root must be a JSON object: " + manifestFile.getFullPathName();
        return false;
    }

    const auto schema = static_cast<int>(obj->getProperty("schemaVersion"));
    if (schema != 1) {
        error = "Unsupported head manifest schema in " + manifestFile.getFullPathName();
        return false;
    }

    Manifest m;
    m.id = id;
    m.folder = folder;
    juce::String headName;
    if (!parseRequiredString(*obj, "stableId", m.stableId, error) ||
        !parseRequiredString(*obj, "displayName", m.displayName, error) ||
        !parseRequiredString(*obj, "headFile", headName, error) ||
        !parseRequiredString(*obj, "headSha256", m.headSha256, error))
        return false;

    const auto expectedId = juce::String(datasetDescriptor(id).stableId.data());
    if (m.stableId != expectedId) {
        error = "Head manifest stableId mismatch. Expected '" + expectedId + "', found '" + m.stableId + "'.";
        return false;
    }

    m.headFile = folder.getChildFile(headName);
    if (!m.headFile.existsAsFile()) {
        error = "Head data file is missing for " + m.displayName + ": " + m.headFile.getFullPathName();
        return false;
    }

    const auto trimValue = obj->getProperty("levelTrimDb");
    if (!trimValue.isVoid()) {
        if (!(trimValue.isDouble() || trimValue.isInt() || trimValue.isInt64())) {
            error = "manifest field 'levelTrimDb' must be numeric";
            return false;
        }
        m.levelTrimDb = static_cast<float>(static_cast<double>(trimValue));
        if (!std::isfinite(m.levelTrimDb) || m.levelTrimDb < -24.0f || m.levelTrimDb > 24.0f) {
            error = "manifest field 'levelTrimDb' must be between -24 and +24 dB";
            return false;
        }
    }

    auto parseOptionalGain = [&](const char* key, float& target) {
        const auto value = obj->getProperty(key);
        if (value.isVoid()) return true;
        if (!(value.isDouble() || value.isInt() || value.isInt64())) {
            error = "manifest field '" + juce::String(key) + "' must be numeric";
            return false;
        }
        target = static_cast<float>(static_cast<double>(value));
        if (!std::isfinite(target) || target < -6.0f || target > 6.0f) {
            error = "manifest field '" + juce::String(key) + "' must be between -6 and +6 dB";
            return false;
        }
        return true;
    };
    if (!parseOptionalGain("monoTrimDb", m.monoTrimDb) ||
        !parseOptionalGain("leftGainDb", m.leftGainDb) || !parseOptionalGain("rightGainDb", m.rightGainDb))
        return false;
    if (std::abs(m.leftGainDb + m.rightGainDb) > 0.05f) {
        error = "leftGainDb/rightGainDb must be equal-and-opposite within 0.05 dB so channel calibration does not change overall head level";
        return false;
    }

    const auto correctionValue = obj->getProperty("correctionFile");
    if (correctionValue.isString() && correctionValue.toString().isNotEmpty()) {
        m.hasCorrection = true;
        m.correctionFile = folder.getChildFile(correctionValue.toString());
        if (!parseRequiredString(*obj, "correctionSha256", m.correctionSha256, error)) return false;
        m.correctionName = obj->getProperty("correctionName").toString();
        m.correctionVersion = obj->getProperty("correctionVersion").toString();
        if (!m.correctionFile.existsAsFile()) {
            error = "Dataset correction file is missing for " + m.displayName + ": " + m.correctionFile.getFullPathName();
            return false;
        }
    }

    const auto stereoLeftValue = obj->getProperty("stereoPairLeftCorrectionFile");
    const auto stereoRightValue = obj->getProperty("stereoPairRightCorrectionFile");
    const bool hasStereoLeft = stereoLeftValue.isString() && stereoLeftValue.toString().isNotEmpty();
    const bool hasStereoRight = stereoRightValue.isString() && stereoRightValue.toString().isNotEmpty();
    if (hasStereoLeft != hasStereoRight) {
        error = "Stereo Pair correction must provide both left and right correction files";
        return false;
    }
    if (hasStereoLeft) {
        m.hasStereoPairCorrection = true;
        m.stereoPairLeftCorrectionFile = folder.getChildFile(stereoLeftValue.toString());
        m.stereoPairRightCorrectionFile = folder.getChildFile(stereoRightValue.toString());
        if (!parseRequiredString(*obj, "stereoPairLeftCorrectionSha256", m.stereoPairLeftCorrectionSha256, error) ||
            !parseRequiredString(*obj, "stereoPairRightCorrectionSha256", m.stereoPairRightCorrectionSha256, error))
            return false;
        m.stereoPairCorrectionName = obj->getProperty("stereoPairCorrectionName").toString();
        m.stereoPairCorrectionVersion = obj->getProperty("stereoPairCorrectionVersion").toString();
        if (!m.stereoPairLeftCorrectionFile.existsAsFile() || !m.stereoPairRightCorrectionFile.existsAsFile()) {
            error = "Stereo Pair correction file is missing for " + m.displayName;
            return false;
        }

        auto parseStereoGain = [&](const char* key, float& target, float minimum, float maximum) {
            const auto value = obj->getProperty(key);
            if (value.isVoid()) return true;
            if (!(value.isDouble() || value.isInt() || value.isInt64())) {
                error = "manifest field '" + juce::String(key) + "' must be numeric";
                return false;
            }
            target = static_cast<float>(static_cast<double>(value));
            if (!std::isfinite(target) || target < minimum || target > maximum) {
                error = "manifest field '" + juce::String(key) + "' is outside the supported range";
                return false;
            }
            return true;
        };
        if (!parseStereoGain("stereoPairLeftGainDb", m.stereoPairLeftGainDb, -6.0f, 6.0f) ||
            !parseStereoGain("stereoPairRightGainDb", m.stereoPairRightGainDb, -6.0f, 6.0f) ||
            !parseStereoGain("stereoPairTrimDb", m.stereoPairTrimDb, -12.0f, 12.0f))
            return false;
        if (std::abs(m.stereoPairLeftGainDb + m.stereoPairRightGainDb) > 0.05f) {
            error = "stereoPairLeftGainDb/stereoPairRightGainDb must be equal-and-opposite within 0.05 dB";
            return false;
        }
    }

    out = std::move(m);
    error.clear();
    return true;
}

std::shared_ptr<const DatasetPackage> HeadRepository::loadSource(const Manifest& manifest, juce::String& error) {
    {
        const std::lock_guard lock(mutex_);
        const auto it = sourceCache_.find(manifest.headSha256.toStdString());
        if (it != sourceCache_.end()) return it->second;
    }

    auto loaded = NativePackage::loadFile(std::filesystem::u8path(manifest.headFile.getFullPathName().toStdString()));
    if (!loaded) {
        error = "Could not load " + manifest.displayName + ": " + juce::String(loaded.error);
        return {};
    }

    auto source = std::shared_ptr<const DatasetPackage>(std::move(loaded.package));
    {
        const std::lock_guard lock(mutex_);
        auto [it, inserted] = sourceCache_.emplace(manifest.headSha256.toStdString(), source);
        if (!inserted) return it->second;
    }
    return source;
}

bool HeadRepository::loadCorrectionWav(const juce::File& file,
                                       const juce::String& name,
                                       const juce::String& version,
                                       CompensationProfile& out,
                                       juce::String& error) {
    if (!file.existsAsFile()) {
        error = "Dataset correction is missing: " + file.getFullPathName();
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels < 1) {
        error = "Dataset correction is not a readable WAV: " + file.getFullPathName();
        return false;
    }

    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, true)) {
        error = "Dataset correction could not be read completely: " + file.getFullPathName();
        return false;
    }
    if (audio.getNumChannels() > 1) {
        const auto* a = audio.getReadPointer(0);
        const auto* b = audio.getReadPointer(1);
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            if (std::abs(a[i] - b[i]) > 1.0e-6f) {
                error = "Dataset correction must be mono or identical stereo: " + file.getFullPathName();
                return false;
            }
        }
    }

    CompensationProfile profile;
    profile.name = name.toStdString();
    profile.version = version.toStdString();
    profile.sampleRate = reader->sampleRate;
    profile.impulse.assign(audio.getReadPointer(0), audio.getReadPointer(0) + audio.getNumSamples());
    std::string reason;
    if (!profile.valid(&reason)) {
        error = "Invalid dataset correction " + file.getFileName() + ": " + juce::String(reason);
        return false;
    }
    out = std::move(profile);
    error.clear();
    return true;
}

std::shared_ptr<const PreparedHeadData> HeadRepository::prepare(BuiltInDatasetId id,
                                                               double hostSampleRate,
                                                               juce::String& error) {
    if (!(hostSampleRate > 0.0) || !std::isfinite(hostSampleRate)) {
        error = "Host sample rate is invalid.";
        return {};
    }

    Manifest manifest;
    if (!readManifest(id, manifest, error)) return {};

    const auto key = cacheKey(manifest.stableId, manifest.headSha256, manifest.correctionSha256,
                              manifest.stereoPairLeftCorrectionSha256, manifest.stereoPairRightCorrectionSha256,
                              manifest.levelTrimDb, manifest.monoTrimDb, manifest.leftGainDb, manifest.rightGainDb,
                              manifest.stereoPairLeftGainDb, manifest.stereoPairRightGainDb, manifest.stereoPairTrimDb,
                              hostSampleRate);
    {
        const std::lock_guard lock(mutex_);
        const auto it = preparedCache_.find(key);
        if (it != preparedCache_.end())
            if (auto cached = it->second.lock()) return cached;
    }

    // Serialize cache misses so hosts that construct many instances in parallel
    // do not parse/resample the same immutable head repeatedly. Cache hits never
    // take this slower path.
    const std::lock_guard buildLock(buildMutex_);
    {
        const std::lock_guard lock(mutex_);
        const auto it = preparedCache_.find(key);
        if (it != preparedCache_.end())
            if (auto cached = it->second.lock()) return cached;
    }

    const auto actualHeadHash = sha256File(manifest.headFile, error);
    if (actualHeadHash.isEmpty()) return {};
    if (!actualHeadHash.equalsIgnoreCase(manifest.headSha256)) {
        error = "Head data hash mismatch for " + manifest.displayName + ". The package may be damaged or mismatched.";
        return {};
    }

    if (manifest.hasCorrection) {
        const auto actualCorrectionHash = sha256File(manifest.correctionFile, error);
        if (actualCorrectionHash.isEmpty()) return {};
        if (!actualCorrectionHash.equalsIgnoreCase(manifest.correctionSha256)) {
            error = "Dataset correction hash mismatch for " + manifest.displayName + ". The package may be damaged or mismatched.";
            return {};
        }
    }
    if (manifest.hasStereoPairCorrection) {
        const auto actualLeftHash = sha256File(manifest.stereoPairLeftCorrectionFile, error);
        if (actualLeftHash.isEmpty()) return {};
        const auto actualRightHash = sha256File(manifest.stereoPairRightCorrectionFile, error);
        if (actualRightHash.isEmpty()) return {};
        if (!actualLeftHash.equalsIgnoreCase(manifest.stereoPairLeftCorrectionSha256) ||
            !actualRightHash.equalsIgnoreCase(manifest.stereoPairRightCorrectionSha256)) {
            error = "Stereo Pair correction hash mismatch for " + manifest.displayName + ". The package may be damaged or mismatched.";
            return {};
        }
    }

    auto source = loadSource(manifest, error);
    if (!source) return {};

    CompensationProfile correction;
    if (manifest.hasCorrection &&
        !loadCorrectionWav(manifest.correctionFile,
                           manifest.correctionName.isNotEmpty() ? manifest.correctionName : manifest.displayName + " Dataset Corrected",
                           manifest.correctionVersion, correction, error))
        return {};
    CompensationProfile stereoLeftCorrection;
    CompensationProfile stereoRightCorrection;
    if (manifest.hasStereoPairCorrection) {
        const auto baseName = manifest.stereoPairCorrectionName.isNotEmpty()
            ? manifest.stereoPairCorrectionName : manifest.displayName + " Stereo Pair Tonal Restoration";
        if (!loadCorrectionWav(manifest.stereoPairLeftCorrectionFile, baseName + " Left",
                               manifest.stereoPairCorrectionVersion, stereoLeftCorrection, error) ||
            !loadCorrectionWav(manifest.stereoPairRightCorrectionFile, baseName + " Right",
                               manifest.stereoPairCorrectionVersion, stereoRightCorrection, error))
            return {};
    }

    auto prepared = std::make_shared<PreparedHeadData>();
    prepared->id = id;
    prepared->stableId = manifest.stableId;
    prepared->displayName = manifest.displayName;
    prepared->headHash = actualHeadHash;
    prepared->levelTrimDb = manifest.levelTrimDb;
    prepared->monoTrimDb = manifest.monoTrimDb;
    prepared->leftGainDb = manifest.leftGainDb;
    prepared->rightGainDb = manifest.rightGainDb;
    prepared->stereoPair.leftGainDb = manifest.stereoPairLeftGainDb;
    prepared->stereoPair.rightGainDb = manifest.stereoPairRightGainDb;
    prepared->stereoPair.trimDb = manifest.stereoPairTrimDb;
    prepared->hrtf = resampleHRTFSet(source->hrtf, hostSampleRate);
    if (manifest.hasCorrection) {
        prepared->compensation = resampleCompensation(correction, hostSampleRate);
        prepared->hasCompensation = true;
    }
    if (manifest.hasStereoPairCorrection) {
        prepared->stereoPair.leftCompensation = resampleCompensation(stereoLeftCorrection, hostSampleRate);
        prepared->stereoPair.rightCompensation = resampleCompensation(stereoRightCorrection, hostSampleRate);
        prepared->stereoPair.hasCompensation = true;
    }

    std::shared_ptr<const PreparedHeadData> immutable = prepared;
    {
        const std::lock_guard lock(mutex_);
        auto& slot = preparedCache_[key];
        if (auto raced = slot.lock()) return raced;
        slot = immutable;
    }
    error.clear();
    return immutable;
}

juce::String HeadRepository::contentHash(BuiltInDatasetId id, juce::String& error) {
    Manifest manifest;
    if (!readManifest(id, manifest, error)) return {};
    return sha256File(manifest.headFile, error);
}

void HeadRepository::clearExpiredForTests() {
    const std::lock_guard lock(mutex_);
    for (auto it = preparedCache_.begin(); it != preparedCache_.end();) {
        if (it->second.expired()) it = preparedCache_.erase(it); else ++it;
    }
}

} // namespace spacetrace::plugin
