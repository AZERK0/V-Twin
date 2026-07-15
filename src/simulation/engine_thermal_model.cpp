#include "simulation/engine_thermal_model.h"

#include "constants.h"
#include "units.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace {
    constexpr double MinimumGasPressureBar = 0.05;
    constexpr double MinimumGasTemperatureK = 200.0;
    constexpr double MinimumChamberVolumeM3 = 1e-9;

    double boreArea(double boreM) {
        return constants::pi * boreM * boreM / 4.0;
    }

    bool isPositiveFinite(double value) {
        return std::isfinite(value) && value > 0.0;
    }
}

bool EngineThermalModel::initialize(
    const EngineThermalParameters &parameters,
    const std::vector<CylinderThermalProperties> &cylinderProperties)
{
    reset();
    m_parameters = parameters;
    m_cylinderProperties = cylinderProperties;
    if (!m_parameters.isValid() || !propertiesAreValid() || !updateIntervalIsStable()) {
        return false;
    }

    const CylinderThermalState initialCylinderState {
        m_parameters.ambientTemperatureK,
        m_parameters.ambientTemperatureK
    };
    m_cylinderStates.assign(m_cylinderProperties.size(), initialCylinderState);
    m_accumulatedEnergy.resize(m_cylinderProperties.size());
    m_oilTemperatureK = m_parameters.ambientTemperatureK;
    m_initialized = true;
    publishState();
    return true;
}

void EngineThermalModel::reset() {
    m_cylinderProperties.clear();
    m_cylinderStates.clear();
    m_accumulatedEnergy.clear();
    m_state = EngineThermalState{};
    m_oilTemperatureK = 0.0;
    m_accumulatedTimeSeconds = 0.0;
    m_inputAnomalyCount = 0;
    m_initialized = false;
}

void EngineThermalModel::update(const std::vector<CylinderThermalSample> &samples, double dt) {
    if (!m_initialized || samples.size() != m_cylinderStates.size() || !isPositiveFinite(dt)) {
        ++m_inputAnomalyCount;
        return;
    }

    for (int i = 0; i < static_cast<int>(samples.size()); ++i) {
        if (sampleIsFinite(samples[i])) {
            accumulateSample(i, samples[i], dt);
        }
        else {
            ++m_inputAnomalyCount;
        }
    }

    m_accumulatedTimeSeconds += dt;
    if (m_accumulatedTimeSeconds >= m_parameters.updateIntervalSeconds) {
        integrateAccumulatedEnergy();
    }
}

double EngineThermalModel::calculateHohenbergHeatTransferCoefficient(
    const EngineThermalParameters &parameters,
    const CylinderThermalSample &sample)
{
    const double volumeM3 = std::max(sample.chamberVolumeM3, MinimumChamberVolumeM3);
    const double pressureBar = std::max(sample.gasPressurePa / units::bar, MinimumGasPressureBar);
    const double temperatureK = std::max(sample.gasTemperatureK, MinimumGasTemperatureK);
    const double meanPistonSpeedMPerS = std::max(sample.meanPistonSpeedMPerS, 0.0);
    const double coefficient = parameters.hohenbergCoefficient
        * std::pow(volumeM3, -0.06)
        * std::pow(pressureBar, 0.8)
        * std::pow(temperatureK, -0.4)
        * std::pow(meanPistonSpeedMPerS + 1.4, 0.8);
    return std::clamp(coefficient, 0.0, parameters.maximumHeatTransferCoefficientWPerM2K);
}

bool EngineThermalModel::propertiesAreValid() const {
    if (m_cylinderProperties.empty()) {
        return false;
    }

    return std::all_of(
        m_cylinderProperties.begin(),
        m_cylinderProperties.end(),
        [](const CylinderThermalProperties &properties) {
            return isPositiveFinite(properties.pistonMassKg)
                && isPositiveFinite(properties.boreM)
                && isPositiveFinite(properties.combustionChamberVolumeM3);
        });
}

bool EngineThermalModel::updateIntervalIsStable() const {
    const double pistonConductance = m_parameters.pistonCylinderConductanceWPerK
        + m_parameters.pistonOilConductanceWPerK;
    const double cylinderConductance = m_parameters.pistonCylinderConductanceWPerK
        + m_parameters.cylinderOilConductanceWPerK
        + m_parameters.cylinderAmbientConductanceWPerK;
    const double oilConductance = m_cylinderProperties.size()
        * (m_parameters.pistonOilConductanceWPerK + m_parameters.cylinderOilConductanceWPerK)
        + m_parameters.oilAmbientConductanceWPerK;
    const double minimumPistonCapacity = std::min_element(
        m_cylinderProperties.begin(),
        m_cylinderProperties.end(),
        [](const auto &left, const auto &right) { return left.pistonMassKg < right.pistonMassKg; })
        ->pistonMassKg * m_parameters.pistonSpecificHeatJPerKgK;
    return (pistonConductance == 0.0 || m_parameters.updateIntervalSeconds < 2.0 * minimumPistonCapacity / pistonConductance)
        && (cylinderConductance == 0.0 || m_parameters.updateIntervalSeconds < 2.0 * m_parameters.cylinderThermalCapacityJPerK / cylinderConductance)
        && (oilConductance == 0.0 || m_parameters.updateIntervalSeconds < 2.0 * m_parameters.oilThermalCapacityJPerK() / oilConductance);
}

