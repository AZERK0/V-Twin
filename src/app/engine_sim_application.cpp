#include "app/engine_sim_application.h"

#include "simulation/simulator.h"
#include "shared/utilities.h"
#include "ui/engine_view.h"
#include "ui/info_cluster.h"
#include "ui/oscilloscope_cluster.h"
#include "ui/performance_cluster.h"
#include "units.h"

#include <delta-studio/include/yds_error_handler.h>

#include "build_info.h"

#include <chrono>
#include <cstdlib>
#include <fstream>

#if ATG_ENGINE_SIM_DISCORD_ENABLED
#include "integration/discord.h"
#endif

namespace {
    constexpr int AudioSampleRate = 44100;
    constexpr double AudioBufferLeadSeconds = 0.1;
    constexpr double AudioResetLeadSeconds = 0.05;

    int leadToSamples(double seconds) {
        return static_cast<int>(AudioSampleRate * seconds);
    }
}

std::string EngineSimApplication::s_buildVersion = ENGINE_SIM_PROJECT_VERSION "a" "-" ENGINE_SIM_SYSTEM_NAME;

struct LoggingErrorHandler : ysErrorHandler {
    void OnError(ysError error, unsigned int line, ysObject *object, const char *file) override {
        printf("Error @ %s:%i - %i\n", file, line, static_cast<int>(error));
    }
};

EngineSimApplication::EngineSimApplication() {
    m_assetPath = "";

    m_geometryVertexBuffer = nullptr;
    m_geometryIndexBuffer = nullptr;

    m_paused = false;
    m_recording = false;
    m_screenResolutionIndex = 0;
    for (int i = 0; i < ScreenResolutionHistoryLength; ++i) {
        m_screenResolution[i][0] = m_screenResolution[i][1] = 0;
    }

    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);

    m_displayHeight = static_cast<float>(units::distance(2.0, units::foot));
    m_outputAudioBuffer = nullptr;
    m_audioSource = nullptr;

    m_torque = 0;
    m_dynoSpeed = 0;

    m_simulator = nullptr;
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_temperatureGauge = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    m_iceEngine = nullptr;
    m_mainRenderTarget = nullptr;

    m_vehicle = nullptr;
    m_transmission = nullptr;

    m_oscillatorSampleOffset = 0;
    m_gameWindowHeight = 256;
    m_screenWidth = 256;
    m_screenHeight = 256;
    m_screen = 0;
    m_viewParameters.Layer0 = 0;
    m_viewParameters.Layer1 = 0;

    m_displayAngle = 0.0f;

    ysErrorSystem::GetInstance()->AttachErrorHandler(&m_error_handler);
}

EngineSimApplication::~EngineSimApplication() {
}

