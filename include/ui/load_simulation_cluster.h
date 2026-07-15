#ifndef ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H
#define ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H

#include "ui/ui_element.h"

#include "simulation/simulator.h"
#include "ui/labeled_gauge.h"

class LoadSimulationCluster : public UiElement {
    public:
        enum class Layout {
            Standard,
            CompactCondition
        };

        LoadSimulationCluster();
        virtual ~LoadSimulationCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();
        void setUnits();

        void setSimulator(Simulator *simulator) { m_simulator = simulator; }
        void setLayout(Layout layout) { m_layout = layout; }

    private:
        Transmission *getTransmission() const { return m_simulator->getTransmission(); }

    protected:
        void drawCurrentGear(const Bounds &bounds);
        void drawClutchPressureGauge(const Bounds &bounds);
        void drawSystemStatus(const Bounds &bounds);
        void updateHpAndTorque(float dt);
        void renderStandard();
        void renderCompactCondition();
        void updateDynoSpeedGauge(const Bounds &bounds);
        void updateTorqueGauge(const Bounds &bounds);
        void updatePowerGauge(const Bounds &bounds);
        void setLayoutVisibility(bool compact);
        bool isIgnitionOn() const;

        float m_systemStatusLights[4];
        LabeledGauge *m_dynoSpeedGauge;
        LabeledGauge *m_torqueGauge;
        LabeledGauge *m_hpGauge;
        LabeledGauge *m_clutchPressureGauge;

        double m_filteredHorsepower;
        double m_filteredTorque;

        double m_peakHorsepowerRpm;
        double m_peakHorsepower;
        double m_peakTorqueRpm;
        double m_peakTorque;
        
        std::string m_powerUnits;
        std::string m_torqueUnits;

        Simulator *m_simulator;
        Layout m_layout;
};

#endif /* ATG_ENGINE_SIM_LOAD_SIMULATION_CLUSTER_H */
