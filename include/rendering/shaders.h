#ifndef ATG_ENGINE_SIM_SHADERS_H
#define ATG_ENGINE_SIM_SHADERS_H

#include "delta.h"

#include "ui/ui_math.h"

class Shaders : public dbasic::ShaderBase {
    public:
        Shaders();
        ~Shaders();

        ysError Initialize(
                dbasic::ShaderSet *shaderSet,
                ysRenderTarget *mainRenderTarget,
                ysRenderTarget *uiRenderTarget,
                ysShaderProgram *shaderProgram,
                ysInputLayout *inputLayout);
        virtual ysError UseMaterial(dbasic::Material *material);
		virtual void SetObjectTransform(const ysMatrix &mat);
		virtual void ConfigureModel(float scale, dbasic::ModelAsset *model);

        void SetBaseColor(const ysVector &color);
        void ResetBaseColor();
        void SetScale(float x, float y, float z = 1.0f);
        void SetColorReplace(bool colorReplace);
        void SetLit(bool lit);
        void SetTexOffset(float u, float v);
        void SetTexScale(float u, float v);
        void ResetObjectState();
        void SetUiDiffuseTexture(ysTexture *texture);

        dbasic::StageEnableFlags GetRegularFlags() const;
        dbasic::StageEnableFlags GetUiFlags() const;

        void CalculateCamera(
            float width,
            float height,
            const Bounds &cameraBounds,
            float screenWidth,
            float screenHeight,
            float angle = 0.0f);
        void CalculateUiCamera(float screenWidth, float screenHeight);

        void SetClearColor(const ysVector &col);

    public:
        dbasic::ShaderScreenVariables m_screenVariables;
        dbasic::ShaderScreenVariables m_uiScreenVariables;
        dbasic::ShaderObjectVariables m_objectVariables;

        ysVector m_cameraPosition;

    protected:
        dbasic::ShaderStage *m_mainStage;
        dbasic::ShaderStage *m_uiStage;

        dbasic::TextureHandle m_uiStageDiffuseTexture;

        dbasic::LightingControls m_lightingControls;
};

#endif /* ATG_ENGINE_SIM_SHADERS_H */
