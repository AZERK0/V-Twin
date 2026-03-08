#include "app/engine_sim_application.h"

void EngineSimApplication::layoutScreen0(const Bounds &windowBounds) {
    Grid grid;
    grid.v_cells = 2;
    grid.h_cells = 3;

    Grid grid3x3;
    grid3x3.v_cells = 3;
    grid3x3.h_cells = 3;

    Grid grid1x3;
    grid1x3.v_cells = 3;
    grid1x3.h_cells = 1;

    m_engineView->setDrawFrame(true);
    m_engineView->setBounds(grid.get(windowBounds, 1, 0, 1, 1));
    m_engineView->setLocalPosition({ 0, 0 });

    m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 2);
    m_oscCluster->m_bounds = grid.get(windowBounds, 1, 1);
    m_performanceCluster->m_bounds = grid3x3.get(windowBounds, 0, 1);
    m_loadSimulationCluster->m_bounds = grid3x3.get(windowBounds, 0, 2);
    m_mixerCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 2);
    m_infoCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 0, 1, 2);

    m_engineView->setVisible(true);
    m_rightGaugeCluster->setVisible(true);
    m_oscCluster->setVisible(true);
    m_performanceCluster->setVisible(true);
    m_loadSimulationCluster->setVisible(true);
    m_mixerCluster->setVisible(true);
    m_infoCluster->setVisible(true);

    m_oscCluster->activate();
}

void EngineSimApplication::layoutScreen1(const Bounds &windowBounds) {
    m_engineView->setDrawFrame(false);
    m_engineView->setBounds(windowBounds);
    m_engineView->setLocalPosition({ 0, 0 });
    m_engineView->activate();

    m_engineView->setVisible(true);
    m_rightGaugeCluster->setVisible(false);
    m_oscCluster->setVisible(false);
    m_performanceCluster->setVisible(false);
    m_loadSimulationCluster->setVisible(false);
    m_mixerCluster->setVisible(false);
    m_infoCluster->setVisible(false);
}

void EngineSimApplication::layoutScreen2(const Bounds &windowBounds) {
    Grid grid;
    grid.v_cells = 1;
    grid.h_cells = 3;

    m_engineView->setDrawFrame(true);
    m_engineView->setBounds(grid.get(windowBounds, 0, 0, 2, 1));
    m_engineView->setLocalPosition({ 0, 0 });
    m_engineView->activate();

    m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 1);

    m_engineView->setVisible(true);
    m_rightGaugeCluster->setVisible(true);
    m_oscCluster->setVisible(false);
    m_performanceCluster->setVisible(false);
    m_loadSimulationCluster->setVisible(false);
    m_mixerCluster->setVisible(false);
    m_infoCluster->setVisible(false);
}

void EngineSimApplication::updateRenderTarget(int screenHeight) {
    const float cameraAspectRatio = m_engineView->m_bounds.width() / m_engineView->m_bounds.height();

    m_engine.GetDevice()->ResizeRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height(),
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height());
    m_engine.GetDevice()->RepositionRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.getPosition(Bounds::tl).x,
        screenHeight - m_engineView->m_bounds.getPosition(Bounds::tl).y);
    m_shaders.CalculateCamera(
        cameraAspectRatio * m_displayHeight / m_engineView->m_zoom,
        m_displayHeight / m_engineView->m_zoom,
        m_engineView->m_bounds,
        m_screenWidth,
        m_screenHeight,
        m_displayAngle);
}

