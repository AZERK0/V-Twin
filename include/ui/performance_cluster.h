#ifndef ATG_ENGINE_SIM_PERFORMANCE_CLUSTER_H
#define ATG_ENGINE_SIM_PERFORMANCE_CLUSTER_H

#include "ui/ui_element.h"

#include "domain/engine/engine.h"
#include "ui/gauge.h"
#include "ui/cylinder_temperature_gauge.h"
#include "ui/cylinder_pressure_gauge.h"
#include "ui/labeled_gauge.h"
#include "ui/throttle_display.h"
#include "simulation/simulator.h"

class PerformanceCluster : public UiElement {
    public:
        PerformanceCluster();
        virtual ~PerformanceCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();

        void setSimulator(Simulator *simulator) { m_simulator = simulator; }
        void addTimePerTimestepSample(double sample);
        void addAudioLatencySample(double sample);
        void addInputBufferUsageSample(double sample);

        LabeledGauge *m_timePerTimestepGauge;
        LabeledGauge *m_fpsGauge;
        LabeledGauge *m_simSpeedGauge;
        LabeledGauge *m_simulationFrequencyGauge;
        LabeledGauge *m_inputSamplesGauge;
        LabeledGauge *m_audioLagGauge;

    protected:
        double m_timePerTimestep;

        double m_filteredSimulationFrequency;
        double m_audioLatency;
        double m_inputBufferUsage;

        Simulator *m_simulator;
};

#endif /* ATG_ENGINE_SIM_PERFORMANCE_CLUSTER_H */
