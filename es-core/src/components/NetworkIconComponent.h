#pragma once
#ifndef ES_CORE_COMPONENTS_NETWORK_COMPONENT_H
#define ES_CORE_COMPONENTS_NETWORK_COMPONENT_H

#include "GuiComponent.h"
#include "components/ImageComponent.h"

class Window;

class NetworkIconComponent : public ImageComponent
{
public:
	NetworkIconComponent(Window* window);

	void update(int deltaTime) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	void onShow() override;
	void onHide() override;

private:
	bool mConnected;
	int mUpdateElapsed;
	bool mActive;

	std::string mNetworkIcon;
};

#endif // ES_CORE_COMPONENTS_NETWORK_COMPONENT_H