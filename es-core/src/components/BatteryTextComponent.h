#pragma once
#ifndef ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H
#define ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H

#include "GuiComponent.h"
#include "components/TextComponent.h"
#include "platform.h"

class Window;

class BatteryTextComponent : public TextComponent
{
public:
	BatteryTextComponent(Window* window);

	virtual void update(int deltaTime);

private:
	BatteryInformation mBatteryInfo;
	int mUpdateElapsed;
};

#endif // ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H