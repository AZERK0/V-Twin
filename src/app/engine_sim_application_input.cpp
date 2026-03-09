#include "app/engine_sim_application.h"

#include "audio/synthesizer.h"
#include "domain/engine/engine.h"
#include "domain/vehicle/transmission.h"
#include "simulation/simulator.h"
#include "shared/utilities.h"
#include "ui/info_cluster.h"
#include "units.h"

namespace {
    double controlRate(bool fineControlMode, double normalRate, double fineRate) {
        return fineControlMode ? fineRate : normalRate;
    }
}

bool EngineSimApplication::processFineAudioControls(float dt, int mouseWheelDelta, bool fineControlMode) {
    struct AudioBinding {
        ysKey::Code key;
        const char *label;
        double normalRate;
        double fineRate;
        float Synthesizer::AudioParameters::*field;
    };

    static const AudioBinding bindings[] = {
        { ysKey::Code::Z, "[Z] - Set volume to ", 0.01, 0.001, &Synthesizer::AudioParameters::volume },
        { ysKey::Code::X, "[X] - Set convolution level to ", 0.01, 0.001, &Synthesizer::AudioParameters::convolution },
        { ysKey::Code::C, "[C] - Set high freq. gain to ", 0.001, 0.00001, &Synthesizer::AudioParameters::dF_F_mix },
        { ysKey::Code::V, "[V] - Set low freq. noise to ", 0.01, 0.001, &Synthesizer::AudioParameters::airNoise },
        { ysKey::Code::B, "[B] - Set high freq. noise to ", 0.01, 0.001, &Synthesizer::AudioParameters::inputSampleNoise },
    };

    for (const AudioBinding &binding : bindings) {
        if (!m_engine.IsKeyDown(binding.key)) {
            continue;
        }

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        const double rate = controlRate(fineControlMode, binding.normalRate, binding.fineRate);
        audioParams.*(binding.field) = static_cast<float>(
            clamp(audioParams.*(binding.field) + mouseWheelDelta * rate * dt));
        m_simulator->synthesizer().setAudioParameters(audioParams);
        m_infoCluster->setLogMessage(binding.label + std::to_string(audioParams.*(binding.field)));
        return true;
    }

    if (m_engine.IsKeyDown(ysKey::Code::N)) {
        const double simulationFrequency = clamp(
            m_simulator->getSimulationFrequency() +
                mouseWheelDelta * controlRate(fineControlMode, 100.0, 10.0) * dt,
            400.0,
            400000.0);

        m_simulator->setSimulationFrequency(simulationFrequency);
        m_infoCluster->setLogMessage(
            "[N] - Set simulation freq to " + std::to_string(m_simulator->getSimulationFrequency()));
        return true;
    }

    return false;
}

void EngineSimApplication::updateThrottleControl(int mouseWheelDelta, bool fineControlMode) {
    const double previousTargetSpeed = m_targetSpeedSetting;
    m_targetSpeedSetting = fineControlMode ? m_targetSpeedSetting : 0.0;

    if (m_engine.IsKeyDown(ysKey::Code::Q)) {
        m_targetSpeedSetting = 0.01;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::W)) {
        m_targetSpeedSetting = 0.1;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::E)) {
        m_targetSpeedSetting = 0.2;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::R)) {
        m_targetSpeedSetting = 1.0;
    }
    else if (fineControlMode) {
        m_targetSpeedSetting = clamp(m_targetSpeedSetting + mouseWheelDelta * 0.0001);
    }

    if (previousTargetSpeed != m_targetSpeedSetting) {
        m_infoCluster->setLogMessage("Speed control set to " + std::to_string(m_targetSpeedSetting));
    }

    m_speedSetting = m_targetSpeedSetting * 0.5 + 0.5 * m_speedSetting;
    m_iceEngine->setSpeedControl(m_speedSetting);
}

