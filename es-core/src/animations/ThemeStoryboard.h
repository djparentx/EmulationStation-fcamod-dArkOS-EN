#pragma once
#ifndef ES_CORE_ANIMATIONS_THEME_STORYBOARD_H
#define ES_CORE_ANIMATIONS_THEME_STORYBOARD_H

#include <memory>
#include <string>
#include <vector>

namespace pugi { class xml_node; }

enum class StoryboardEasing { Linear, EaseIn, EaseInCubic, EaseInQuint, EaseOut, EaseOutCubic, EaseOutQuint, EaseInOut, Bump };

struct ThemeAnimation
{
	std::string property;
	std::string from;
	std::string to;
	bool hasFrom = false;
	bool hasTo = false;

	int  begin = 0;      // ms from storyboard start
	int  duration = 0;   // ms, 0 = jump to "to"
	int  repeat = 1;     // 0 = forever
	bool autoReverse = false;
	StoryboardEasing easing = StoryboardEasing::Linear;

	bool        enabled = true;
	std::string enabledExpr; // binding expression, evaluated when the storyboard starts
};

struct ThemeStoryboard
{
	std::string event;   // "", "activate", "deactivate", ...
	int repeat = 1;      // 0 = forever
	std::vector<ThemeAnimation> animations;

	// nullptr if the node holds no <animation> items
	static std::shared_ptr<ThemeStoryboard> fromXml(const pugi::xml_node& node);
};

#endif // ES_CORE_ANIMATIONS_THEME_STORYBOARD_H