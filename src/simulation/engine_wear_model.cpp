#include "simulation/engine_wear_model.h"

#include "simulation/simulator.h"

#include "domain/engine/combustion_chamber.h"
#include "domain/engine/engine.h"
#include "units.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr double ModelUpdateInterval = 0.02;
    constexpr double SnapshotInterval = 0.1;
    constexpr double StoichAfr = 14.7;

    double clampUnit(double value) {
        return std::clamp(value, 0.0, 1.0);
    }

    double smoothValue(double current, double target, double dt, double response) {
        const double alpha = 1.0 - std::exp(-dt * response);
        return current + (target - current) * alpha;
    }
}

EngineWearModel::EngineWearModel() {
    reset();
}

EngineWearModel::~EngineWearModel() {
    /* void */
}

void EngineWearModel::reset() {
    m_state = EngineWearState{};

    m_modelTimer = 0.0;
    m_snapshotTimer = SnapshotInterval;
    m_elapsedHours = 0.0;
    m_lastThrottle = 0.0;

    m_thermalMargin = 0.78;
    m_lubricationMargin = 0.82;
    m_detonationMargin = 0.88;
    m_mechanicalFatigueLoad = 0.22;
    m_combustionStability = 0.91;

    m_oilTemperatureC = 94.0;
    m_coolantTemperatureC = 88.0;

    m_bottomEndDamage = 0.08;
    m_ringSealDamage = 0.05;
    m_valvetrainDamage = 0.04;
    m_headGasketDamage = 0.03;
    m_lubricationSystemDamage = 0.05;

    m_bottomEndRate = 0.0;
    m_ringSealRate = 0.0;
    m_valvetrainRate = 0.0;
    m_headGasketRate = 0.0;
    m_lubricationSystemRate = 0.0;

    m_overRevSeconds = 0.0;
    m_overTempSeconds = 0.0;
    m_coldHighLoadSeconds = 0.0;
    m_oilStarvationSeconds = 0.0;

    m_severeKnockEvents = 0;
    m_thermalCycles = 0;

    m_knockEventActive = false;
    m_thermalCycleArmed = true;
}

void EngineWearModel::update(const Simulator &simulator, double dt) {
    Engine *engine = simulator.getEngine();
    if (engine == nullptr || engine->getCylinderCount() == 0) {
        reset();
        return;
    }

    m_modelTimer += dt;
    while (m_modelTimer >= ModelUpdateInterval) {
        stepModel(simulator, ModelUpdateInterval);
        m_modelTimer -= ModelUpdateInterval;
    }
}