void EngineSimApplication::initialize(void *instance, ysContextObject::DeviceAPI api) {
    dbasic::Path modulePath = dbasic::GetModulePath();

    if (getenv("ENGINE_SIM_DATA_ROOT") != nullptr) {
        m_dataRoot = getenv("ENGINE_SIM_DATA_ROOT");
    }
    else {
        m_dataRoot = ENGINE_SIM_DATA_ROOT;
    }

    if (!m_dataRoot.IsAbsolute()) {
        m_dataRoot = modulePath.Append(m_dataRoot).GetAbsolute();
    }

    if (getenv("XDG_DATA_HOME") != nullptr) {
        m_userData = getenv("XDG_DATA_HOME");
    }
    else if (getenv("HOME") != nullptr) {
        m_userData = dbasic::Path(getenv("HOME")).Append(".local/share/engine-sim");
    }
    else {
        m_userData = modulePath;
    }

    if (!m_userData.Exists()) {
        m_userData.CreateDir();
    }

    if (getenv("XDG_STATE_HOME") != nullptr) {
        m_outputPath = getenv("XDG_STATE_HOME");
    }
    else if (getenv("HOME") != nullptr) {
        m_outputPath = dbasic::Path(getenv("HOME")).Append(".local/share/engine-sim");
    }
    else {
        m_outputPath = modulePath;
    }

    if (!m_outputPath.Exists()) {
        m_outputPath.CreateDir();
    }

    std::string enginePath = m_dataRoot.Append("dependencies/submodules/delta-studio/engines/basic").ToString();
    m_assetPath = m_dataRoot.Append("assets").ToString();

    dbasic::Path confPath = modulePath.Append("delta.conf");
    if (confPath.Exists()) {
        std::fstream confFile(confPath.ToString(), std::ios::in);
        std::getline(confFile, enginePath);
        std::getline(confFile, m_assetPath);
        enginePath = modulePath.Append(enginePath).ToString();
        m_assetPath = modulePath.Append(m_assetPath).ToString();
    }

    m_engine.GetConsole()->SetDefaultFontDirectory(enginePath + "/fonts/");

    const std::string shaderPath = enginePath + "/shaders/";
    const std::string winTitle = "Engine Sim | AngeTheGreat | v" + s_buildVersion;
    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = false;
    settings.Instance = instance;
    settings.ShaderDirectory = shaderPath.c_str();
    settings.WindowTitle = winTitle.c_str();
    settings.WindowPositionX = 0;
    settings.WindowPositionY = 0;
    settings.WindowStyle = ysWindow::WindowStyle::Windowed;
    settings.WindowWidth = 1920;
    settings.WindowHeight = 1080;

    m_engine.CreateGameWindow(settings);

    m_engine.GetDevice()->CreateSubRenderTarget(
        &m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        0,
        0,
        0,
        0);

    m_engine.InitializeShaderSet(&m_shaderSet);
    m_shaders.Initialize(
        &m_shaderSet,
        m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        m_engine.GetDefaultShaderProgram(),
        m_engine.GetDefaultInputLayout());
    m_engine.InitializeConsoleShaders(&m_shaderSet);
    m_engine.SetShaderSet(&m_shaderSet);

    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));

    m_assetManager.SetEngine(&m_engine);

    m_engine.GetDevice()->CreateIndexBuffer(
        &m_geometryIndexBuffer, sizeof(unsigned short) * 200000, nullptr);
    m_engine.GetDevice()->CreateVertexBuffer(
        &m_geometryVertexBuffer, sizeof(dbasic::Vertex) * 100000, nullptr);

    m_geometryGenerator.initialize(100000, 200000);

    initialize();
}

void EngineSimApplication::initialize() {
    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));
    m_assetManager.CompileInterchangeFile((m_assetPath + "/assets").c_str(), 1.0f, true);
    m_assetManager.LoadSceneFile((m_assetPath + "/assets").c_str(), true);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_engine.GetConsole()->GetFont());

    loadScript();

    m_audioBuffer.initialize(AudioSampleRate, AudioSampleRate);
    m_audioBuffer.m_writePointer = leadToSamples(AudioBufferLeadSeconds);

    ysAudioParameters params;
    params.m_bitsPerSample = 16;
    params.m_channelCount = 1;
    params.m_sampleRate = AudioSampleRate;
    m_outputAudioBuffer = m_engine.GetAudioDevice()->CreateBuffer(&params, AudioSampleRate);

    m_audioSource = m_engine.GetAudioDevice()->CreateSource(m_outputAudioBuffer);
    const bool hasLoadedEngine = m_simulator != nullptr && m_simulator->getEngine() != nullptr;
    m_audioSource->SetMode(hasLoadedEngine ? ysAudioSource::Mode::Loop : ysAudioSource::Mode::Stop);
    m_audioSource->SetPan(0.0f);
    m_audioSource->SetVolume(1.0f);

#ifdef ATG_ENGINE_SIM_DISCORD_ENABLED
    CDiscord::CreateInstance();
    GetDiscordManager()->SetUseDiscord(true);
    DiscordRichPresence passMe = { 0 };

    std::string engineName = m_iceEngine != nullptr
        ? m_iceEngine->getName()
        : "Broken Engine";

    GetDiscordManager()->SetStatus(passMe, engineName, s_buildVersion);
#endif
}

