#include "animations/ThemeStoryboard.h"
#include "utils/StringUtil.h"
#include <pugixml/src/pugixml.hpp>
#include <cstdlib>

static int parseRepeat(const std::string& value, int defaultValue)
{
	if (value == "forever" || value == "infinite")
		return 0;
	if (value.empty() || value == "none")
		return defaultValue;
	return atoi(value.c_str());
}

static std::string attr(const pugi::xml_node& node, const char* name, const char* altName = nullptr)
{
	if (node.attribute(name))
		return node.attribute(name).as_string();
	if (altName != nullptr && node.attribute(altName))
		return node.attribute(altName).as_string();
	return "";
}

static StoryboardEasing parseEasing(const std::string& value)
{
	std::string mode = Utils::String::toLower(value);

	if (mode == "easein")       return StoryboardEasing::EaseIn;
	if (mode == "easeincubic")  return StoryboardEasing::EaseInCubic;
	if (mode == "easeinquint")  return StoryboardEasing::EaseInQuint;
	if (mode == "easeout")      return StoryboardEasing::EaseOut;
	if (mode == "easeoutcubic") return StoryboardEasing::EaseOutCubic;
	if (mode == "easeoutquint") return StoryboardEasing::EaseOutQuint;
	if (mode == "easeinout")    return StoryboardEasing::EaseInOut;
	if (mode == "bump")         return StoryboardEasing::Bump;
	return StoryboardEasing::Linear;
}

std::shared_ptr<ThemeStoryboard> ThemeStoryboard::fromXml(const pugi::xml_node& root)
{
	auto sb = std::make_shared<ThemeStoryboard>();
	sb->event = root.attribute("event").as_string();
	sb->repeat = parseRepeat(root.attribute("repeat").as_string(), 1);

	for (pugi::xml_node node = root.child("animation"); node; node = node.next_sibling("animation"))
	{
		ThemeAnimation a;
		a.property = node.attribute("property").as_string();
		if (a.property.empty())
			continue;

		if (node.attribute("from"))
		{
			a.hasFrom = true;
			a.from = node.attribute("from").as_string();
		}

		if (node.attribute("to"))
		{
			a.hasTo = true;
			a.to = node.attribute("to").as_string();
		}

		a.begin = node.attribute("begin").as_int(0);
		a.duration = node.attribute("duration").as_int(0);
		a.repeat = parseRepeat(node.attribute("repeat").as_string(), 1);

		std::string autoReverse = attr(node, "autoReverse", "autoreverse");
		a.autoReverse = (autoReverse == "true" || autoReverse == "1");

		a.easing = parseEasing(attr(node, "mode", "easingMode"));

		if (node.attribute("enabled"))
		{
			std::string enabled = node.attribute("enabled").as_string();
			if (enabled.find('{') != std::string::npos && enabled.find(':') != std::string::npos && enabled.find('}') != std::string::npos)
				a.enabledExpr = enabled;
			else
				a.enabled = (enabled == "true" || enabled == "1");
		}

		sb->animations.push_back(a);
	}

	if (sb->animations.empty())
		return nullptr;

	return sb;
}