#pragma once
#ifndef ES_CORE_COMPONENTS_CLOCK_COMPONENT_H
#define ES_CORE_COMPONENTS_CLOCK_COMPONENT_H

#include "GuiComponent.h"
#include "components/TextComponent.h"

class Window;

class ClockComponent : public TextComponent
{
public:
	ClockComponent(Window* window);

	virtual void update(int deltaTime);
	void onShow() override;
	void onHide() override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	int mClockElapsed;
	bool mActive;
};

#endif // ES_CORE_COMPONENTS_CLOCK_COMPONENT_H