void EngineSimApplication::renderScene() {
    getShaders()->ResetBaseColor();
    getShaders()->SetObjectTransform(ysMath::LoadIdentity());

    m_textRenderer.SetColor(ysColor::linearToSrgb(m_foreground));
    m_shaders.SetClearColor(ysColor::linearToSrgb(m_shadow));

    const int screenWidth = m_engine.GetGameWindow()->GetGameWidth();
    const int screenHeight = m_engine.GetGameWindow()->GetGameHeight();
    const Bounds windowBounds(static_cast<float>(screenWidth), static_cast<float>(screenHeight), { 0, static_cast<float>(screenHeight) });

    const Point cameraPos = m_engineView->getCameraPosition();
    m_shaders.m_cameraPosition = ysMath::LoadVector(cameraPos.x, cameraPos.y);
    m_shaders.CalculateUiCamera(screenWidth, screenHeight);

    if (m_screen == 0) {
        layoutScreen0(windowBounds);
    }
    else if (m_screen == 1) {
        layoutScreen1(windowBounds);
    }
    else {
        layoutScreen2(windowBounds);
    }

    updateRenderTarget(screenHeight);

    m_geometryGenerator.reset();
    render();

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryVertexBuffer,
        const_cast<char *>(reinterpret_cast<const char *>(m_geometryGenerator.getVertexData())),
        sizeof(dbasic::Vertex) * m_geometryGenerator.getCurrentVertexCount(),
        0);

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryIndexBuffer,
        const_cast<char *>(reinterpret_cast<const char *>(m_geometryGenerator.getIndexData())),
        sizeof(unsigned short) * m_geometryGenerator.getCurrentIndexCount(),
        0);
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();
    m_uiManager.initialize(this);

    m_engineView = m_uiManager.getRoot()->addElement<EngineView>();
    m_rightGaugeCluster = m_uiManager.getRoot()->addElement<RightGaugeCluster>();
    m_oscCluster = m_uiManager.getRoot()->addElement<OscilloscopeCluster>();
    m_performanceCluster = m_uiManager.getRoot()->addElement<PerformanceCluster>();
    m_loadSimulationCluster = m_uiManager.getRoot()->addElement<LoadSimulationCluster>();
    m_mixerCluster = m_uiManager.getRoot()->addElement<MixerCluster>();
    m_infoCluster = m_uiManager.getRoot()->addElement<InfoCluster>();

    m_infoCluster->setEngine(m_iceEngine);
    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);
    m_oscCluster->setSimulator(m_simulator);
    if (m_iceEngine != nullptr) {
        m_oscCluster->setDynoMaxRange(units::toRpm(m_iceEngine->getRedline()));
    }
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);
}

void EngineSimApplication::startRecording() {
    m_recording = true;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Encoder::VideoSettings settings{};
    settings.fname = "../workspace/video_capture/engine_sim_video_capture.mp4";
    settings.inputWidth = m_engine.GetScreenWidth();
    settings.inputHeight = m_engine.GetScreenHeight();
    settings.width = settings.inputWidth;
    settings.height = settings.inputHeight;
    settings.hardwareEncoding = true;
    settings.inputAlpha = true;
    settings.bitRate = 40000000;

    m_encoder.run(settings, 2);
#endif
}

void EngineSimApplication::updateScreenSizeStability() {
    m_screenResolution[m_screenResolutionIndex][0] = m_engine.GetScreenWidth();
    m_screenResolution[m_screenResolutionIndex][1] = m_engine.GetScreenHeight();
    m_screenResolutionIndex = (m_screenResolutionIndex + 1) % ScreenResolutionHistoryLength;
}

bool EngineSimApplication::readyToRecord() {
    const int width = m_screenResolution[0][0];
    const int height = m_screenResolution[0][1];

    if (width <= 0 && height <= 0) return false;
    if ((width % 2) != 0 || (height % 2) != 0) return false;

    for (int i = 1; i < ScreenResolutionHistoryLength; ++i) {
        if (m_screenResolution[i][0] != width) return false;
        if (m_screenResolution[i][1] != height) return false;
    }

    return true;
}

void EngineSimApplication::stopRecording() {
    m_recording = false;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    m_encoder.commit();
    m_encoder.stop();
#endif
}

void EngineSimApplication::recordFrame() {
#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Frame *frame = m_encoder.newFrame(false);
    if (frame != nullptr && m_encoder.getError() == atg_dtv::Encoder::Error::None) {
        m_engine.GetDevice()->ReadRenderTarget(m_engine.GetScreenRenderTarget(), frame->m_rgb);
    }

    m_encoder.submitFrame();
#endif
}