void EngineSimApplication::process(float frame_dt) {
    frame_dt = static_cast<float>(clamp(frame_dt, 1 / 200.0f, 1 / 30.0f));

    double speed = 1.0;
    if (m_engine.IsKeyDown(ysKey::Code::N1)) {
        speed = 1 / 10.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N2)) {
        speed = 1 / 100.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N3)) {
        speed = 1 / 200.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N4)) {
        speed = 1 / 500.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N5)) {
        speed = 1 / 1000.0;
    }

    if (m_engine.IsKeyDown(ysKey::Code::F1)) {
        m_displayAngle += frame_dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::F2)) {
        m_displayAngle -= frame_dt;
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::F3)) {
        m_displayAngle = 0.0f;
    }

    m_simulator->setSimulationSpeed(speed);

    const double avgFramerate = clamp(m_engine.GetAverageFramerate(), 30.0f, 1000.0f);
    m_simulator->startFrame(1 / avgFramerate);

    const auto procStart = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        m_oscCluster->sample();
    }
    const auto procEnd = std::chrono::steady_clock::now();

    m_simulator->endFrame();

    const auto duration = procEnd - procStart;
    if (iterationCount > 0) {
        m_performanceCluster->addTimePerTimestepSample((duration.count() / 1E9) / iterationCount);
    }

    const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    const SampleOffset writePosition = m_audioBuffer.m_writePointer;
    const SampleOffset targetWritePosition =
        m_audioBuffer.getBufferIndex(safeWritePosition, leadToSamples(AudioBufferLeadSeconds));

    SampleOffset maxWrite = m_audioBuffer.offsetDelta(writePosition, targetWritePosition);
    SampleOffset currentLead = m_audioBuffer.offsetDelta(safeWritePosition, writePosition);
    const SampleOffset newLead = m_audioBuffer.offsetDelta(safeWritePosition, targetWritePosition);

    if (currentLead > AudioSampleRate * 0.5) {
        m_audioBuffer.m_writePointer =
            m_audioBuffer.getBufferIndex(safeWritePosition, leadToSamples(AudioResetLeadSeconds));
        currentLead = m_audioBuffer.offsetDelta(safeWritePosition, m_audioBuffer.m_writePointer);
        maxWrite = m_audioBuffer.offsetDelta(m_audioBuffer.m_writePointer, targetWritePosition);
    }

    if (currentLead > newLead) {
        maxWrite = 0;
    }

    int16_t *samples = new int16_t[maxWrite];
    const int readSamples = m_simulator->readAudioOutput(maxWrite, samples);

    for (SampleOffset i = 0; i < static_cast<SampleOffset>(readSamples) && i < maxWrite; ++i) {
        const int16_t sample = samples[i];
        if (m_oscillatorSampleOffset % 4 == 0) {
            m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
                m_oscillatorSampleOffset,
                sample / static_cast<float>(INT16_MAX));
        }

        m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, static_cast<int>(i));
        m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (AudioSampleRate / 10);
    }

    delete[] samples;

    if (readSamples > 0) {
        SampleOffset size0;
        SampleOffset size1;
        void *data0;
        void *data1;
        m_audioSource->LockBufferSegment(
            m_audioBuffer.m_writePointer,
            readSamples,
            &data0,
            &size0,
            &data1,
            &size1);

        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data0),
            m_audioBuffer.m_writePointer,
            size0);
        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data1),
            m_audioBuffer.getBufferIndex(m_audioBuffer.m_writePointer, size0),
            size1);

        m_audioSource->UnlockBufferSegments(data0, size0, data1, size1);
        m_audioBuffer.commitBlock(readSamples);
    }

    m_performanceCluster->addInputBufferUsageSample(
        static_cast<double>(m_simulator->getSynthesizerInputLatency()) /
        m_simulator->getSynthesizerInputLatencyTarget());
    m_performanceCluster->addAudioLatencySample(
        m_audioBuffer.offsetDelta(m_audioSource->GetCurrentWritePosition(), m_audioBuffer.m_writePointer) /
        (AudioSampleRate * AudioBufferLeadSeconds));
}

void EngineSimApplication::render() {
    for (SimulationObject *object : m_objects) {
        object->generateGeometry();
    }

    for (int sublayer = 0; sublayer < 3; ++sublayer) {
        m_viewParameters.Sublayer = sublayer;
        for (SimulationObject *object : m_objects) {
            object->render(&getViewParameters());
        }
    }

    m_uiManager.render();
}

