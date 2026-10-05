#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(rel): return (ROOT/rel).read_text(encoding='utf-8')
def require(text, needle, msg):
    if needle not in text: raise AssertionError(msg)
def reject(text, needle, msg):
    if needle in text: raise AssertionError(msg)
def method_body(text, signature, next_signature):
    start=text.find(signature); end=text.find(next_signature,start+1)
    if start<0 or end<=start: raise AssertionError(f'Could not isolate {signature}')
    return text[start:end]

def main():
    cmake=read('CMakeLists.txt'); editor_h=read('plugin/PluginEditor.h'); editor=read('plugin/PluginEditor.cpp')
    slider=read('plugin/AccessibleParameterSlider.cpp'); combo=read('plugin/AccessibleComboBox.cpp'); processor_h=read('plugin/PluginProcessor.h')
    processor=read('plugin/PluginProcessor.cpp'); catalog=read('core/src/DatasetCatalog.cpp')
    build_ps=read('BUILD_WINDOWS.ps1') if (ROOT/'BUILD_WINDOWS.ps1').exists() else ''
    fetch_ps=read('FETCH_JUCE_WINDOWS.ps1') if (ROOT/'FETCH_JUCE_WINDOWS.ps1').exists() else ''
    fetch_clap_ps=read('FETCH_CLAP_WINDOWS.ps1') if (ROOT/'FETCH_CLAP_WINDOWS.ps1').exists() else ''
    sadie_ps=read('scripts/Prepare-SADIE-D1.ps1') if (ROOT/'scripts/Prepare-SADIE-D1.ps1').exists() else ''
    datasets_ps=read('scripts/Prepare-Datasets.ps1') if (ROOT/'scripts/Prepare-Datasets.ps1').exists() else ''
    sadie_selector=read('scripts/select_sadie_d1_sofa.py') if (ROOT/'scripts/select_sadie_d1_sofa.py').exists() else ''
    validator=read('scripts/validate_builtins.py') if (ROOT/'scripts/validate_builtins.py').exists() else ''
    package_checker=read('scripts/check_native_package.py') if (ROOT/'scripts/check_native_package.py').exists() else ''
    head_converter=read('scripts/convert_sofa_head.py') if (ROOT/'scripts/convert_sofa_head.py').exists() else ''
    head_validator=read('scripts/validate_head.py') if (ROOT/'scripts/validate_head.py').exists() else ''
    correction_normalizer=read('scripts/normalize_correction.py') if (ROOT/'scripts/normalize_correction.py').exists() else ''
    correction_plotter=read('scripts/plot_correction.py') if (ROOT/'scripts/plot_correction.py').exists() else ''
    channel_analyzer=read('scripts/analyze_head_channels.py') if (ROOT/'scripts/analyze_head_channels.py').exists() else ''
    head_packager=read('scripts/package_head.py') if (ROOT/'scripts/package_head.py').exists() else ''
    stereo_pair_prep=read('scripts/prepare_stereo_pair.py') if (ROOT/'scripts/prepare_stereo_pair.py').exists() else ''
    scripts_readme=read('scripts/README.md') if (ROOT/'scripts/README.md').exists() else ''
    requirements=read('scripts/requirements.txt') if (ROOT/'scripts/requirements.txt').exists() else ''
    first_test=read('WINDOWS_FIRST_TEST.md') if (ROOT/'WINDOWS_FIRST_TEST.md').exists() else ''
    realtime_h=read('core/include/spacetrace/RealtimeFIRRenderer.h')
    realtime=read('core/src/RealtimeFIRRenderer.cpp')
    distance_h=read('core/include/spacetrace/DistanceModel.h')
    distance=read('core/src/DistanceModel.cpp')
    papa_h=read('core/include/spacetrace/PapaPanRenderer.h')
    papa=read('core/src/PapaPanRenderer.cpp')
    resample=read('core/src/Resample.cpp')
    head_repo_h=read('plugin/HeadRepository.h')
    head_repo=read('plugin/HeadRepository.cpp')
    spatial_h=read('plugin/SpatialDisplay.h') if (ROOT/'plugin/SpatialDisplay.h').exists() else ''
    spatial=read('plugin/SpatialDisplay.cpp') if (ROOT/'plugin/SpatialDisplay.cpp').exists() else ''
    ui_contract=read('Docs/UNIVERSAL_ACCESS_UI_CONTRACT.md') if (ROOT/'Docs/UNIVERSAL_ACCESS_UI_CONTRACT.md').exists() else ''
    custom_heads_doc=read('Docs/CUSTOM_HEADS.md') if (ROOT/'Docs/CUSTOM_HEADS.md').exists() else ''
    manual=read('Docs/MANUAL.md') if (ROOT/'Docs/MANUAL.md').exists() else ''
    build_release_doc=read('Docs/BUILD_AND_RELEASE.md') if (ROOT/'Docs/BUILD_AND_RELEASE.md').exists() else ''
    stage_release=read('scripts/stage_release.py') if (ROOT/'scripts/stage_release.py').exists() else ''
    license_text=read('LICENSE.txt') if (ROOT/'LICENSE.txt').exists() else ''
    third_party_notices=read('Licenses/THIRD_PARTY_NOTICES.txt') if (ROOT/'Licenses/THIRD_PARTY_NOTICES.txt').exists() else ''
    clap_binary_test=read('tests/test_clap_binary.cpp') if (ROOT/'tests/test_clap_binary.cpp').exists() else ''
    plugin_test=read('tests/test_plugin_processor.cpp') if (ROOT/'tests/test_plugin_processor.cpp').exists() else ''
    visual_snapshots=read('tests/visual_snapshots.cpp') if (ROOT/'tests/visual_snapshots.cpp').exists() else ''
    visual_ps=read('BUILD_VISUAL_SNAPSHOTS.ps1') if (ROOT/'BUILD_VISUAL_SNAPSHOTS.ps1').exists() else ''

    require(cmake,'project(SpaceTrace VERSION 1.0.0','1.0 RC project identity missing')
    require(cmake,'PRODUCT_NAME "SpaceTrace"','SpaceTrace product identity missing')
    require(cmake,'FORMATS VST3 Standalone','VST3/Standalone JUCE target missing')
    require(cmake,'clap_juce_extensions_plugin(TARGET SpaceTrace','CLAP wrapper must target the same SpaceTrace JUCE target')
    require(cmake,'CLAP_USE_JUCE_PARAMETER_RANGES ALL','CLAP must expose JUCE parameter ranges rather than generic 0-1 ranges')
    require(cmake,'CLAP_FEATURES audio-effect stereo','CLAP audio-effect/stereo feature metadata missing')
    require(cmake,'SPACETRACE_EXPECTED_VERSION=\\"${PROJECT_VERSION}\\"','CLAP binary test must derive expected version from the CMake project version')
    require(clap_binary_test,'SPACETRACE_EXPECTED_VERSION','CLAP binary test is not using the CMake-derived expected version')
    reject(clap_binary_test,'desc->version, "0.8.0"','stale hard-coded CLAP version assertion returned')
    require(cmake,'SPACETRACE_BUILD_VISUAL_SNAPSHOTS','developer visual snapshot CMake option missing')
    require(cmake,'spacetrace_visual_snapshots','developer visual snapshot target missing')
    require(visual_snapshots,'createComponentSnapshot','snapshot harness must use JUCE component snapshots')
    require(visual_snapshots,'juce::SoftwareImageType','snapshot harness must use deterministic software images')
    require(visual_snapshots,'snapshot image is uniform/blank','snapshot harness does not reject uniform images')
    require(visual_snapshots,'snapshot image is black','snapshot harness does not reject black images')
    require(visual_snapshots,'paramInputMode, 1.0f','snapshot harness does not select Stereo Pair through the real host parameter')
    for state in ('stereo_center_0','stereo_center_45','stereo_center_90','stereo_center_180','stereo_center_45_el30','stereo_center_270_elminus45','stereo_width_minus45','stereo_width_plus45','stereo_az45_width_plus20'):
        require(visual_snapshots, state, f'snapshot harness is missing required Stereo Pair state: {state}')
    require(visual_snapshots,'Real Windows 100/125/150/175/200% DPI validation remains a separate host test','snapshot harness must distinguish component scale from real Windows DPI')
    require(visual_ps,'SPACETRACE_BUILD_VISUAL_SNAPSHOTS=ON','visual snapshot build script does not enable the developer target')
    require(visual_ps,'SPACETRACE_BUILD_CLAP=OFF','visual snapshot build must not require CLAP')

    require(editor,'setAccessible(true);','editor root must be explicitly accessible')
    require(editor,'setTitle("SpaceTrace")','editor accessible title missing')
    require(editor,'setExplicitFocusOrder(focusOrder++)','explicit product focus order missing')
    reject(editor_h,'createFocusTraverser','do not add a custom focus traverser')
    for forbidden in ('WM_SETFOCUS','SetWindowSubclass','reaperPluginHostWrapProc','keyboardFocusContainer'):
        reject(editor+editor_h,forbidden,f'host/focus workaround forbidden: {forbidden}')

    require(editor_h,'AccessibleParameterSlider* exactTarget_ = nullptr;','exact-entry target missing')
    require(editor,'beginExactEntry(AccessibleParameterSlider& target)','exact-entry entry point missing')
    require(editor,'exactEditor_->grabKeyboardFocus();','exact editor must get focus when invoked')
    require(editor,'target->grabKeyboardFocus();','exact entry must restore invoking focus')
    require(editor,'target->setFromExactEntry(parsed);','exact commit must use slider value path')
    reject(editor,'AccessibilityHandler::postAnnouncement','editor must rely on native accessibility events rather than explicit speech announcements')
    reject(slider,'TextEditor','slider must not own a hidden/transient editor')
    reject(slider,'numberPadEnter','JUCE 8.0.12 has no KeyPress::numberPadEnter constant')
    require(slider,'juce::KeyPress::homeKey','slider Home key support missing')
    require(slider,'juce::KeyPress::endKey','slider End key support missing')
    require(slider,'juce::KeyPress::pageUpKey','slider Page Up support missing')
    require(slider,'juce::KeyPress::pageDownKey','slider Page Down support missing')
    require(editor,'azimuth_.setKeyboardPageStep(10.0);','Azimuth coarse keyboard step missing')
    require(editor,'elevation_.setKeyboardPageStep(10.0);','Elevation coarse keyboard step missing')
    require(combo,'juce::KeyPress::homeKey','combo Home key support missing')
    require(combo,'juce::KeyPress::endKey','combo End key support missing')
    require(editor_h,'AccessibleComboBox renderer_;','renderer must use accessible combo keyboard semantics')
    require(editor_h,'AccessibleComboBox inputMode_;','Input Mode must use accessible combo keyboard semantics')
    require(editor_h,'AccessibleComboBox dataset_;','head selector must use accessible combo keyboard semantics')
    require(editor_h,'AccessibleComboBox compensation_;','tone selector must use accessible combo keyboard semantics')
    require(editor,'compensation_.addItem("Custom Correction IR", 3);','user-facing custom correction identity missing')
    require(editor,'loadCustomIR_.setTitle("Load Custom Correction IR")','custom correction load control title missing')
    reject(editor,'compensation_.addItem("Custom IR", 3);','ambiguous Custom IR label returned')
    require(editor,'renderer_.addItem("Modern HRTF", 1);','renderer selector missing Modern HRTF')
    require(editor,'renderer_.addItem(u8"Papa Pan \\u2014 historical", 2);','renderer selector missing Papa Pan')
    require(editor,'inputMode_.addItem("Mono Point Source", 1);','Input Mode missing Mono Point Source')
    require(editor,'inputMode_.addItem("Stereo Pair", 2);','Input Mode missing Stereo Pair')
    require(editor,'dataset_.setTitle("Head Model");','head selector title must remain Head Model')
    require(editor,'dataset_.setExplicitFocusOrder(focusOrder++);\n    renderer_.setExplicitFocusOrder(focusOrder++);\n    inputMode_.setExplicitFocusOrder(focusOrder++);','Head Model must be the first structural control in keyboard order')
    require(editor,'airLoss_.setButtonText("Air Loss");','Air Loss toggle must have a visible label')
    require(editor,'inputMode_.setEnabled(!papaMode);','Papa Pan must disable Stereo Pair selection in the custom UI')
    require(editor,'widthOffset_.setEnabled(stereoPairActive);','Width Offset must be enabled only for active Modern HRTF Stereo Pair')
    require(editor,'Width Offset (Stereo Pair only)','Width Offset needs an explicit stereo-only visible label')
    require(editor,'dataset_.setEnabled(!papaMode);','Papa Pan must disable the irrelevant head selector')
    require(editor,'elevation_.setEnabled(!papaMode);','Papa Pan must disable the unsupported elevation control')
    for x in ('mouseDown(','mouseDrag(','mouseWheelMove('): reject(slider,x,'mouse behavior must remain JUCE-owned')

    for param in ('paramAzimuth','paramElevation','paramDistance','paramInputMode','paramWidthOffset','paramAirLoss','paramOutputDb','paramBypass'):
        require(processor_h,param,f'host parameter missing: {param}')
    reject(processor_h,'paramDataset','HRTF set must remain non-automatable structural state')
    reject(processor_h,'paramCompensationMode','compensation mode must remain non-automatable structural state')
    reject(processor_h,'paramRendererMode','renderer mode must remain non-automatable structural state')
    require(processor_h,'RendererMode::ModernHRTF','renderer mode structural state missing')
    require(processor_h,'activeRendererModeIndex_','prepared renderer publication state missing')
    require(catalog,'BuiltInDatasetId::IrcamListen1050','IRCAM catalog entry missing')
    require(catalog,'BuiltInDatasetId::MitKemarNormalPinna','KEMAR catalog entry missing')
    require(catalog,'BuiltInDatasetId::Sadie2D1Ku100','SADIE II D1 / KU100 catalog entry missing')
    require(catalog,'BuiltInDatasetId::ThkKu100Full2Deg','TH Köln KU100 FULL2DEG catalog entry missing')
    require(catalog,'BuiltInDatasetId::FabianHato0','FABIAN HATO 0 catalog entry missing')
    require(editor,'dataset_.addItem(u8"TH K\\u00f6ln KU100 FULL2DEG", 4);','FULL2DEG head selector entry missing')
    require(editor,'dataset_.addItem("FABIAN HATO 0", 5);','FABIAN head selector entry missing')
    require(catalog,'true,','catalog no longer marks a default')
    require(read('core/include/spacetrace/DatasetCatalog.h'),'builtInDatasetCount = 5','five-head catalog count missing')

    require(processor_h,'stateSchemaVersion = 5','project-state schema version missing')
    require(plugin_test,'schema-3 migration did not default Input Mode to Mono Point Source','schema-3 Input Mode migration regression missing')
    require(plugin_test,'schema-3 migration did not default Air Loss on','schema-3 Air Loss migration regression missing')
    require(plugin_test,'schema-4 migration did not default Stereo Pair Width Offset to zero','schema-4 Width Offset migration regression missing')
    require(plugin_test,'Stereo Pair Width Offset did not change the rendered source geometry','Width Offset processor integration regression missing')
    require(plugin_test,'Stereo Pair zero-width display geometry changed','Stereo Pair geometry regression must distinguish canonical and display azimuths')
    require(clap_binary_test,'expected eight automatable controls','CLAP regression must cover the eighth Width Offset control')
    require(clap_binary_test,'Stereo Pair Width Offset','CLAP regression must discover Width Offset by name')
    require(clap_binary_test,'info.min_value == -45.0 && info.max_value == 45.0','CLAP regression must verify Width Offset range')
    require(processor,'juce::ValueTree root("SpaceTraceState")','single project state root missing')
    require(processor,'root.setProperty("schemaVersion", stateSchemaVersion','project-state schema version is not serialized')
    require(processor,'schemaVersion > stateSchemaVersion','future incompatible project state is not rejected')
    require(processor,'root.addChild(parameters_.copyState()','parameter state not stored')
    require(processor,'root.addChild(structuralState_.createCopy()','structural state not stored')
    require(processor,'encodeCustomIR(customCompensation_)','custom IR not stored in project state')
    require(processor,'decodeCustomIR(*data, customCompensation_)','custom IR not restored from project state')
    require(processor,'structuralState_.setProperty("rendererMode"','renderer mode is not stored in project state')
    require(processor,'rendererModeIndex_.store(renderer','renderer mode is not restored from project state')
    require(processor,'structuralState_.setProperty("headHash"','selected head content hash is not stored in project state')
    require(processor,'restoredHeadHash_ = structuralState_.getProperty("headHash")','saved head hash is not restored for provenance checking')
    require(processor_h,'unresolvedHeadId_','unknown saved head identity is not retained')
    require(processor,'status_ = "Referenced head is unavailable: " + unresolvedHeadId_','unknown saved head must produce an explicit accessible error')
    require(processor,'unresolvedHeadId_ = knownDataset ? juce::String() : dataset','unknown saved head must not silently substitute IRCAM')

    for cached in ('azimuthValue_','elevationValue_','distanceValue_','inputModeValue_','widthOffsetValue_','airLossValue_','outputDbValue_','bypassValue_'):
        require(processor_h,cached,f'cached realtime parameter pointer missing: {cached}')
    require(processor,'parameters_.getRawParameterValue(paramAzimuth)','parameter cache initialization missing')
    process=method_body(processor,'void SpaceTraceAudioProcessor::processBlock','RendererMode SpaceTraceAudioProcessor::rendererMode')
    for forbidden in ('new ','make_unique','std::vector','ScopedLock','CriticalSection','mutex','File','FileChooser','AudioFormat','MessageManager','triggerAsyncUpdate','suspendProcessing','loadImpulseResponse','prepare(','setValueNotifyingHost'):
        reject(process,forbidden,f'real-time processBlock contains forbidden operation: {forbidden}')
    reject(process,'getRawParameterValue','audio callback must use cached parameter atomics')
    require(processor_h,'getBypassParameter() const override','processor must expose its existing bypass parameter to wrappers')
    require(processor,'return bypassParameter_;','wrapper bypass must reuse the SpaceTrace bypass parameter')
    reject(process,'1.0f / juce::jmax(0.25f, distance)','Distance must not regress to gain-only inverse scaling')
    require(process,'DistanceModel::levelGain(distance)','Distance must use the shared perceptual distance model')
    require(process,'DistanceModel::airHighGain(distance)','Distance must include the bounded air-loss target')
    require(process,'airLossValue_->load','Air Loss must be consumed from its cached host parameter')
    require(process,'stereoPairMode = !papaMode','Stereo Pair must be impossible in Papa Pan')
    require(process,'stereoPairGeometry(static_cast<double>(az)','Stereo Pair DSP must use the shared geometry helper')
    require(process,'widthOffsetValue_->load','Stereo Pair Width Offset must be consumed from its cached host parameter')
    require(read('plugin/StereoPairGeometry.h'),'centreAzimuthDegrees + 90.0 + width','Stereo Pair Left geometry must derive from centre +90 + width')
    require(read('plugin/StereoPairGeometry.h'),'centreAzimuthDegrees - 90.0 - width','Stereo Pair Right geometry must derive from centre -90 - width')
    require(process,'stereoPairRenderer_.process','Stereo Pair secondary Modern HRTF renderer is not wired')
    require(processor_h,'stereoPairLeftCompensation_','Stereo Pair left correction convolver missing')
    require(processor_h,'stereoPairRightCompensation_','Stereo Pair right correction convolver missing')
    require(process,'stereoPairLeftCompensation_.process','Stereo Pair left correction is not applied before summing')
    require(process,'stereoPairRightCompensation_.process','Stereo Pair right correction is not applied before summing')
    require(process,'!usedStereoPairCorrection','Stereo Pair corrected path must bypass the old post-sum correction when side assets are active')
    require(processor_h,'activeStereoPairTrimLinear_','Stereo Pair fixed reference-level calibration publication missing')
    require(process,'activeStereoPairTrimLinear_.load','Stereo Pair fixed reference-level calibration is not applied')
    require(process,'stereoPairMode != wasStereoPairMode_','Input Mode transitions must be edge-detected')
    require(process,'stereoPairRenderer_.reset();','Input Mode transitions must clear stale secondary renderer history')
    require(distance,'(d - referenceDistance) * airLossDbPerMetreBeyondReference','conservative synthetic air-loss law changed')
    require(distance,'return low + std::clamp(highGain, 0.0f, 1.0f) * (input - low);','distance air-loss split changed')
    require(read('tests/test_core.cpp'),'1 m distance air-loss stage is not spectrally neutral','explicit 1 m spectral-neutrality regression is missing')
    require(read('tests/test_core.cpp'),'disabled air-loss stage changed the spectrum','disabled Air Loss direct-DSP regression is missing')
    require(process,'if (!wasBypassed_)','bypass transition must be edge-detected')
    require(process,'hrtfRenderer_.reset();','bypass entry must clear stale HRTF history')
    require(process,'commonCompensation_.reset();','bypass entry must clear stale compensation history')

    require(processor_h,'maxCustomCompensationSeconds = 0.25','custom compensation duration limit must be explicit')
    require(processor_h,'getTailLengthSeconds() const override { return 0.40; }','host tail declaration must cover composed stereo correction + HRTF tail')
    require(processor,'durationSeconds > maxCustomCompensationSeconds','custom compensation loader must enforce the declared FIR duration limit')
    reject(processor_h,'dryScratch_','unused dry buffer scaffolding must not return')
    require(processor,'SpaceTraceAudioProcessor::~SpaceTraceAudioProcessor()','explicit processor teardown missing')
    destructor=method_body(processor,'SpaceTraceAudioProcessor::~SpaceTraceAudioProcessor()','juce::AudioProcessorValueTreeState::ParameterLayout SpaceTraceAudioProcessor::createParameterLayout')
    require(destructor,'suspendProcessing(true);','teardown must exclude in-flight host processing')
    require(destructor,'releaseResources();','teardown must clear DSP state before member destruction')

    require(processor_h,'activeCompensationModeIndex_','active compensation publication state missing')
    require(process,'activeCompensationModeIndex_.load','audio callback must use prepared compensation mode')
    require(process,'activeRendererModeIndex_.load','audio callback must use prepared renderer mode')
    require(process,'rendererReady_.load','audio callback must refuse processing when the referenced external head is unavailable')
    reject(process,'selectedDatasetIndex_.load','audio callback must not consume unprepared requested dataset state')
    reject(process,'rendererMode()','audio callback must not consume unprepared requested renderer state')
    require(process,'papaPanRenderer_.process','Papa Pan audio path is not wired')
    reject(process,'compensationMode()','audio callback must not consume unprepared requested compensation state')
    reject(processor_h,'AsyncUpdater','processor must not own queued structural callbacks during teardown')
    reject(processor,'triggerAsyncUpdate','processor structural changes must not queue lifetime-sensitive callbacks')
    require(processor,'prepareStructuralState(true);','structural changes must prepare synchronously outside audio callback')
    require(processor,'activeDatasetIndex_.store(requestedDataset','dataset publication must follow preparation')
    select_body=method_body(processor,'void SpaceTraceAudioProcessor::selectBuiltInDataset','CompensationMode SpaceTraceAudioProcessor::compensationMode')
    require(select_body,'prepareStructuralState(true);','every dataset change must publish a prepared structural pair, including Raw mode')
    require(processor,'activeCompensationModeIndex_.store','effective compensation mode publication missing')
    reject(processor_h,'sourcePackages_','processor must not own every source head package')
    reject(processor_h,'runtimeDatasets_','processor must not prepare every installed head per instance')
    reject(processor,'BinaryData::','external head architecture must not embed HRTF data in the plug-in')
    reject(cmake,'juce_add_binary_data','external head architecture must not compile HRTF data into the plug-in')
    require(cmake,'plugin/HeadRepository.cpp','external head repository is not part of the plug-in target')
    require(cmake,'juce::juce_cryptography','SHA-256 head verification requires the JUCE cryptography module')
    require(head_repo,'<juce_cryptography/juce_cryptography.h>','HeadRepository must include JUCE cryptography for SHA-256')
    require(cmake,'tests/validate_heads.py','external head package validator is not registered')
    require(head_repo,'SPACETRACE_HEADS_DIR','portable head locator/test override missing')
    require(head_repo,'getChildFile("Heads")','portable Heads/ discovery missing')
    require(head_repo,'preparedCache_','prepared head cache missing')
    require(head_repo_h,'std::shared_ptr<const PreparedHeadData>','prepared head data must be immutable/shared across instances')
    require(head_repo_h,'std::shared_ptr<const DatasetPackage>','parsed source head cache must be immutable/shared across instances')
    require(head_repo,'resampleHRTFSet(source->hrtf, hostSampleRate)','host-rate head preparation path missing')
    require(head_repo,'resampleCompensation(correction, hostSampleRate)','host-rate dataset correction preparation path missing')
    require(head_repo,'levelTrimDb','per-head fixed level trim metadata missing')
    require(processor_h,'activeHeadTrimLinear_','audio thread publication for per-head level trim missing')
    require(process,'activeHeadTrimLinear_.load','per-head level trim is not applied in the audio path')
    require(head_repo,'leftGainDb','per-head left channel calibration metadata missing')
    require(head_repo,'rightGainDb','per-head right channel calibration metadata missing')
    require(head_repo,'stereoPairLeftCorrectionFile','Stereo Pair left correction manifest support missing')
    require(head_repo,'stereoPairRightCorrectionFile','Stereo Pair right correction manifest support missing')
    require(head_repo_h,'struct StereoPairResources','Stereo Pair prepared resources must be grouped in one structure')
    require(head_repo_h,'StereoPairResources stereoPair;','prepared head must own one grouped Stereo Pair resource object')
    require(head_repo,'stereoPairTrimDb','Stereo Pair fixed level calibration metadata missing')
    require(processor_h,'activeHeadLeftGainLinear_','audio thread publication for per-head left channel calibration missing')
    require(processor_h,'activeHeadRightGainLinear_','audio thread publication for per-head right channel calibration missing')
    require(process,'activeHeadLeftGainLinear_.load','per-head left calibration is not applied in the audio path')
    require(process,'activeHeadRightGainLinear_.load','per-head right calibration is not applied in the audio path')
    require(head_repo,'buildMutex_','parallel cache misses must be serialized to avoid duplicate multi-instance preparation')

    # Reproducible user-facing SOFA -> SpaceTrace head toolchain.
    require(cmake,'spacetrace_head_tool_tests','custom-head script failure-mode regression suite is not registered')
    require(head_converter,'--stable-id','custom-head converter must require stable identity')
    require(head_converter,'provenance.json','custom-head converter must preserve source provenance')
    require(head_validator,'Head SHA-256 mismatch','custom-head validator must diagnose hash mismatch')
    require(correction_normalizer,'pink-noise RMS neutral','correction normalizer must document its level-neutral criterion')
    require(head_packager,'--level-trim-db','head packager must keep loudness trim separate from correction')
    require(head_packager,'--calibrate-front-center','head packager must support explicit front-center channel calibration')
    require(stereo_pair_prep,'Stereo Pair preparation: PASS','Stereo Pair preparation tool missing or incomplete')
    require(stereo_pair_prep,'90/270 deg Raw source legs','Stereo Pair calibration provenance must name its reference geometry')
    require(stereo_pair_prep,'equal-and-opposite source balance','Stereo Pair source balance must remain a fixed neutral calibration')
    require(head_validator,'Stereo Pair corrections: PASS','head validator must verify optional Stereo Pair correction assets')
    require(channel_analyzer,'--write-manifest','front-center channel analyzer must support audited manifest calibration')
    require(correction_plotter,'machine-generated text description','correction plotter must generate an auditable text description')
    require(correction_plotter,'Tone Trace .ttm','correction plotter must support Tone Trace models')
    require(correction_plotter,'correction WAV','correction plotter must support final correction WAVs')
    require(scripts_readme,'SOFA is the archival/interchange source format','scripts documentation must explain the SOFA/runtime-format decision')
    require(scripts_readme,'0 degrees, front-center','custom-head correction workflow must document front-center Tone Trace target')
    require(scripts_readme,'prepare_stereo_pair.py','custom-head workflow must document Stereo Pair preparation')
    require(custom_heads_doc,'stereo_pair_left_correction.wav','custom-head guide must document packaged Stereo Pair correction assets')
    require(requirements,'numpy','scripts/requirements.txt must list NumPy')
    require(requirements,'h5py','scripts/requirements.txt must list h5py')
    require(requirements,'matplotlib','scripts/requirements.txt must list matplotlib for correction documentation plots')
    require(manual,'0° and 360° are the same physical position','manual must explain the azimuth endpoint/direction convention')
    require(manual,'https://github.com/dgl1984/ToneTrace/releases','manual must tell users where to obtain Tone Trace')
    require(manual,'one parameter, one user-facing control','manual must explain the universal-access control contract')
    require(manual,'TH Köln / Bernschütz FULL2DEG KU100','manual must document all five shipping heads')
    require(manual,'python scripts/plot_correction.py','manual must document user-generated correction plots')
    require(manual,'Page Up/Page Down','manual must document the accessibility coarse-step keys')
    require(manual,'Shared head loading','manual must document the shared immutable head cache')
    require(manual,'Sample-rate handling','manual must document host-rate preparation')
    require(manual,'Apache License 2.0 with the Commons Clause License Condition v1.0','manual must state the first-party licensing decision')
    require(build_release_doc,'same `SpaceTrace` JUCE target','build docs must forbid divergent VST3/CLAP implementations')
    require(stage_release,'PUBLIC_TOP_LEVEL','release staging must use an explicit public-root whitelist')
    require(stage_release,'PUBLIC_DOC_FILES','release staging must whitelist public documentation')
    require(stage_release,'PUBLIC_SCRIPT_FILES','release staging must whitelist user-facing custom-head scripts')
    require(stage_release,'THIRD_PARTY_NOTICES.txt','release staging must require third-party notices')
    reject(build_ps,"Copy-Item -LiteralPath (Join-Path $Root 'Docs')",'BUILD_WINDOWS must not copy the whole Docs tree into the public package')
    reject(build_ps,"Copy-Item -LiteralPath (Join-Path $Root 'third_party')",'BUILD_WINDOWS must not ship the source third_party folder wholesale')
    require(build_ps,'stage_release.py','BUILD_WINDOWS must use the tested release staging script')
    require(license_text,'Commons Clause License Condition v1.0','first-party SpaceTrace license must match the selected Tone Trace model')
    require(license_text,'Apache License 2.0','first-party license base must be Apache 2.0')
    require(license_text,'does not relicense third-party components','first-party license must explicitly exclude third-party material')
    require(third_party_notices,'FULL2DEG','third-party notices must cover FULL2DEG')
    require(third_party_notices,'CC BY-SA 3.0','FULL2DEG ShareAlike treatment must be explicit')
    require(third_party_notices,'FABIAN','third-party notices must cover FABIAN')
    require(third_party_notices,'JUCE 8.0.12','third-party notices must identify the pinned JUCE dependency')
    for head_dir in ('IRCAM_1050','MIT_KEMAR_Normal','KU100_SADIE_D1','KU100_FULL2DEG','FABIAN_HATO0'):
        if not (ROOT/'Heads'/head_dir/'provenance.json').is_file():
            raise AssertionError(f'shipping head provenance missing: {head_dir}')

    # Universal-access visual foundation: one parameter, one real control.
    require(ui_contract,'One parameter, one user-facing control','universal-access single-control contract is missing')
    require(ui_contract,'Visualisations are output-only','read-only visualisation contract is missing')
    require(ui_contract,'No explicit text-to-speech layer','no-parallel-TTS accessibility contract is missing')
    require(cmake,'plugin/SpatialDisplay.cpp','read-only spatial visualisation is not part of the plug-in target')
    require(editor_h,'SpatialDisplay spatialDisplay_','editor is missing the read-only spatial display')
    require(spatial,'setAccessible(false)','spatial visualisation must be excluded from the accessibility hierarchy')
    require(spatial,'setWantsKeyboardFocus(false)','spatial visualisation must never take keyboard focus')
    require(spatial,'setInterceptsMouseClicks(false, false)','spatial visualisation must not be a mouse control')
    require(spatial_h,'void setViewState','spatial visualisation needs a read-only state update API')
    reject(spatial,'mouseDown(','spatial visualisation must not implement mouse editing')
    reject(spatial,'mouseDrag(','spatial visualisation must not implement mouse editing')
    reject(spatial,'keyPressed(','spatial visualisation must not implement keyboard editing')
    reject(spatial,'setValueNotifyingHost','spatial visualisation must not write host parameters')
    require(spatial,'stereoPairGeometry(static_cast<double>(azimuthDegrees_)','spatial display must use the shared Stereo Pair geometry helper')
    reject(spatial,'beginChangeGesture','spatial visualisation must not start automation gestures')
    reject(spatial,'endChangeGesture','spatial visualisation must not end automation gestures')
    reject(editor, 'startTimerHz(30)', 'visual display must not use a 30 Hz processor-polling timer')
    require(editor, 'startTimerHz(4)', 'editor structural/status refresh must remain low-rate')
    require(editor, 'azimuth_.onValueChange', 'spatial display must follow the real Azimuth control')
    require(editor, 'elevation_.onValueChange', 'spatial display must follow the real Elevation control')
    require(editor, 'distance_.onValueChange', 'spatial display must follow the real Distance control')

    # refreshSpatialDisplay must be a view of the real UI controls, not a new processor
    # polling path. This is both a universal-access and teardown-lifetime invariant.
    refresh_match = re.search(r'void\s+SpaceTraceAudioProcessorEditor::refreshSpatialDisplay\(\)\s*\{(?P<body>.*?)\n\}', editor, re.S)
    if refresh_match is None:
        raise AssertionError('refreshSpatialDisplay implementation missing')
    refresh_body = refresh_match.group('body')
    reject(refresh_body, 'processor_.', 'spatial display refresh must not dereference the processor')
    if editor.count('SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramAzimuth') != 1:
        raise AssertionError('Azimuth must have exactly one editor control attachment')
    if editor.count('SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramElevation') != 1:
        raise AssertionError('Elevation must have exactly one editor control attachment')
    if editor.count('SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramDistance') != 1:
        raise AssertionError('Distance must have exactly one editor control attachment')
    if editor.count('ComboBoxAttachment>(apvts, SpaceTraceAudioProcessor::paramInputMode') != 1:
        raise AssertionError('Input Mode must have exactly one editor control attachment')
    if editor.count('ButtonAttachment>(apvts, SpaceTraceAudioProcessor::paramAirLoss') != 1:
        raise AssertionError('Air Loss must have exactly one editor control attachment')
    reject(processor,'juce::jlimit(0, 1,', 'processor still contains a two-dataset clamp')

    require(realtime_h,'bool positionDirty_ = true','realtime position dirty-state cache missing')
    require(realtime,'if (positionDirty_)','position interpolation is not demand-driven')
    require(realtime,'positionDirty_ = false','position interpolation dirty flag is never cleared')
    require(realtime,'collapseTransitionToCurrent()','continuous-motion retarget collapse is missing')
    set_hrtf=method_body(realtime,'void RealtimeFIRRenderer::setHRTF','void RealtimeFIRRenderer::setPosition')
    require(set_hrtf,'collapseTransitionToCurrent();','dataset switch does not preserve an audible in-progress transition')
    require(realtime,'if (transitionRemaining_ > 0) collapseTransitionToCurrent();','retarget does not start from the audible in-progress state')
    require(realtime,'auto ringBlend =','grid-aware azimuth interpolation missing')
    require(realtime,'lowerEl','grid-aware elevation bracketing missing')
    require(realtime,'upperEl','grid-aware elevation bracketing missing')
    require(realtime,'const std::array<WeightedMeasurement, 4> corners','bilinear spherical-grid interpolation missing')
    require(realtime,'scratchLeft_','preallocated interpolation scratch path missing')
    reject(realtime,'set_->nearestMeasurementIndex(position_)','realtime renderer regressed to nearest-neighbor snapping')
    reject(realtime,'targetIndex_ == index || currentIndex_ == index','old reversal-blocking transition guard returned')

    require(papa_h,'sectorCount = 24','Papa Pan must use the documented 24 horizontal sectors')
    require(papa_h,'referenceFramesPerStep = 256','Papa Pan historical movement cadence missing')
    require(papa,'sector * 15','Papa Pan 15-degree sector mapping missing')
    require(papa,'shortestStep','Papa Pan one-sector catch-up logic missing')
    require(papa,'frontGainForAzimuth','Papa Pan historical front-gain contour missing')
    require(papa,'sampleRate * static_cast<double>(referenceFramesPerStep) / referenceSampleRate','Papa Pan cadence must be time-preserved across sample rates')
    reject(papa,'RealtimeFIRRenderer','Papa Pan must remain a separate renderer, not mutate Modern HRTF interpolation')
    require(resample,'acc *= sourceRate / targetRate;','raw FIR resampling is missing convolution-gain scaling')
    require(resample,'resampleMinimumPhaseImpulse','minimum-phase FIR resampling path missing')
    require(resample,'Real-cepstrum minimum-phase reconstruction','minimum-phase spectral reconstruction missing')
    custom_load=method_body(processor,'bool SpaceTraceAudioProcessor::loadCustomCompensationIR','void SpaceTraceAudioProcessor::updateStructuralStateProperties')
    require(custom_load,'if (compensationMode() == CompensationMode::CustomIR)','custom IR replacement does not detect active CustomIR mode')
    require(custom_load,'prepareStructuralState(true);','active CustomIR replacement does not reload convolution')

    if build_ps:
        require(build_ps,'SpaceTrace','Windows build identity missing')
        require(build_ps,'Prepare-SADIE-D1.ps1','Windows build does not prepare the official KU100 package when absent/stale')
        require(build_ps,'check_native_package.py','Windows build does not reject a stale Raw KU100 package')
        require(build_ps,"--compensation-version 'tonetrace-raw-v1'",'Windows stale-package check does not require the current KU100 correction identity')
        require(build_ps,"-Python 'python'",'Windows build and SADIE preparation do not share the verified Python command')
        require(build_ps,'SPACETRACE_BUILD_CLAP=ON','Windows release path must build CLAP from the shared JUCE target')
        require(build_ps,'FETCH_CLAP_WINDOWS.ps1','Windows build must restore the pinned CLAP wrapper when absent')
        require(build_ps,'SpaceTrace_CLAP','Windows build does not build the CLAP target')
        require(build_ps,'SpaceTrace.clap','portable staging does not include the CLAP artifact')
        require(build_ps,'spacetrace_plugin_tests','Windows build does not compile processor integration tests')
        require(build_ps,'validate_heads.py','Windows build does not validate the portable head packages')
        require(build_ps,'--require-all','Windows build does not require all release heads')
        require(build_ps,'SpaceTrace_VST3','Windows build does not build the VST3 target')
        require(build_ps,'Nothing was installed automatically','non-installing build contract missing')
        reject(build_ps,'C:\\Program Files\\Common Files\\VST3','build script must not install into system VST3 folders')
    if fetch_ps: require(fetch_ps,'8.0.12','JUCE fetch script is not pinned to 8.0.12')
    if fetch_clap_ps:
        require(fetch_clap_ps,'55525c9858d4b25687be7759a5e0f70eccef218e','CLAP wrapper revision is not pinned')
        require(fetch_clap_ps,'29ffcc273be7c7c651f6c9953b99e69700e2387a','CLAP API revision is not pinned')
        require(fetch_clap_ps,'a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc','CLAP helpers revision is not pinned')
        require(fetch_clap_ps,'SPACETRACE_PINNED_REVISIONS.txt','CLAP fetch must leave a human-readable revision record')
    if datasets_ps:
        require(datasets_ps,"importlib.util.find_spec('h5py')",'built-in dataset Python dependency probe is not PowerShell-5.1-safe')
        reject(datasets_ps,'import h5py, numpy','built-in dataset dependency probe can emit a terminating traceback on PowerShell 5.1')
        require(datasets_ps,'[System.IO.File]::OpenRead($Path)','SOFA signature validation should not load an entire large file into PowerShell memory')
    if sadie_ps:
        require(sadie_ps,'4850c1eb8e63e2d4f605edcdb4d5c883','SADIE D1 archive MD5 pin missing')
        require(sadie_ps,'select_sadie_d1_sofa.py','SADIE build still depends on a brittle archive-internal filename')
        require(sadie_ps,'validate_builtins.py','generated KU100 package is not source-validated before build')
        require(sadie_ps,'--ku100-sofa','KU100 validator is not given the official extracted SOFA')
        require(sadie_ps,'--ku100-package','KU100 validator is not given the generated native package')
        require(sadie_ps,'SADIE2_D1_KU100_DatasetCorrected_ToneTrace.wav','KU100 measured correction asset is not wired into preparation')
        require(sadie_ps,'--compensation-version "tonetrace-raw-v1"','KU100 native package correction version is not frozen')
        require(sadie_ps,'--ku100-compensation','KU100 validator is not given the measured correction WAV')
    if sadie_selector:
        require(sadie_selector,'ir.shape == (8802, 2, 256)','SADIE selector does not identify D1 by expected geometry')
        require(sadie_selector,'44100.0','SADIE selector does not require the 44.1 kHz representation')
    if validator:
        require(validator,'def validate_ku100','KU100 source/package validator missing')
        require(validator,"'SADIE II D1 / Neumann KU100',8802,256,1.2",'KU100 validation identity/geometry contract missing')
        require(validator,'KU100: raw HRIR copy mismatch','KU100 validator does not compare packaged HRIRs with the official SOFA')
        require(validator,"pkg['meta']['comp_version']!='tonetrace-raw-v1'",'KU100 validator does not require the measured correction identity')
        require(validator,'correction samples differ from the measured Tone Trace asset','KU100 validator does not compare packaged correction samples with the measured WAV')
    if package_checker:
        require(package_checker,'--require-compensation','fast native-package checker does not require compensation')
        require(package_checker,'--compensation-version','fast native-package checker cannot reject stale correction metadata')
        reject(package_checker,'import numpy','fast package checker must stay standard-library-only')
        reject(package_checker,'import h5py','fast package checker must stay standard-library-only')
    if plugin_test:
        require(plugin_test,'std::unique_ptr<juce::OutputStream> stream','JUCE 8 writer test must use OutputStream ownership type')
        require(plugin_test,'for (int pass = 0; pass < 64; ++pass)','processor lifecycle stress loop missing')
        require(plugin_test,'stressed.reset();','lifecycle stress must exercise processor destructor directly')
        require(plugin_test,'stressed->createEditor()','lifecycle stress must include editor construction/destruction')
        require(plugin_test,'stressedEditor.reset();','lifecycle stress must destroy editor before processor')
    if first_test:
        require(first_test,'SpaceTrace','Windows manual test document is stale')
        require(first_test,'Do not patch files by hand','first-test recovery instruction missing')
        require(first_test,'Removal crash blocker','known REAPER teardown crash is missing from the manual blocker checklist')
        require(first_test,'SADIE II D1 / Neumann KU100','third HRTF is missing from the manual validation checklist')
        require(first_test,'Custom Correction IR','custom correction semantics are missing from the manual checklist')

    require(requirements,'numpy','scripts/requirements.txt must pin NumPy')
    require(requirements,'h5py','scripts/requirements.txt must pin h5py')
    require(scripts_readme,'HOW TO FIX:','script documentation must promise actionable failure guidance')
    require(scripts_readme,'unprocessed pink noise','custom-head correction workflow missing pink-noise reference guidance')
    require(custom_heads_doc,'.sthrtf','custom-head documentation must explain the native runtime format')
    require(custom_heads_doc,'SOFA remains','custom-head documentation must preserve SOFA as archival source')
    require(custom_heads_doc,'0 degrees','custom-head documentation must specify front-center correction capture')
    require(cmake,'spacetrace_head_tool_tests','script regression suite is not wired into CTest')

    # These checks must run before main returns, using the same helper signature.
    require(editor, 'resizableCorner->setAccessible(false)',
            'Resizable corner must be excluded from accessibility')
    require(editor, 'setResizeLimits(820, 648, 1280, 920)',
            'Minimum editor height must preserve Bypass/Status with exact-entry error visible')

    print('PASS: SpaceTrace 1.0 RC6 WIP2b stereo-pair-width source contract')
    return 0

if __name__=='__main__':
    try: raise SystemExit(main())
    except Exception as exc:
        print(f'FAIL: {exc}',file=sys.stderr); raise SystemExit(1)
