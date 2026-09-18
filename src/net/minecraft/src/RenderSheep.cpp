#include "RenderSheep.h"
#include "EntitySheep.h"
#include "ModelBase.h"
#include "EntityLiving.h"
#include "Entity.h"
#include "RenderManager.h"
#include "platform/PlatformTuning.h"
#include "platform/RenderAPI.h"

RenderSheep::RenderSheep(ModelBase* modelBase, ModelBase* modelBase2, float shadowSize) : RenderLiving(modelBase, shadowSize) {
    setRenderPassModel(modelBase2);
}

RenderSheep::~RenderSheep()
{
    delete renderPassModel;
    renderPassModel = nullptr;
}

void RenderSheep::renderSheep(EntitySheep* entitySheep, double d, double d1, double d2, float f, float f1) {
    RenderLiving::doRenderLiving(entitySheep, d, d1, d2, f, f1);
}

void RenderSheep::doRenderLiving(EntityLiving* entityLiving, double d, double d1, double d2, float f, float f1) {
    renderSheep((EntitySheep*)entityLiving, d, d1, d2, f, f1);
}

void RenderSheep::doRender(Entity* entity, double d, double d1, double d2, float f, float f1) {
    renderSheep((EntitySheep*)entity, d, d1, d2, f, f1);
}

int RenderSheep::getColorMultiplier(EntityLiving* entityLiving, float f, float f1) {
    return getSheepColorMultiplier((EntitySheep*)entityLiving, f, f1);
}

int RenderSheep::getSheepColorMultiplier(EntitySheep* entitySheep, float f, float f1) {
    return entitySheep->getFleeceColor();
}

int RenderSheep::shouldRenderPass(EntityLiving* entityLiving, int i, float f) {
    return renderSheepFurPass((EntitySheep*)entityLiving, i, f);
}

int RenderSheep::renderSheepFurPass(EntitySheep* entitySheep, int i, float f) {
    if (i == 0 && !entitySheep->getSheared()) {
        if (PLATFORM_SHEEP_WOOL_LOD_DISTANCE_SQ > 0.0f) {
            // See PS2_SHEEP_WOOL_LOD_DISTANCE_SQ. Same viewer position the
            // entity was placed against this frame.
            const double dx = entitySheep->posX - RenderManager::renderPosX;
            const double dy = entitySheep->posY - RenderManager::renderPosY;
            const double dz = entitySheep->posZ - RenderManager::renderPosZ;
            if (dx * dx + dy * dy + dz * dz > (double)PLATFORM_SHEEP_WOOL_LOD_DISTANCE_SQ)
                return -1;
        }
        loadTexture("/mob/sheep_fur.png");
        float f1 = 1.0f;
        int j = entitySheep->getFleeceColor();
        renderColor3f(f1 * EntitySheep::fleeceColorTable[j][0], f1 * EntitySheep::fleeceColorTable[j][1], f1 * EntitySheep::fleeceColorTable[j][2]);
        return 1;
    } else {
        return -1;
    }
}