void EngineWearModel::stepModel(const Simulator &simulator, double dt) {
    Engine *engine = simulator.getEngine();
    if (engine == nullptr || engine->getCylinderCount() == 0) {
        return;
    }

    const double dtHours = dt / units::hour;
    m_elapsedHours += dtHours;

    const double redlineRpm = std::max(units::toRpm(engine->getRedline()), 1000.0);
    const double rpm = engine->getRpm();
    const double rpmLoad = clampUnit(rpm / redlineRpm);
    const double throttle = clampUnit(engine->getThrottle());
    const double throttleRate = clampUnit(std::abs(throttle - m_lastThrottle) / std::max(dt * 4.0, 1e-6));
    m_lastThrottle = throttle;

    const double manifoldLoad = clampUnit(engine->getManifoldPressure() / units::pressure(1.15, units::atm));
    const double intakeAfr = engine->getIntakeAfr();
    const double afrError = (intakeAfr > 0.0)
        ? clampUnit(std::abs(intakeAfr - StoichAfr) / 4.5)
        : 0.0;
    const double leanPenalty = (intakeAfr > StoichAfr)
        ? clampUnit((intakeAfr - StoichAfr) / 3.0)
        : 0.0;

    double averagePressureBar = 0.0;
    double maximumPressureBar = 0.0;
    double minimumPressureBar = 1e9;
    double averageTemperatureC = 0.0;
    double averageFrictionForce = 0.0;

    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        CombustionChamber *chamber = engine->getChamber(i);
        const double pressureBar = units::convert(chamber->m_system.pressure(), units::bar);
        const double temperatureC = chamber->m_system.temperature() - units::celcius(0.0);

        averagePressureBar += pressureBar;
        averageTemperatureC += temperatureC;
        averageFrictionForce += chamber->getFrictionForce();
        maximumPressureBar = std::max(maximumPressureBar, pressureBar);
        minimumPressureBar = std::min(minimumPressureBar, pressureBar);
    }

    const double cylinderCount = static_cast<double>(engine->getCylinderCount());
    averagePressureBar /= cylinderCount;
    averageTemperatureC /= cylinderCount;
    averageFrictionForce /= cylinderCount;

    const double pressureLoad = clampUnit(maximumPressureBar / 85.0);
    const double pressureSpread = clampUnit((maximumPressureBar - minimumPressureBar) / 25.0);
    const double chamberTemperatureLoad = clampUnit((averageTemperatureC - 180.0) / 1500.0);
    const double frictionLoad = clampUnit(averageFrictionForce / units::force(700.0, units::N));
    const double dynoBias = simulator.m_dyno.m_enabled ? 0.08 : 0.0;

    const double fatigueTarget = clampUnit(
        0.52 * std::pow(rpmLoad, 1.7)
        + 0.28 * pressureLoad
        + 0.12 * manifoldLoad
        + 0.08 * frictionLoad);
    m_mechanicalFatigueLoad = smoothValue(m_mechanicalFatigueLoad, fatigueTarget, dt, 2.2);

    const double thermalSeverity = clampUnit(
        0.30 * chamberTemperatureLoad
        + 0.24 * pressureLoad
        + 0.18 * rpmLoad
        + 0.18 * manifoldLoad
        + 0.10 * dynoBias);
    m_coolantTemperatureC = smoothValue(
        m_coolantTemperatureC,
        82.0 + 24.0 * thermalSeverity + 6.0 * m_mechanicalFatigueLoad,
        dt,
        1.5);
    m_oilTemperatureC = smoothValue(
        m_oilTemperatureC,
        88.0 + 18.0 * thermalSeverity + 22.0 * m_mechanicalFatigueLoad,
        dt,
        1.3);

    const double thermalTemperaturePenalty = clampUnit((m_coolantTemperatureC - 96.0) / 18.0);
    const double thermalMarginTarget = 1.0 - clampUnit(
        0.42 * thermalSeverity
        + 0.33 * thermalTemperaturePenalty
        + 0.15 * pressureLoad
        + 0.10 * dynoBias);
    m_thermalMargin = smoothValue(m_thermalMargin, thermalMarginTarget, dt, 2.0);

    const double oilWindowPenalty = clampUnit(std::abs(m_oilTemperatureC - 102.0) / 40.0);
    const double coldLoadPenalty = (m_oilTemperatureC < 70.0)
        ? clampUnit(((70.0 - m_oilTemperatureC) / 20.0) * (0.45 * throttle + 0.55 * rpmLoad))
        : 0.0;
    const double lubricationMarginTarget = 1.0 - clampUnit(
        0.34 * oilWindowPenalty
        + 0.24 * m_mechanicalFatigueLoad
        + 0.20 * pressureLoad
        + 0.12 * manifoldLoad
        + 0.10 * coldLoadPenalty);
    m_lubricationMargin = smoothValue(m_lubricationMargin, lubricationMarginTarget, dt, 2.1);

    const double detonationMarginTarget = 1.0 - clampUnit(
        0.34 * pressureLoad
        + 0.22 * chamberTemperatureLoad
        + 0.18 * leanPenalty
        + 0.14 * manifoldLoad
        + 0.12 * throttleRate);
    m_detonationMargin = smoothValue(m_detonationMargin, detonationMarginTarget, dt, 2.6);

    const double combustionStabilityTarget = 1.0 - clampUnit(
        0.46 * afrError
        + 0.28 * pressureSpread
        + 0.16 * throttleRate
        + 0.10 * leanPenalty);
    m_combustionStability = smoothValue(m_combustionStability, combustionStabilityTarget, dt, 2.0);

    if (rpm >= redlineRpm * 0.98) {
        m_overRevSeconds += dt;
    }
    if (m_coolantTemperatureC >= 108.0 || m_oilTemperatureC >= 128.0) {
        m_overTempSeconds += dt;
    }
    if (m_oilTemperatureC < 70.0 && throttle > 0.65 && rpmLoad > 0.45) {
        m_coldHighLoadSeconds += dt;
    }
    if (m_lubricationMargin < 0.12) {
        m_oilStarvationSeconds += dt;
    }

    const bool severeKnock = m_detonationMargin < 0.18;
    if (severeKnock && !m_knockEventActive) {
        ++m_severeKnockEvents;
    }
    m_knockEventActive = severeKnock;

    if (m_thermalCycleArmed && m_coolantTemperatureC >= 95.0) {
        ++m_thermalCycles;
        m_thermalCycleArmed = false;
    }
    else if (!m_thermalCycleArmed && m_coolantTemperatureC <= 70.0) {
        m_thermalCycleArmed = true;
    }

    const double thermalDamageRate = std::pow(1.0 - m_thermalMargin, 2.2) * 0.065;
    const double lubricationDamageRate = std::pow(1.0 - m_lubricationMargin, 2.1) * 0.060;
    const double detonationDamageRate = std::pow(1.0 - m_detonationMargin, 2.4) * 0.085;
    const double fatigueDamageRate = std::pow(m_mechanicalFatigueLoad, 2.0) * 0.055;
    const double instabilityDamageRate = std::pow(1.0 - m_combustionStability, 2.0) * 0.045;

    m_bottomEndRate = 0.62 * fatigueDamageRate + 0.26 * lubricationDamageRate + 0.12 * detonationDamageRate;
    m_ringSealRate = 0.40 * thermalDamageRate + 0.25 * detonationDamageRate + 0.20 * instabilityDamageRate + 0.15 * pressureLoad * 0.02;
    m_valvetrainRate = 0.46 * fatigueDamageRate + 0.36 * thermalDamageRate + 0.18 * instabilityDamageRate;
    m_headGasketRate = 0.58 * thermalDamageRate + 0.26 * detonationDamageRate + 0.16 * instabilityDamageRate;
    m_lubricationSystemRate = 0.60 * lubricationDamageRate + 0.24 * thermalDamageRate + 0.16 * fatigueDamageRate;

    m_bottomEndDamage = clampUnit(m_bottomEndDamage + dtHours * m_bottomEndRate);
    m_ringSealDamage = clampUnit(m_ringSealDamage + dtHours * m_ringSealRate);
    m_valvetrainDamage = clampUnit(m_valvetrainDamage + dtHours * m_valvetrainRate);
    m_headGasketDamage = clampUnit(m_headGasketDamage + dtHours * m_headGasketRate);
    m_lubricationSystemDamage = clampUnit(m_lubricationSystemDamage + dtHours * m_lubricationSystemRate);

    m_snapshotTimer += dt;
    if (!m_state.valid || m_snapshotTimer >= SnapshotInterval) {
        publishSnapshot();
        m_snapshotTimer = 0.0;
    }
}

