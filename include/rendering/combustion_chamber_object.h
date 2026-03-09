#ifndef ATG_ENGINE_SIM_COMBUSTION_CHAMBER_OBJECT_H
#define ATG_ENGINE_SIM_COMBUSTION_CHAMBER_OBJECT_H

#include "rendering/simulation_object.h"

#include "domain/engine/combustion_chamber.h"
#include "rendering/geometry_generator.h"

class CombustionChamberObject : public SimulationObject {
    public:
        CombustionChamberObject();
        virtual ~CombustionChamberObject();

        virtual void generateGeometry();
        virtual void render(const ViewParameters *view);
        virtual void process(float dt);
        virtual void destroy();

        CombustionChamber *m_chamber;

    protected:
        GeometryGenerator::GeometryIndices m_indices;
};

#endif /* ATG_ENGINE_SIM_COMBUSTION_CHAMBER_OBJECT_H */
