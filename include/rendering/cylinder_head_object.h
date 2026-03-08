#ifndef ATG_ENGINE_SIM_CYLINDER_HEAD_OBJECT_H
#define ATG_ENGINE_SIM_CYLINDER_HEAD_OBJECT_H

#include "rendering/simulation_object.h"

#include "domain/engine/cylinder_head.h"
#include "rendering/geometry_generator.h"
#include "domain/engine/engine.h"

class CylinderHeadObject : public SimulationObject {
    public:
        CylinderHeadObject();
        virtual ~CylinderHeadObject();

        virtual void generateGeometry();
        virtual void render(const ViewParameters *view);
        virtual void process(float dt);
        virtual void destroy();

        CylinderHead *m_head;
        Engine *m_engine;

    protected:
        void generateCamshaft(
            Camshaft *camshaft,
            double padding,
            double rollerRadius,
            GeometryGenerator::GeometryIndices *indices);
};

#endif /* ATG_ENGINE_SIM_CYLINDER_HEAD_OBJECT_H */