float EngineSimApplication::pixelsToUnits(float pixels) const {
    const float scale = m_displayHeight / m_engineView->m_bounds.height();
    return pixels * scale;
}

float EngineSimApplication::unitsToPixels(float units) const {
    const float scale = m_engineView->m_bounds.height() / m_displayHeight;
    return units * scale;
}

void EngineSimApplication::run() {
    if (m_simulator == nullptr) {
        return;
    }

    while (true) {
        m_engine.StartFrame();

        if (!m_engine.IsOpen() || m_engine.ProcessKeyDown(ysKey::Code::Escape)) {
            break;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Return)) {
            m_audioSource->SetMode(ysAudioSource::Mode::Stop);
            loadScript();
            if (m_simulator != nullptr && m_simulator->getEngine() != nullptr) {
                m_audioSource->SetMode(ysAudioSource::Mode::Loop);
            }
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Tab)) {
            m_screen = (m_screen + 1) % 3;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F)) {
            const bool fullscreen =
                m_engine.GetGameWindow()->GetWindowStyle() == ysWindow::WindowStyle::Fullscreen;
            m_engine.GetGameWindow()->SetWindowStyle(
                fullscreen ? ysWindow::WindowStyle::Windowed : ysWindow::WindowStyle::Fullscreen);
            m_infoCluster->setLogMessage(fullscreen ? "Exited fullscreen mode" : "Entered fullscreen mode");
        }

        m_gameWindowHeight = m_engine.GetGameWindow()->GetGameHeight();
        m_screenHeight = m_engine.GetGameWindow()->GetScreenHeight();
        m_screenWidth = m_engine.GetGameWindow()->GetScreenWidth();

        updateScreenSizeStability();
        processEngineInput();

        if (m_engine.ProcessKeyDown(ysKey::Code::Insert) && m_engine.GetGameWindow()->IsActive()) {
            if (!isRecording() && readyToRecord()) {
                startRecording();
            }
            else if (isRecording()) {
                stopRecording();
            }
        }

        if (isRecording() && !readyToRecord()) {
            stopRecording();
        }

        if (!m_paused || m_engine.ProcessKeyDown(ysKey::Code::Right)) {
            process(m_engine.GetFrameLength());
        }

        m_uiManager.update(m_engine.GetFrameLength());
        renderScene();
        m_engine.EndFrame();

        if (isRecording()) {
            recordFrame();
        }
    }

    if (isRecording()) {
        stopRecording();
    }

    if (m_simulator != nullptr) {
        m_simulator->endAudioRenderingThread();
    }
}

void EngineSimApplication::destroy() {
    m_shaderSet.Destroy();

    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryVertexBuffer);
    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryIndexBuffer);

    m_assetManager.Destroy();
    m_engine.Destroy();

    if (m_simulator != nullptr) {
        m_simulator->destroy();
        delete m_simulator;
        m_simulator = nullptr;
    }

    m_audioBuffer.destroy();
    m_geometryGenerator.destroy();
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}

void EngineSimApplication::drawGeneratedUi(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer,
    dbasic::StageEnableFlags flags)
{
    m_engine.DrawGeneric(
        flags,
        m_geometryIndexBuffer,
        m_geometryVertexBuffer,
        sizeof(dbasic::Vertex),
        indices.BaseIndex,
        indices.BaseVertex,
        indices.FaceCount,
        false,
        layer);
}

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;

    if (settings.startFullscreen) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }

    m_background = ysColor::srgbiToLinear(m_applicationSettings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(m_applicationSettings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(m_applicationSettings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(m_applicationSettings.colorPink);
    m_red = ysColor::srgbiToLinear(m_applicationSettings.colorRed);
    m_orange = ysColor::srgbiToLinear(m_applicationSettings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(m_applicationSettings.colorYellow);
    m_blue = ysColor::srgbiToLinear(m_applicationSettings.colorBlue);
    m_green = ysColor::srgbiToLinear(m_applicationSettings.colorGreen);
}

const SimulationObject::ViewParameters &EngineSimApplication::getViewParameters() const {
    return m_viewParameters;
}
