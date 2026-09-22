#include "GuiButton.h"
#include "FontRenderer.h"
#include "GameSettings.h"
#include "RenderEngine.h"
#include "Minecraft.h"
#include "platform/RenderAPI.h"

GuiButton::GuiButton(int_t id_, int_t x, int_t y, const std::string &text)
	: GuiButton(id_, x, y, 200, 20, text)
{
}

GuiButton::GuiButton(int_t id_, int_t x, int_t y, int_t w, int_t h, const std::string &text)
	: width(w)
	, height(h)
	, xPosition(x)
	, yPosition(y)
	, displayString(text)
	, id(id_)
	, enabled(true)
	, enabled2(true)
	, keyboardSelected(false)
{
}

int_t GuiButton::getHoverState(bool hovered)
{
	int_t state = 1;
	if (!enabled)
	{
		state = 0;
	}
	else if (hovered)
	{
		state = 2;
	}
	return state;
}

void GuiButton::drawButton(Minecraft *mc, int_t mouseX, int_t mouseY)
{
	if (!enabled2) return;

	FontRenderer *fontrenderer = mc->fontRenderer;
	bool hovered = keyboardSelected || (mouseX >= xPosition && mouseY >= yPosition
	            && mouseX < xPosition + width && mouseY < yPosition + height);
	int_t k = getHoverState(hovered);
	const int_t buttonStyle = mc->gameSettings != nullptr ? mc->gameSettings->buttonStyle : 0;
	const int_t clampedStyle = buttonStyle < 0 ? 0 : (buttonStyle > 2 ? 2 : buttonStyle);

	if (clampedStyle == 0)
	{
		const int_t textureY = 46 + k * 20;
		renderBindTexture(mc->renderEngine->getTexture("/gui/gui.png"));
		renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		drawTexturedModalRect(xPosition, yPosition, 0, textureY, width / 2, height);
		drawTexturedModalRect(xPosition + width / 2, yPosition, 200 - width / 2, textureY, width / 2, height);
	}
	else
	{
		renderBindTexture(mc->renderEngine->getTexture("/gui/cbuttons.png"));
		renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		const int_t textureY = 107 + ((clampedStyle - 1) * 60) + (k * 20);

		drawTexturedModalRect(xPosition, yPosition, 0, textureY, width / 2, height);
		drawTexturedModalRect(xPosition + width / 2, yPosition, 200 - width / 2, textureY, width / 2, height);
	}

	mouseDragged(mc, mouseX, mouseY);

	if (!enabled)
	{
		drawCenteredString(fontrenderer, displayString, xPosition + width / 2, yPosition + (height - 8) / 2, 0xffa0a0a0);
	}
	else if (hovered)
	{
		drawCenteredString(fontrenderer, displayString, xPosition + width / 2, yPosition + (height - 8) / 2, 0xffffa0);
	}
	else
	{
		drawCenteredString(fontrenderer, displayString, xPosition + width / 2, yPosition + (height - 8) / 2, 0xe0e0e0);
	}
}

void GuiButton::mouseDragged(Minecraft *mc, int_t mouseX, int_t mouseY)
{
}

void GuiButton::mouseReleased(int_t mouseX, int_t mouseY)
{
}

bool GuiButton::mousePressed(Minecraft *mc, int_t mouseX, int_t mouseY)
{
	return enabled && enabled2
	    && mouseX >= xPosition && mouseY >= yPosition
	    && mouseX < xPosition + width && mouseY < yPosition + height;
}

void GuiButton::setKeyboardSelected(bool selected)
{
	keyboardSelected = selected;
}