void EngineSimApplication::updateViewLayer() {
    if (m_engine.ProcessKeyDown(ysKey::Code::M)) {
        const int currentLayer = getViewParameters().Layer0;
        if (currentLayer + 1 < m_iceEngine->getMaxDepth()) {
            setViewLayer(currentLayer + 1);
        }

        m_infoCluster->setLogMessage("[M] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::OEM_Comma)) {
        if (getViewParameters().Layer0 - 1 >= 0) {
            setViewLayer(getViewParameters().Layer0 - 1);
        }

        m_infoCluster->setLogMessage("[,] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }
}

void EngineSimApplication::updateDynoControls(float dt, int mouseWheelDelta) {
    if (m_engine.IsKeyDown(ysKey::Code::G) && m_simulator->m_dyno.m_hold) {
        if (mouseWheelDelta > 0) {
            m_dynoSpeed += m_iceEngine->getDynoHoldStep();
        }
        else if (mouseWheelDelta < 0) {
            m_dynoSpeed -= m_iceEngine->getDynoHoldStep();
        }

        m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
        m_infoCluster->setLogMessage("[G] - Set dyno speed to " + std::to_string(units::toRpm(m_dynoSpeed)));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::D)) {
        m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;
        m_infoCluster->setLogMessage(
            m_simulator->m_dyno.m_enabled ? "DYNOMOMETER ENABLED" : "DYNOMOMETER DISABLED");
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::H)) {
        m_simulator->m_dyno.m_hold = !m_simulator->m_dyno.m_hold;
        m_infoCluster->setLogMessage(
            m_simulator->m_dyno.m_hold
                ? (m_simulator->m_dyno.m_enabled ? "HOLD ENABLED" : "HOLD ON STANDBY [ENABLE DYNO. FOR HOLD]")
                : "HOLD DISABLED");
    }

    if (m_simulator->m_dyno.m_enabled) {
        if (!m_simulator->m_dyno.m_hold) {
            if (m_simulator->getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)) {
                m_dynoSpeed += units::rpm(500) * dt;
            }
            else {
                m_dynoSpeed *= (1 / (1 + dt));
            }

            if (m_dynoSpeed > m_iceEngine->getRedline()) {
                m_simulator->m_dyno.m_enabled = false;
                m_dynoSpeed = units::rpm(0);
            }
        }
    }
    else if (!m_simulator->m_dyno.m_hold) {
        m_dynoSpeed = units::rpm(0);
    }

    m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
    m_simulator->m_dyno.m_rotationSpeed = m_dynoSpeed;
}

void EngineSimApplication::updateStarterState() {
    const bool starterWasEnabled = m_simulator->m_starterMotor.m_enabled;
    m_simulator->m_starterMotor.m_enabled = m_engine.IsKeyDown(ysKey::Code::S);

    if (starterWasEnabled != m_simulator->m_starterMotor.m_enabled) {
        m_infoCluster->setLogMessage(
            m_simulator->m_starterMotor.m_enabled ? "STARTER ENABLED" : "STARTER DISABLED");
    }
}

void EngineSimApplication::updateIgnitionState() {
    if (!m_engine.ProcessKeyDown(ysKey::Code::A)) {
        return;
    }

    auto *ignitionModule = m_simulator->getEngine()->getIgnitionModule();
    ignitionModule->m_enabled = !ignitionModule->m_enabled;
    m_infoCluster->setLogMessage(ignitionModule->m_enabled ? "IGNITION ENABLED" : "IGNITION DISABLED");
}

void EngineSimApplication::updateTransmissionState() {
    if (m_engine.ProcessKeyDown(ysKey::Code::Up)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() + 1);
        m_infoCluster->setLogMessage(
            "UPSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::Down)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() - 1);
        if (m_simulator->getTransmission()->getGear() != -1) {
            m_infoCluster->setLogMessage(
                "DOWNSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
        }
        else {
            m_infoCluster->setLogMessage("SHIFTED TO NEUTRAL");
        }
    }
}

void EngineSimApplication::updateClutchState(float dt) {
    if (m_engine.IsKeyDown(ysKey::Code::T)) {
        m_targetClutchPressure -= 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::U)) {
        m_targetClutchPressure += 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::Shift)) {
        m_targetClutchPressure = 0.0;
        m_infoCluster->setLogMessage("CLUTCH DEPRESSED");
    }
    else if (!m_engine.IsKeyDown(ysKey::Code::Y)) {
        m_targetClutchPressure = 1.0;
    }

    m_targetClutchPressure = clamp(m_targetClutchPressure);

    double clutchResponse = 0.001;
    if (m_engine.IsKeyDown(ysKey::Code::Space)) {
        clutchResponse = 1.0;
    }

    const double clutchMix = dt / (dt + clutchResponse);
    m_clutchPressure = m_clutchPressure * (1 - clutchMix) + m_targetClutchPressure * clutchMix;
    m_simulator->getTransmission()->setClutchPressure(m_clutchPressure);
}

void EngineSimApplication::processEngineInput() {
    if (m_iceEngine == nullptr) {
        return;
    }

    const float dt = m_engine.GetFrameLength();
    const bool fineControlMode = m_engine.IsKeyDown(ysKey::Code::Space);

    const int mouseWheel = m_engine.GetMouseWheel();
    const int mouseWheelDelta = mouseWheel - m_lastMouseWheel;
    m_lastMouseWheel = mouseWheel;

    processFineAudioControls(dt, mouseWheelDelta, fineControlMode);
    updateThrottleControl(mouseWheelDelta, fineControlMode);
    updateViewLayer();
    updateDynoControls(dt, mouseWheelDelta);
    updateStarterState();
    updateIgnitionState();
    updateTransmissionState();
    updateClutchState(dt);
}
