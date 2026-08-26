<<<<<<< HEAD
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
	void onShow() override;
	void onHide() override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	BatteryInformation mBatteryInfo;
	int mUpdateElapsed;
	bool mActive;
};

=======
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
	void onShow() override;
	void onHide() override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

private:
	BatteryInformation mBatteryInfo;
	int mUpdateElapsed;
	bool mActive;
};

>>>>>>> 1eee1723 (added format version7 theme support)
#endif // ES_CORE_COMPONENTS_BATTTEXT_COMPONENT_H