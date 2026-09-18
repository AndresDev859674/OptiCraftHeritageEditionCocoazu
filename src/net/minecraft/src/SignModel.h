#pragma once

class ModelRenderer;

// net.minecraft.src.SignModel
class SignModel
{
public:
	SignModel();
	~SignModel();

	void renderSign();
	void renderStaticPart();
	void prepareStaticPart();

	ModelRenderer *signBoard;
	ModelRenderer *signStick;
};