bool EngineThermalModel::sampleIsFinite(const CylinderThermalSample &sample) const {
    return std::isfinite(sample.gasTemperatureK)
        && std::isfinite(sample.gasPressurePa)
        && std::isfinite(sample.chamberVolumeM3)
        && std::isfinite(sample.meanPistonSpeedMPerS)
        && std::isfinite(sample.frictionPowerW)
        && sample.frictionPowerW >= 0.0;
}

void EngineThermalModel::accumulateSample(
    int cylinderIndex,
    const CylinderThermalSample &sample,
    double dt)
{
    if (sample.gasTemperatureK < MinimumGasTemperatureK
        || sample.gasPressurePa / units::bar < MinimumGasPressureBar
        || sample.chamberVolumeM3 < MinimumChamberVolumeM3
        || sample.meanPistonSpeedMPerS < 0.0)
    {
        ++m_inputAnomalyCount;
    }

    AccumulatedEnergy &energy = m_accumulatedEnergy[cylinderIndex];
    energy.pistonGasJ += calculatePistonGasPower(cylinderIndex, sample) * dt;
    energy.cylinderGasJ += calculateCylinderGasPower(cylinderIndex, sample) * dt;
    energy.frictionJ += sample.frictionPowerW * dt;
}

void EngineThermalModel::integrateAccumulatedEnergy() {
    std::vector<double> pistonPowersW(m_cylinderStates.size());
    std::vector<double> cylinderPowersW(m_cylinderStates.size());
    for (int i = 0; i < static_cast<int>(m_cylinderStates.size()); ++i) {
        pistonPowersW[i] = calculatePistonPower(i, m_accumulatedTimeSeconds);
        cylinderPowersW[i] = calculateCylinderPower(i, m_accumulatedTimeSeconds);
    }

    const double oilPowerW = calculateOilPower(m_accumulatedTimeSeconds);
    const bool temperaturesUpdated = applyTemperatureChanges(
        pistonPowersW,
        cylinderPowersW,
        oilPowerW,
        m_accumulatedTimeSeconds);
    clearAccumulator();
    if (temperaturesUpdated) {
        publishState();
    }
    else {
        ++m_inputAnomalyCount;
    }
}

double EngineThermalModel::calculatePistonPower(int cylinderIndex, double elapsedSeconds) const {
    const CylinderThermalState &state = m_cylinderStates[cylinderIndex];
    const AccumulatedEnergy &energy = m_accumulatedEnergy[cylinderIndex];
    return energy.pistonGasJ / elapsedSeconds
        + m_parameters.pistonFrictionHeatFraction * energy.frictionJ / elapsedSeconds
        - m_parameters.pistonCylinderConductanceWPerK
            * (state.pistonTemperatureK - state.cylinderTemperatureK)
        - m_parameters.pistonOilConductanceWPerK
            * (state.pistonTemperatureK - m_oilTemperatureK);
}

double EngineThermalModel::calculateCylinderPower(int cylinderIndex, double elapsedSeconds) const {
    const CylinderThermalState &state = m_cylinderStates[cylinderIndex];
    const AccumulatedEnergy &energy = m_accumulatedEnergy[cylinderIndex];
    return energy.cylinderGasJ / elapsedSeconds
        + m_parameters.cylinderFrictionHeatFraction * energy.frictionJ / elapsedSeconds
        + m_parameters.pistonCylinderConductanceWPerK
            * (state.pistonTemperatureK - state.cylinderTemperatureK)
        - m_parameters.cylinderOilConductanceWPerK
            * (state.cylinderTemperatureK - m_oilTemperatureK)
        - m_parameters.cylinderAmbientConductanceWPerK
            * (state.cylinderTemperatureK - m_parameters.ambientTemperatureK);
}

double EngineThermalModel::calculateOilPower(double elapsedSeconds) const {
    double oilPowerW = -m_parameters.oilAmbientConductanceWPerK
        * (m_oilTemperatureK - m_parameters.ambientTemperatureK);
    for (int i = 0; i < static_cast<int>(m_cylinderStates.size()); ++i) {
        const CylinderThermalState &state = m_cylinderStates[i];
        oilPowerW += m_parameters.pistonOilConductanceWPerK
            * (state.pistonTemperatureK - m_oilTemperatureK);
        oilPowerW += m_parameters.cylinderOilConductanceWPerK
            * (state.cylinderTemperatureK - m_oilTemperatureK);
        oilPowerW += m_parameters.oilFrictionHeatFraction
            * m_accumulatedEnergy[i].frictionJ / elapsedSeconds;
    }
    return oilPowerW;
}

