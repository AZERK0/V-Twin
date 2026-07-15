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
    constexpr double MaximumRelativeEnergyResidual = 1e-9;

    double boreArea(double boreM) {
        return constants::pi * boreM * boreM / 4.0;
    }

    bool isPositiveFinite(double value) {
        return std::isfinite(value) && value > 0.0;
    }

    bool nodeIsStable(double interval, double capacity, double conductance) {
        return conductance == 0.0 || interval < 2.0 * capacity / conductance;
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
    const CylinderThermalState initialState {
        m_parameters.ambientTemperatureK,
        m_parameters.ambientTemperatureK
    };
    m_cylinderStates.assign(m_cylinderProperties.size(), initialState);
    m_accumulatedEnergy.resize(m_cylinderProperties.size());
    m_oilTemperatureK = m_parameters.ambientTemperatureK;
    m_sumpTemperatureK = m_parameters.ambientTemperatureK;
    m_coolantTemperatureK = m_parameters.ambientTemperatureK;
    m_initialized = true;
    publishState();
    return true;
}

void EngineThermalModel::reset() {
    m_cylinderProperties.clear();
    m_cylinderStates.clear();
    m_accumulatedEnergy.clear();
    m_state = EngineThermalState{};
    m_lastCoolingState = EngineCoolingState{};
    m_lastPowerBalance = EngineThermalPowerBalance{};
    m_oilTemperatureK = 0.0;
    m_sumpTemperatureK = 0.0;
    m_coolantTemperatureK = 0.0;
    m_accumulatedTimeSeconds = 0.0;
    m_accumulatedVehicleSpeedMeters = 0.0;
    m_accumulatedEngineAngleRadians = 0.0;
    m_inputAnomalyCount = 0;
    m_initialized = false;
}

void EngineThermalModel::update(
    const std::vector<CylinderThermalSample> &samples,
    double dt)
{
    update(samples, EngineThermalOperatingPoint{}, dt);
}

