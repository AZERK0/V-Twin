#include "simulation/engine_condition_model.h"

#include "constants.h"
#include "domain/engine/engine.h"
#include "domain/vehicle/transmission.h"
#include "domain/vehicle/vehicle.h"
#include "simulation/simulator.h"
#include "units.h"

#include <cmath>

namespace {
    constexpr double ReferenceAmbientPressurePa = units::pressure(1.0, units::atm);
    constexpr double ReferenceAmbientTemperatureK = units::celcius(25.0);
}

void EngineConditionModel::reset() {
    m_state = EngineConditionState{};
}

void EngineConditionModel::update(const Simulator &simulator) {
    Engine *engine = simulator.getEngine();
    if (engine == nullptr) {
        reset();
        return;
    }

    m_state.revision += 1;
    m_state.valid = true;
    m_state.ignitionEnabled = engine->getIgnitionModule()->m_enabled;
    m_state.starterEnabled = simulator.m_starterMotor.m_enabled;
    m_state.dynamometerEnabled = simulator.m_dyno.m_enabled;
    m_state.dynamometerHoldEnabled = simulator.m_dyno.m_hold;
    m_state.engineSpeedRadPerSecond = engine->getSpeed();
    m_state.torqueNewtonMeters = simulator.getFilteredDynoTorque();
    m_state.powerWatts = simulator.getDynoPower();
    m_state.manifoldPressurePascals = engine->getManifoldPressure();
    m_state.intakeFlowRate = engine->getIntakeFlowRate();
    m_state.volumetricEfficiency = calculateVolumetricEfficiency(
        m_state.intakeFlowRate,
        engine->getDisplacement(),
        engine->getRpm());
    m_state.throttlePosition = engine->getThrottle();
    m_state.intakeAfr = engine->getIntakeAfr();
    m_state.exhaustOxygenFraction = engine->getExhaustO2();
    m_state.thermal = simulator.getEngineThermalState();

    const Vehicle *vehicle = simulator.getVehicle();
    m_state.vehicleSpeedMetersPerSecond = vehicle != nullptr ? vehicle->getSpeed() : 0.0;
    const Transmission *transmission = simulator.getTransmission();
    m_state.transmissionGear = transmission != nullptr ? transmission->getGear() : -1;
}

double EngineConditionModel::calculateVolumetricEfficiency(
    double intakeFlowRate,
    double engineDisplacementM3,
    double engineSpeedRpm)
{
    const double theoreticalAirPerRevolution = 0.5
        * ReferenceAmbientPressurePa
        * engineDisplacementM3
        / (constants::R * ReferenceAmbientTemperatureK);
    const double theoreticalAirPerSecond = theoreticalAirPerRevolution
        * engineSpeedRpm
        / 60.0;
    if (std::abs(theoreticalAirPerSecond) < 1e-3) {
        return 0.0;
    }

    return intakeFlowRate / theoreticalAirPerSecond;
}
