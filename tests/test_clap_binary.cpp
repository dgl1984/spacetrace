#include <clap/clap.h>
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef SPACETRACE_EXPECTED_VERSION
#error SPACETRACE_EXPECTED_VERSION must be supplied by CMake from PROJECT_VERSION
#endif

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct HostData { bool audio = false; std::atomic<bool> callback {false}; };
HostData& data(const clap_host_t* h) { return *static_cast<HostData*>(h->host_data); }
bool CLAP_ABI mainThread(const clap_host_t*) { return true; }
bool CLAP_ABI audioThread(const clap_host_t* h) { return data(h).audio; }
const clap_host_thread_check_t threadCheck {mainThread, audioThread};
const void* CLAP_ABI extension(const clap_host_t*, const char* id) {
    return std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0 ? &threadCheck : nullptr;
}
void CLAP_ABI request(const clap_host_t*) {}
void CLAP_ABI callback(const clap_host_t* h) { data(h).callback.store(true); }
uint32_t CLAP_ABI emptySize(const clap_input_events_t*) { return 0; }
const clap_event_header_t* CLAP_ABI emptyGet(const clap_input_events_t*, uint32_t) { return nullptr; }
bool CLAP_ABI push(const clap_output_events_t*, const clap_event_header_t*) { return true; }
const clap_input_events_t noEvents {nullptr, emptySize, emptyGet};
const clap_output_events_t outputEvents {nullptr, push};
struct Instance {
    const clap_plugin_t* p;
    HostData& host;
    bool active = false, processing = false;
    ~Instance() {
        if (processing) { host.audio = true; p->stop_processing(p); host.audio = false; }
        if (active) p->deactivate(p);
        if (p) p->destroy(p);
    }
};
void drain(Instance& i) {
    if (i.host.callback.exchange(false)) i.p->on_main_thread(i.p);
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}
void parameter(Instance& i, const clap_plugin_params_t* params, clap_id id, double value) {
    clap_event_param_value_t e {};
    e.header.size = sizeof(e); e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    e.header.type = CLAP_EVENT_PARAM_VALUE; e.param_id = id; e.value = value;
    e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
    clap_input_events_t input {&e,
        [](const clap_input_events_t*) -> uint32_t { return 1; },
        [](const clap_input_events_t* events, uint32_t index) -> const clap_event_header_t* {
            return index == 0 ? &static_cast<const clap_event_param_value_t*>(events->ctx)->header : nullptr;
        }};
    params->flush(i.p, &input, &outputEvents);
}
struct State { std::vector<char> bytes; size_t position = 0; };
int64_t CLAP_ABI writeState(const clap_ostream_t* s, const void* buffer, uint64_t size) {
    auto& state = *static_cast<State*>(s->ctx);
    const auto n = static_cast<size_t>(std::min<uint64_t>(size, 37));
    const auto* begin = static_cast<const char*>(buffer);
    state.bytes.insert(state.bytes.end(), begin, begin + n); return static_cast<int64_t>(n);
}
int64_t CLAP_ABI readState(const clap_istream_t* s, void* buffer, uint64_t size) {
    auto& state = *static_cast<State*>(s->ctx);
    const auto n = std::min({static_cast<size_t>(size), size_t(29), state.bytes.size() - state.position});
    std::memcpy(buffer, state.bytes.data() + state.position, n); state.position += n;
    return static_cast<int64_t>(n);
}
}
int main(int argc, char** argv) {
    HMODULE library = nullptr;
    const clap_plugin_entry_t* entry = nullptr;
    bool initialized = false;
    try {
        require(argc == 2, "supply the CLAP binary path");
        library = LoadLibraryA(argv[1]); require(library != nullptr, "CLAP DLL load failed");
        entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(library, "clap_entry"));
        require(entry && clap_version_is_compatible(entry->clap_version), "missing or incompatible clap_entry");
        initialized = entry->init(argv[1]); require(initialized, "CLAP entry init failed");
        auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        require(factory && factory->get_plugin_count(factory) == 1, "expected one CLAP plugin");
        const auto* desc = factory->get_plugin_descriptor(factory, 0);
        require(desc && std::strcmp(desc->id, "com.lanesaudio.spacetrace") == 0, "wrong CLAP ID");
        require(std::strcmp(desc->version, SPACETRACE_EXPECTED_VERSION) == 0, "wrong CLAP version");
        HostData hostData;
        clap_host_t host {CLAP_VERSION, &hostData, "SpaceTrace binary test", "LanesAudio", "", "1", extension, request, request, callback};
        State saved;
        for (int pass = 0; pass < 12; ++pass) {
            Instance instance {factory->create_plugin(factory, &host, desc->id), hostData};
            require(instance.p && instance.p->init(instance.p), "CLAP instance init failed");
            auto* params = static_cast<const clap_plugin_params_t*>(instance.p->get_extension(instance.p, CLAP_EXT_PARAMS));
            auto* state = static_cast<const clap_plugin_state_t*>(instance.p->get_extension(instance.p, CLAP_EXT_STATE));
            auto* ports = static_cast<const clap_plugin_audio_ports_t*>(instance.p->get_extension(instance.p, CLAP_EXT_AUDIO_PORTS));
            require(params && state && ports, "required CLAP extension missing");
            require(params->count(instance.p) == 8, "expected eight automatable controls");
            for (bool input : {true, false}) {
                clap_audio_port_info_t info {};
                require(ports->count(instance.p, input) == 1 && ports->get(instance.p, 0, input, &info), "wrong bus count");
                require(info.channel_count == 2, "default bus must be stereo");
            }
            clap_id azimuth = CLAP_INVALID_ID;
            clap_id inputMode = CLAP_INVALID_ID;
            clap_id widthOffset = CLAP_INVALID_ID;
            clap_id airLoss = CLAP_INVALID_ID;
            for (uint32_t n = 0; n < params->count(instance.p); ++n) {
                clap_param_info_t info {}; require(params->get_info(instance.p, n, &info), "parameter metadata failed");
                if (std::strcmp(info.name, "Azimuth") == 0) {
                    require(info.min_value == 0.0 && info.max_value == 360.0, "CLAP azimuth range is not degrees");
                    azimuth = info.id;
                }
                if (std::strcmp(info.name, "Distance") == 0)
                    require(info.min_value == 0.25 && info.max_value == 20.0, "CLAP distance range is not metres");
                if (std::strcmp(info.name, "Input Mode") == 0) {
                    require(info.min_value == 0.0 && info.max_value == 1.0, "CLAP Input Mode range is wrong");
                    inputMode = info.id;
                }
                if (std::strcmp(info.name, "Stereo Pair Width Offset") == 0) {
                    require(info.min_value == -45.0 && info.max_value == 45.0, "CLAP Width Offset range is wrong");
                    widthOffset = info.id;
                }
                if (std::strcmp(info.name, "Air Loss") == 0) {
                    require(info.min_value == 0.0 && info.max_value == 1.0, "CLAP Air Loss range is wrong");
                    airLoss = info.id;
                }
                double value = 0; char text[256] {};
                require(params->get_value(instance.p, info.id, &value), "parameter value failed");
                require(value >= info.min_value && value <= info.max_value, "parameter outside its declared range");
                require(params->value_to_text(instance.p, info.id, value, text, sizeof(text)) && text[0], "parameter text failed");
            }
            require(azimuth != CLAP_INVALID_ID, "azimuth missing");
            require(inputMode != CLAP_INVALID_ID, "Input Mode missing");
            require(widthOffset != CLAP_INVALID_ID, "Stereo Pair Width Offset missing");
            require(airLoss != CLAP_INVALID_ID, "Air Loss missing");
            if (pass == 0) {
                parameter(instance, params, azimuth, 123.0);
                parameter(instance, params, inputMode, 1.0);
                parameter(instance, params, widthOffset, 17.0);
                parameter(instance, params, airLoss, 0.0);
                clap_ostream_t output {&saved, writeState};
                require(state->save(instance.p, &output) && !saved.bytes.empty(), "state save failed with short writes");
            } else {
                saved.position = 0; clap_istream_t input {&saved, readState};
                require(state->load(instance.p, &input), "state load failed with short reads");
                double value = 0;
                require(params->get_value(instance.p, azimuth, &value) && std::abs(value - 123.0) < 0.01, "state did not restore azimuth");
                require(params->get_value(instance.p, inputMode, &value) && std::abs(value - 1.0) < 0.01, "state did not restore Input Mode");
                require(params->get_value(instance.p, widthOffset, &value) && std::abs(value - 17.0) < 0.01, "state did not restore Width Offset");
                require(params->get_value(instance.p, airLoss, &value) && std::abs(value) < 0.01, "state did not restore Air Loss");
            }
            parameter(instance, params, azimuth, 90.0);
            parameter(instance, params, inputMode, 0.0);
            parameter(instance, params, widthOffset, 0.0);
            parameter(instance, params, airLoss, 1.0);
            const double rate = std::array<double, 3>{44100.0, 48000.0, 96000.0}[pass % 3];
            instance.active = instance.p->activate(instance.p, rate, 1, 512);
            require(instance.active, "CLAP activation failed");
            hostData.audio = true;
            instance.processing = instance.p->start_processing(instance.p);
            require(instance.processing, "CLAP start_processing failed");
            std::array<float, 512> inL {}, inR {}, outL {}, outR {};
            float* inputs[] {inL.data(), inR.data()}; float* outputs[] {outL.data(), outR.data()};
            clap_audio_buffer_t input {}; input.data32 = inputs; input.channel_count = 2;
            clap_audio_buffer_t output {}; output.data32 = outputs; output.channel_count = 2;
            int64_t clock = 0; double leftEnergy = 0, rightEnergy = 0;
            for (uint32_t frames : {1u, 17u, 64u, 511u, 512u, 512u, 512u, 512u}) {
                inL.fill(0); inR.fill(0); outL.fill(0); outR.fill(0);
                if (clock == 0) inL[0] = inR[0] = 1.0f;
                clap_process_t process {}; process.steady_time = clock; process.frames_count = frames;
                process.audio_inputs = &input; process.audio_outputs = &output;
                process.audio_inputs_count = process.audio_outputs_count = 1;
                process.in_events = &noEvents; process.out_events = &outputEvents;
                require(instance.p->process(instance.p, &process) != CLAP_PROCESS_ERROR, "CLAP processing failed");
                for (uint32_t n = 0; n < frames; ++n) {
                    require(std::isfinite(outL[n]) && std::isfinite(outR[n]), "non-finite CLAP audio");
                    leftEnergy += outL[n] * outL[n]; rightEnergy += outR[n] * outR[n];
                }
                clock += frames;
            }
            require(leftEnergy > 1e-8 && leftEnergy > rightEnergy * 1.5, "CLAP impulse silent or wrong ear dominance");
            instance.p->stop_processing(instance.p); instance.processing = false; hostData.audio = false;
            instance.p->deactivate(instance.p); instance.active = false; drain(instance);
        }
        entry->deinit(); initialized = false; FreeLibrary(library); library = nullptr;
        std::cout << "PASS: CLAP binary metadata, ranges, state with partial I/O, 12 lifecycles, variable blocks, 44.1/48/96 kHz audio\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        if (initialized) entry->deinit();
        if (library) FreeLibrary(library);
        return 1;
    }
}