void EngineThermalModel::update(
    const std::vector<CylinderThermalSample> &samples,
    const EngineThermalOperatingPoint &operatingPoint,
    double dt)
{
    if (!m_initialized || samples.size() != m_cylinderStates.size()
        || !isPositiveFinite(dt) || !operatingPointIsFinite(operatingPoint)) {
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
    accumulateOperatingPoint(operatingPoint, dt);
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
    const EngineCoolingState cooling = calculateMaximumCoolingState();
    return baseNodesAreStable(cooling) && extendedNodesAreStable(cooling);
}

EngineCoolingState EngineThermalModel::calculateMaximumCoolingState() const {
    const auto &coolingParameters = m_parameters.cooling;
    const EngineThermalOperatingPoint maximumOperatingPoint {
        coolingParameters.maximumAirSpeedMPerS,
        std::max(
            coolingParameters.coolantPumpReferenceSpeedRadPerSecond,
            coolingParameters.oilPumpReferenceSpeedRadPerSecond) * 10.0
    };
    const double maximumTemperatureK = std::max(
        coolingParameters.fanFullSpeedTemperatureK,
        coolingParameters.oilThermostatFullOpenTemperatureK) + 1.0;
    return EngineCoolingModel::calculate(
        m_parameters, maximumOperatingPoint, maximumTemperatureK, maximumTemperatureK);
}

double EngineThermalModel::calculateMinimumPistonCapacityJPerK() const {
    return std::min_element(
        m_cylinderProperties.begin(), m_cylinderProperties.end(),
        [](const auto &left, const auto &right) {
            return left.pistonMassKg < right.pistonMassKg;
        })->pistonMassKg * m_parameters.pistonSpecificHeatJPerKgK;
}

bool EngineThermalModel::baseNodesAreStable(const EngineCoolingState &cooling) const {
    const auto &coolingParameters = m_parameters.cooling;
    const double pistonConductance = m_parameters.pistonCylinderConductanceWPerK
        + m_parameters.pistonOilConductanceWPerK;
    const double cylinderConductance = m_parameters.pistonCylinderConductanceWPerK
        + m_parameters.cylinderOilConductanceWPerK
        + cooling.cylinderAmbientConductanceWPerK
        + cooling.cylinderCoolantConductanceWPerK;
    const double oilConductance = m_cylinderProperties.size()
        * (m_parameters.pistonOilConductanceWPerK + m_parameters.cylinderOilConductanceWPerK)
        + m_parameters.oilAmbientConductanceWPerK
        + coolingParameters.oilSumpConductanceWPerK
        + cooling.oilCoolerAmbientConductanceWPerK
        + cooling.oilCoolantConductanceWPerK;
    return nodeIsStable(m_parameters.updateIntervalSeconds,
        calculateMinimumPistonCapacityJPerK(), pistonConductance)
        && nodeIsStable(m_parameters.updateIntervalSeconds,
            m_parameters.cylinderThermalCapacityJPerK, cylinderConductance)
        && nodeIsStable(m_parameters.updateIntervalSeconds,
            m_parameters.oilThermalCapacityJPerK(), oilConductance);
}

bool EngineThermalModel::extendedNodesAreStable(const EngineCoolingState &cooling) const {
    const auto &coolingParameters = m_parameters.cooling;
    const double sumpConductance = coolingParameters.oilSumpConductanceWPerK
        + cooling.sumpAmbientConductanceWPerK;
    const double coolantConductance = m_cylinderProperties.size()
        * cooling.cylinderCoolantConductanceWPerK
        + cooling.oilCoolantConductanceWPerK
        + cooling.radiatorAmbientConductanceWPerK;
    const bool sumpStable = !coolingParameters.hasSump()
        || nodeIsStable(m_parameters.updateIntervalSeconds, coolingParameters.sumpThermalCapacityJPerK, sumpConductance);
    const bool coolantStable = !coolingParameters.hasCoolant()
        || nodeIsStable(m_parameters.updateIntervalSeconds, coolingParameters.coolantThermalCapacityJPerK, coolantConductance);
    return sumpStable && coolantStable;
}

bool EngineThermalModel::sampleIsFinite(const CylinderThermalSample &sample) const {
    return std::isfinite(sample.gasTemperatureK)
        && std::isfinite(sample.gasPressurePa)
        && std::isfinite(sample.chamberVolumeM3)
        && std::isfinite(sample.meanPistonSpeedMPerS)
        && std::isfinite(sample.frictionPowerW)
        && sample.frictionPowerW >= 0.0;
}

bool EngineThermalModel::operatingPointIsFinite(
    const EngineThermalOperatingPoint &operatingPoint) const
{
    return std::isfinite(operatingPoint.vehicleSpeedMPerS)
        && std::isfinite(operatingPoint.engineSpeedRadPerSecond);
}

void EngineThermalModel::accumulateSample(
    int cylinderIndex,
    const CylinderThermalSample &sample,
    double dt)
{
    if (sample.gasTemperatureK < MinimumGasTemperatureK
        || sample.gasPressurePa / units::bar < MinimumGasPressureBar
        || sample.chamberVolumeM3 < MinimumChamberVolumeM3
        || sample.meanPistonSpeedMPerS < 0.0) {
        ++m_inputAnomalyCount;
    }
    AccumulatedEnergy &energy = m_accumulatedEnergy[cylinderIndex];
    energy.pistonGasJ += calculatePistonGasPower(cylinderIndex, sample) * dt;
    energy.cylinderGasJ += calculateCylinderGasPower(cylinderIndex, sample) * dt;
    energy.frictionJ += sample.frictionPowerW * dt;
}

void EngineThermalModel::accumulateOperatingPoint(
    const EngineThermalOperatingPoint &operatingPoint,
    double dt)
{
    m_accumulatedVehicleSpeedMeters += std::abs(operatingPoint.vehicleSpeedMPerS) * dt;
    m_accumulatedEngineAngleRadians += std::abs(operatingPoint.engineSpeedRadPerSecond) * dt;
}

void EngineThermalModel::integrateAccumulatedEnergy() {
    PowerSolution solution = calculatePowerSolution(m_accumulatedTimeSeconds);
    const bool balanceValid = energyBalanceIsValid(solution.balance);
    const bool temperaturesUpdated = applyTemperatureChanges(
        solution,
        m_accumulatedTimeSeconds);
    clearAccumulator();
    if (!balanceValid) {
        ++m_inputAnomalyCount;
    }
    if (!temperaturesUpdated) {
        ++m_inputAnomalyCount;
        return;
    }
    m_lastCoolingState = solution.cooling;
    m_lastPowerBalance = solution.balance;
    publishState();
}

EngineThermalOperatingPoint EngineThermalModel::calculateAverageOperatingPoint() const {
    EngineThermalOperatingPoint operatingPoint;
    operatingPoint.vehicleSpeedMPerS = m_accumulatedVehicleSpeedMeters
        / m_accumulatedTimeSeconds;
    operatingPoint.engineSpeedRadPerSecond = m_accumulatedEngineAngleRadians
        / m_accumulatedTimeSeconds;
    return operatingPoint;
}

EngineThermalModel::PowerSolution EngineThermalModel::calculatePowerSolution(
    double elapsedSeconds) const
{
    PowerSolution solution;
    solution.cooling = EngineCoolingModel::calculate(
        m_parameters,
        calculateAverageOperatingPoint(),
        m_oilTemperatureK,
        m_coolantTemperatureK);
    solution.cylinders.resize(m_cylinderStates.size());
    for (int i = 0; i < static_cast<int>(m_cylinderStates.size()); ++i) {
        solution.cylinders[i] = calculateCylinderPowerFlow(
            i,
            elapsedSeconds,
            solution.cooling);
    }
    calculateFluidPowerFlows(solution);
    populatePowerBalance(solution);
    return solution;
}

EngineThermalModel::CylinderPowerFlow EngineThermalModel::calculateCylinderPowerFlow(
    int cylinderIndex,
    double elapsedSeconds,
    const EngineCoolingState &cooling) const
{
    const CylinderThermalState &state = m_cylinderStates[cylinderIndex];
    const AccumulatedEnergy &energy = m_accumulatedEnergy[cylinderIndex];
    CylinderPowerFlow flow;
    flow.gasToPistonW = energy.pistonGasJ / elapsedSeconds;
    flow.gasToCylinderW = energy.cylinderGasJ / elapsedSeconds;
    flow.frictionW = energy.frictionJ / elapsedSeconds;
    flow.pistonToCylinderW = m_parameters.pistonCylinderConductanceWPerK
        * (state.pistonTemperatureK - state.cylinderTemperatureK);
    flow.pistonToOilW = m_parameters.pistonOilConductanceWPerK
        * (state.pistonTemperatureK - m_oilTemperatureK);
    flow.cylinderToOilW = m_parameters.cylinderOilConductanceWPerK
        * (state.cylinderTemperatureK - m_oilTemperatureK);
    flow.cylinderToAmbientW = cooling.cylinderAmbientConductanceWPerK
        * (state.cylinderTemperatureK - m_parameters.ambientTemperatureK);
    flow.cylinderToCoolantW = cooling.cylinderCoolantConductanceWPerK
        * (state.cylinderTemperatureK - m_coolantTemperatureK);
    calculateCylinderStoragePowers(flow);
    return flow;
}

void EngineThermalModel::calculateCylinderStoragePowers(CylinderPowerFlow &flow) const {
    flow.pistonStorageW = flow.gasToPistonW
        + m_parameters.pistonFrictionHeatFraction * flow.frictionW
        - flow.pistonToCylinderW - flow.pistonToOilW;
    flow.cylinderStorageW = flow.gasToCylinderW
        + m_parameters.cylinderFrictionHeatFraction * flow.frictionW
        + flow.pistonToCylinderW - flow.cylinderToOilW
        - flow.cylinderToAmbientW - flow.cylinderToCoolantW;
}

void EngineThermalModel::calculateFluidPowerFlows(PowerSolution &solution) const {
    solution.oilStorageW = calculateOilStoragePower(solution);
    solution.sumpStorageW = calculateSumpStoragePower(solution);
    solution.coolantStorageW = calculateCoolantStoragePower(solution);
}

double EngineThermalModel::calculateOilStoragePower(PowerSolution &solution) const {
    EngineThermalPowerBalance &balance = solution.balance;
    balance.oilToAmbientW = m_parameters.oilAmbientConductanceWPerK
        * (m_oilTemperatureK - m_parameters.ambientTemperatureK);
    balance.oilToSumpW = m_parameters.cooling.hasSump()
        ? m_parameters.cooling.oilSumpConductanceWPerK
            * (m_oilTemperatureK - m_sumpTemperatureK)
        : 0.0;
    balance.oilCoolerToAmbientW = solution.cooling.oilCoolerAmbientConductanceWPerK
        * (m_oilTemperatureK - m_parameters.ambientTemperatureK);
    balance.oilToCoolantW = m_parameters.cooling.hasCoolant()
        ? solution.cooling.oilCoolantConductanceWPerK
            * (m_oilTemperatureK - m_coolantTemperatureK)
        : 0.0;
    double oilInputW = 0.0;
    for (const CylinderPowerFlow &flow : solution.cylinders) {
        oilInputW += flow.pistonToOilW + flow.cylinderToOilW
            + m_parameters.oilFrictionHeatFraction * flow.frictionW;
    }
    return oilInputW - balance.oilToAmbientW - balance.oilToSumpW
        - balance.oilCoolerToAmbientW - balance.oilToCoolantW;
}

double EngineThermalModel::calculateSumpStoragePower(PowerSolution &solution) const {
    if (!m_parameters.cooling.hasSump()) {
        return 0.0;
    }
    solution.balance.sumpToAmbientW = solution.cooling.sumpAmbientConductanceWPerK
        * (m_sumpTemperatureK - m_parameters.ambientTemperatureK);
    return solution.balance.oilToSumpW - solution.balance.sumpToAmbientW;
}

double EngineThermalModel::calculateCoolantStoragePower(PowerSolution &solution) const {
    if (!m_parameters.cooling.hasCoolant()) {
        return 0.0;
    }
    double cylinderInputW = 0.0;
    for (const CylinderPowerFlow &flow : solution.cylinders) {
        cylinderInputW += flow.cylinderToCoolantW;
    }
    solution.balance.radiatorToAmbientW = solution.cooling.radiatorAmbientConductanceWPerK
        * (m_coolantTemperatureK - m_parameters.ambientTemperatureK);
    return cylinderInputW + solution.balance.oilToCoolantW
        - solution.balance.radiatorToAmbientW;
}

void EngineThermalModel::populatePowerBalance(PowerSolution &solution) const {
    EngineThermalPowerBalance &balance = solution.balance;
    for (const CylinderPowerFlow &flow : solution.cylinders) {
        addCylinderFlowToBalance(flow, balance);
    }
    balance.frictionToPistonsW = m_parameters.pistonFrictionHeatFraction * balance.frictionW;
    balance.frictionToCylindersW = m_parameters.cylinderFrictionHeatFraction * balance.frictionW;
    balance.frictionToOilW = m_parameters.oilFrictionHeatFraction * balance.frictionW;
    balance.oilStorageW = solution.oilStorageW;
    balance.sumpStorageW = solution.sumpStorageW;
    balance.coolantStorageW = solution.coolantStorageW;
    balance.ambientRejectionW = balance.cylinderToAmbientW
        + balance.oilToAmbientW + balance.sumpToAmbientW
        + balance.oilCoolerToAmbientW + balance.radiatorToAmbientW;
    const double inputW = balance.gasToPistonsW
        + balance.gasToCylindersW + balance.frictionW;
    const double storageW = balance.pistonStorageW + balance.cylinderStorageW
        + balance.oilStorageW + balance.sumpStorageW + balance.coolantStorageW;
    balance.energyResidualW = storageW - (inputW - balance.ambientRejectionW);
    balance.relativeEnergyResidual = std::abs(balance.energyResidualW)
        / std::max(1.0, std::abs(inputW) + std::abs(balance.ambientRejectionW));
}

void EngineThermalModel::addCylinderFlowToBalance(
    const CylinderPowerFlow &flow,
    EngineThermalPowerBalance &balance) const
{
    balance.gasToPistonsW += flow.gasToPistonW;
    balance.gasToCylindersW += flow.gasToCylinderW;
    balance.frictionW += flow.frictionW;
    balance.pistonToCylinderW += flow.pistonToCylinderW;
    balance.pistonToOilW += flow.pistonToOilW;
    balance.cylinderToOilW += flow.cylinderToOilW;
    balance.cylinderToAmbientW += flow.cylinderToAmbientW;
    balance.cylinderToCoolantW += flow.cylinderToCoolantW;
    balance.pistonStorageW += flow.pistonStorageW;
    balance.cylinderStorageW += flow.cylinderStorageW;
}

bool EngineThermalModel::applyTemperatureChanges(
    const PowerSolution &solution,
    double dt)
{
    std::vector<CylinderThermalState> nextCylinderStates =
        calculateNextCylinderStates(solution, dt);
    const FluidTemperatures nextFluids = calculateNextFluidTemperatures(solution, dt);
    if (!temperaturesAreValid(nextCylinderStates, nextFluids.oilK,
        nextFluids.sumpK, nextFluids.coolantK)) {
        return false;
    }
    m_cylinderStates = std::move(nextCylinderStates);
    m_oilTemperatureK = nextFluids.oilK;
    m_sumpTemperatureK = nextFluids.sumpK;
    m_coolantTemperatureK = nextFluids.coolantK;
    return true;
}

std::vector<CylinderThermalState> EngineThermalModel::calculateNextCylinderStates(
    const PowerSolution &solution,
    double dt) const
{
    std::vector<CylinderThermalState> nextStates = m_cylinderStates;
    for (int i = 0; i < static_cast<int>(m_cylinderStates.size()); ++i) {
        const double pistonCapacity = m_cylinderProperties[i].pistonMassKg
            * m_parameters.pistonSpecificHeatJPerKgK;
        nextStates[i].pistonTemperatureK += dt
            * solution.cylinders[i].pistonStorageW / pistonCapacity;
        nextStates[i].cylinderTemperatureK += dt
            * solution.cylinders[i].cylinderStorageW
            / m_parameters.cylinderThermalCapacityJPerK;
    }
    return nextStates;
}

EngineThermalModel::FluidTemperatures EngineThermalModel::calculateNextFluidTemperatures(
    const PowerSolution &solution,
    double dt) const
{
    FluidTemperatures next;
    next.oilK = m_oilTemperatureK
        + dt * solution.oilStorageW / m_parameters.oilThermalCapacityJPerK();
    next.sumpK = m_parameters.cooling.hasSump()
        ? m_sumpTemperatureK + dt * solution.sumpStorageW
            / m_parameters.cooling.sumpThermalCapacityJPerK
        : m_sumpTemperatureK;
    next.coolantK = m_parameters.cooling.hasCoolant()
        ? m_coolantTemperatureK + dt * solution.coolantStorageW
            / m_parameters.cooling.coolantThermalCapacityJPerK
        : m_coolantTemperatureK;
    return next;
}

bool EngineThermalModel::temperaturesAreValid(
    const std::vector<CylinderThermalState> &cylinderStates,
    double oilTemperatureK,
    double sumpTemperatureK,
    double coolantTemperatureK) const
{
    if (!isPositiveFinite(oilTemperatureK)
        || !isPositiveFinite(sumpTemperatureK)
        || !isPositiveFinite(coolantTemperatureK)) {
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

bool EngineThermalModel::energyBalanceIsValid(
    const EngineThermalPowerBalance &balance) const
{
    return std::isfinite(balance.relativeEnergyResidual)
        && balance.relativeEnergyResidual < MaximumRelativeEnergyResidual;
}

void EngineThermalModel::publishState() {
    m_state.revision += 1;
    m_state.inputAnomalyCount = m_inputAnomalyCount;
    m_state.valid = true;
    m_state.oilTemperatureK = m_oilTemperatureK;
    m_state.sumpTemperatureK = m_sumpTemperatureK;
    m_state.coolantTemperatureK = m_coolantTemperatureK;
    m_state.sumpModeled = m_parameters.cooling.hasSump();
    m_state.coolantModeled = m_parameters.cooling.hasCoolant();
    m_state.cooling = m_lastCoolingState;
    m_state.powerBalance = m_lastPowerBalance;
    m_state.cylinders = m_cylinderStates;
    publishAverageTemperatures();
    publishMaximumTemperatures();
}

void EngineThermalModel::publishAverageTemperatures() {
    const double count = static_cast<double>(m_cylinderStates.size());
    m_state.averagePistonTemperatureK = std::accumulate(
        m_cylinderStates.begin(), m_cylinderStates.end(), 0.0,
        [](double sum, const auto &state) {
            return sum + state.pistonTemperatureK;
        }) / count;
    m_state.averageCylinderTemperatureK = std::accumulate(
        m_cylinderStates.begin(), m_cylinderStates.end(), 0.0,
        [](double sum, const auto &state) {
            return sum + state.cylinderTemperatureK;
        }) / count;
}

void EngineThermalModel::publishMaximumTemperatures() {
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
    m_accumulatedVehicleSpeedMeters = 0.0;
    m_accumulatedEngineAngleRadians = 0.0;
}

double EngineThermalModel::calculatePistonGasPower(
    int cylinderIndex,
    const CylinderThermalSample &sample) const
{
    const double heatTransferCoefficient =
        calculateHohenbergHeatTransferCoefficient(m_parameters, sample);
    const double pistonArea = m_parameters.pistonGasSurfaceFactor
        * boreArea(m_cylinderProperties[cylinderIndex].boreM);
    const double gasTemperatureK = std::max(
        sample.gasTemperatureK,
        MinimumGasTemperatureK);
    return m_parameters.pistonGasHeatTransferFactor
        * heatTransferCoefficient * pistonArea
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
    const double gasTemperatureK = std::max(
        sample.gasTemperatureK,
        MinimumGasTemperatureK);
    return m_parameters.cylinderGasHeatTransferFactor
        * calculateHohenbergHeatTransferCoefficient(m_parameters, sample)
        * cylinderArea
        * (gasTemperatureK - m_cylinderStates[cylinderIndex].cylinderTemperatureK);
}