void EngineWearModel::publishSnapshot() {
    const double effectiveDamage =
        0.28 * m_bottomEndDamage
        + 0.22 * m_ringSealDamage
        + 0.18 * m_valvetrainDamage
        + 0.17 * m_headGasketDamage
        + 0.15 * m_lubricationSystemDamage;
    const double effectiveRate =
        0.28 * m_bottomEndRate
        + 0.22 * m_ringSealRate
        + 0.18 * m_valvetrainRate
        + 0.17 * m_headGasketRate
        + 0.15 * m_lubricationSystemRate;
    const double health = clampUnit(1.0 - effectiveDamage);

    struct FailureScore {
        EngineWearState::FailureMode mode;
        double score;
    };

    const FailureScore scores[] = {
        { EngineWearState::FailureMode::ThermalOverload, (1.0 - m_thermalMargin) * 0.65 + m_headGasketDamage * 0.35 },
        { EngineWearState::FailureMode::LubricationBreakdown, (1.0 - m_lubricationMargin) * 0.70 + m_lubricationSystemDamage * 0.30 },
        { EngineWearState::FailureMode::DetonationDamage, (1.0 - m_detonationMargin) * 0.72 + m_ringSealDamage * 0.18 + m_headGasketDamage * 0.10 },
        { EngineWearState::FailureMode::BottomEndFatigue, m_bottomEndRate * 8.0 + m_bottomEndDamage * 0.40 },
        { EngineWearState::FailureMode::RingSealWear, m_ringSealRate * 8.0 + m_ringSealDamage * 0.40 },
        { EngineWearState::FailureMode::ValvetrainFatigue, m_valvetrainRate * 8.0 + m_valvetrainDamage * 0.40 },
        { EngineWearState::FailureMode::HeadGasketRisk, m_headGasketRate * 8.0 + m_headGasketDamage * 0.40 }
    };

    EngineWearState::FailureMode dominantMode = EngineWearState::FailureMode::Balanced;
    double dominantScore = 0.0;
    for (const FailureScore &failure : scores) {
        if (failure.score > dominantScore) {
            dominantScore = failure.score;
            dominantMode = failure.mode;
        }
    }

    if (dominantScore < 0.18) {
        dominantMode = EngineWearState::FailureMode::Balanced;
    }

    m_state.revision += 1;
    m_state.valid = true;
    m_state.globalHealth = static_cast<float>(health);
    m_state.damageRatePerHour = static_cast<float>(effectiveRate * 100.0);
    m_state.remainingUsefulLifeHours = static_cast<float>((effectiveRate > 1e-5) ? health / effectiveRate : 5000.0);
    m_state.confidence = 1.0f;
    m_state.oilTemperatureC = static_cast<float>(m_oilTemperatureC);
    m_state.coolantTemperatureC = static_cast<float>(m_coolantTemperatureC);
    m_state.thermalMargin = static_cast<float>(clampUnit(m_thermalMargin));
    m_state.lubricationMargin = static_cast<float>(clampUnit(m_lubricationMargin));
    m_state.detonationMargin = static_cast<float>(clampUnit(m_detonationMargin));
    m_state.mechanicalFatigueLoad = static_cast<float>(clampUnit(m_mechanicalFatigueLoad));
    m_state.combustionStability = static_cast<float>(clampUnit(m_combustionStability));
    m_state.bottomEndDamage = static_cast<float>(clampUnit(m_bottomEndDamage));
    m_state.ringSealDamage = static_cast<float>(clampUnit(m_ringSealDamage));
    m_state.valvetrainDamage = static_cast<float>(clampUnit(m_valvetrainDamage));
    m_state.headGasketDamage = static_cast<float>(clampUnit(m_headGasketDamage));
    m_state.lubricationSystemDamage = static_cast<float>(clampUnit(m_lubricationSystemDamage));
    m_state.overRevSeconds = static_cast<float>(m_overRevSeconds);
    m_state.overTempSeconds = static_cast<float>(m_overTempSeconds);
    m_state.coldHighLoadSeconds = static_cast<float>(m_coldHighLoadSeconds);
    m_state.oilStarvationSeconds = static_cast<float>(m_oilStarvationSeconds);
    m_state.severeKnockEvents = m_severeKnockEvents;
    m_state.thermalCycles = m_thermalCycles;
    m_state.dominantFailureMode = dominantMode;
}
