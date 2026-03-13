#ifndef ATG_ENGINE_SIM_ENGINE_WEAR_MODEL_H
#define ATG_ENGINE_SIM_ENGINE_WEAR_MODEL_H

#include <cstdint>

class Simulator;

struct EngineWearState {
    enum class FailureMode {
        Balanced,
        ThermalOverload,
        LubricationBreakdown,
        DetonationDamage,
        BottomEndFatigue,
        RingSealWear,
        ValvetrainFatigue,
        HeadGasketRisk
    };

    std::uint64_t revision = 0;
    bool valid = false;

    float globalHealth = 1.0f;
    float damageRatePerHour = 0.0f;
    float remainingUsefulLifeHours = 0.0f;
    float confidence = 1.0f;

    float oilTemperatureC = 90.0f;
    float coolantTemperatureC = 88.0f;

    float thermalMargin = 1.0f;
    float lubricationMargin = 1.0f;
    float detonationMargin = 1.0f;
    float mechanicalFatigueLoad = 0.0f;
    float combustionStability = 1.0f;

    float bottomEndDamage = 0.0f;
    float ringSealDamage = 0.0f;
    float valvetrainDamage = 0.0f;
    float headGasketDamage = 0.0f;
    float lubricationSystemDamage = 0.0f;

    float overRevSeconds = 0.0f;
    float overTempSeconds = 0.0f;
    float coldHighLoadSeconds = 0.0f;
    float oilStarvationSeconds = 0.0f;
    int severeKnockEvents = 0;
    int thermalCycles = 0;

    FailureMode dominantFailureMode = FailureMode::Balanced;
};

class EngineWearModel {
    public:
        EngineWearModel();
        ~EngineWearModel();

        void reset();
        void update(const Simulator &simulator, double dt);

        const EngineWearState &getState() const { return m_state; }

    private:
        void stepModel(const Simulator &simulator, double dt);
        void publishSnapshot();

    private:
        EngineWearState m_state;

        double m_modelTimer;
        double m_snapshotTimer;
        double m_elapsedHours;
        double m_lastThrottle;

        double m_thermalMargin;
        double m_lubricationMargin;
        double m_detonationMargin;
        double m_mechanicalFatigueLoad;
        double m_combustionStability;

        double m_oilTemperatureC;
        double m_coolantTemperatureC;

        double m_bottomEndDamage;
        double m_ringSealDamage;
        double m_valvetrainDamage;
        double m_headGasketDamage;
        double m_lubricationSystemDamage;

        double m_bottomEndRate;
        double m_ringSealRate;
        double m_valvetrainRate;
        double m_headGasketRate;
        double m_lubricationSystemRate;

        double m_overRevSeconds;
        double m_overTempSeconds;
        double m_coldHighLoadSeconds;
        double m_oilStarvationSeconds;

        int m_severeKnockEvents;
        int m_thermalCycles;

        bool m_knockEventActive;
        bool m_thermalCycleArmed;
    };

#endif /* ATG_ENGINE_SIM_ENGINE_WEAR_MODEL_H */