bool EngineThermalModel::applyTemperatureChanges(
    const std::vector<double> &pistonPowersW,
    const std::vector<double> &cylinderPowersW,
    double oilPowerW,
    double dt)
{
    std::vector<CylinderThermalState> nextCylinderStates = m_cylinderStates;
    for (int i = 0; i < static_cast<int>(m_cylinderStates.size()); ++i) {
        const double pistonCapacity = m_cylinderProperties[i].pistonMassKg
            * m_parameters.pistonSpecificHeatJPerKgK;
        nextCylinderStates[i].pistonTemperatureK += dt * pistonPowersW[i] / pistonCapacity;
        nextCylinderStates[i].cylinderTemperatureK += dt * cylinderPowersW[i]
            / m_parameters.cylinderThermalCapacityJPerK;
    }
    const double nextOilTemperatureK = m_oilTemperatureK
        + dt * oilPowerW / m_parameters.oilThermalCapacityJPerK();
    if (!temperaturesAreValid(nextCylinderStates, nextOilTemperatureK)) {
        return false;
    }
    m_cylinderStates = std::move(nextCylinderStates);
    m_oilTemperatureK = nextOilTemperatureK;
    return true;
}

bool EngineThermalModel::temperaturesAreValid(
    const std::vector<CylinderThermalState> &cylinderStates,
    double oilTemperatureK) const
{
    if (!isPositiveFinite(oilTemperatureK)) {
        return false;
    }
    return std::all_of(
        cylinderStates.begin(),
        cylinderStates.end(),
        [](const CylinderThermalState &state) {
            return isPositiveFinite(state.pistonTemperatureK)
                && isPositiveFinite(state.cylinderTemperatureK);
        });
}

void EngineThermalModel::publishState() {
    m_state.revision += 1;
    m_state.inputAnomalyCount = m_inputAnomalyCount;
    m_state.valid = true;
    m_state.oilTemperatureK = m_oilTemperatureK;
    m_state.cylinders = m_cylinderStates;
    const double count = static_cast<double>(m_cylinderStates.size());
    m_state.averagePistonTemperatureK = std::accumulate(
        m_cylinderStates.begin(), m_cylinderStates.end(), 0.0,
        [](double sum, const auto &state) { return sum + state.pistonTemperatureK; }) / count;
    m_state.averageCylinderTemperatureK = std::accumulate(
        m_cylinderStates.begin(), m_cylinderStates.end(), 0.0,
        [](double sum, const auto &state) { return sum + state.cylinderTemperatureK; }) / count;
    m_state.maximumPistonTemperatureK = std::max_element(
        m_cylinderStates.begin(), m_cylinderStates.end(),
        [](const auto &left, const auto &right) {
            return left.pistonTemperatureK < right.pistonTemperatureK;
        })->pistonTemperatureK;
    m_state.maximumCylinderTemperatureK = std::max_element(
        m_cylinderStates.begin(), m_cylinderStates.end(),
        [](const auto &left, const auto &right) {
            return left.cylinderTemperatureK < right.cylinderTemperatureK;
        })->cylinderTemperatureK;
}

void EngineThermalModel::clearAccumulator() {
    std::fill(m_accumulatedEnergy.begin(), m_accumulatedEnergy.end(), AccumulatedEnergy{});
    m_accumulatedTimeSeconds = 0.0;
}

double EngineThermalModel::calculatePistonGasPower(
    int cylinderIndex,
    const CylinderThermalSample &sample) const
{
    const double heatTransferCoefficient = calculateHohenbergHeatTransferCoefficient(m_parameters, sample);
    const double pistonArea = m_parameters.pistonGasSurfaceFactor
        * boreArea(m_cylinderProperties[cylinderIndex].boreM);
    const double gasTemperatureK = std::max(sample.gasTemperatureK, MinimumGasTemperatureK);
    return m_parameters.pistonGasHeatTransferFactor
        * heatTransferCoefficient
        * pistonArea
        * (gasTemperatureK - m_cylinderStates[cylinderIndex].pistonTemperatureK);
}

double EngineThermalModel::calculateCylinderGasPower(
    int cylinderIndex,
    const CylinderThermalSample &sample) const
{
    const CylinderThermalProperties &properties = m_cylinderProperties[cylinderIndex];
    const double exposedHeight = std::max(
        0.0,
        (sample.chamberVolumeM3 - properties.combustionChamberVolumeM3)
            / boreArea(properties.boreM));
    const double cylinderArea = m_parameters.cylinderGasSurfaceFactor
        * constants::pi * properties.boreM * exposedHeight;
    const double gasTemperatureK = std::max(sample.gasTemperatureK, MinimumGasTemperatureK);
    return m_parameters.cylinderGasHeatTransferFactor
        * calculateHohenbergHeatTransferCoefficient(m_parameters, sample)
        * cylinderArea
        * (gasTemperatureK - m_cylinderStates[cylinderIndex].cylinderTemperatureK);
}
