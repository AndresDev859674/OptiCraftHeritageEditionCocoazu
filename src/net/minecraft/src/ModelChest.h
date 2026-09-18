#pragma once

#include "ModelBase.h"

class ModelRenderer;

// net.minecraft.src.ModelChest
class ModelChest : public ModelBase
{
public:
    ModelChest();
    ~ModelChest() override;
    virtual void renderAll();
    virtual void renderStaticPart();
    virtual void renderDynamicPart();
    virtual void prepareStaticPart();

    ModelRenderer* chestLid;
    ModelRenderer* chestBelow;
    ModelRenderer* chestKnob;
};
