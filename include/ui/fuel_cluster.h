#ifndef ATG_ENGINE_SIM_FUEL_CLUSTER_H
#define ATG_ENGINE_SIM_FUEL_CLUSTER_H

#include "ui/ui_element.h"

#include "domain/engine/engine.h"
#include "simulation/simulator.h"

class FuelCluster : public UiElement {
    public:
        FuelCluster();
        virtual ~FuelCluster();

        virtual void initialize(EngineSimApplication *app);
        virtual void destroy();

        virtual void update(float dt);
        virtual void render();

        Engine *m_engine;
        Simulator *m_simulator;

    private:
        double getTotalVolumeFuelConsumed() const;
};

#endif /* ATG_ENGINE_SIM_FUEL_CLUSTER_H */
