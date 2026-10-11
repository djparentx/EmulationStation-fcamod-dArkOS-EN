#include "BindingManager.h"

// Ported from AmberELEC ES (es-core/src/BindingManager.cpp)
// Not ported: ComponentBinding / GridTemplateBinding (no item templates in this fork),
// nested bindables ({game:system:name}), and AmberELEC-only globals (netplay, cheevos, ip...)

#include "components/TextComponent.h"
#include "components/StackPanelComponent.h"
#include "renderers/Renderer.h"
#include "utils/MathExpr.h"
#include "utils/StringUtil.h"
#include "EsLocale.h"
#include "GuiComponent.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "platform.h"
#include <algorithm>

BindableProperty BindableProperty::Null;
BindableProperty BindableProperty::EmptyString("", BindablePropertyType::String);

class GlobalBinding : public IBindable
{
	BindableProperty getProperty(const std::string& name) override
	{
		if (name == "help")
			return Settings::getInstance()->getBool("ShowHelpPrompts");

		if (name == "clock")
			return Settings::getInstance()->getBool("DrawClock");

		if (name == "battery")
			return queryBatteryInformation(true).hasBattery;

		if (name == "batteryLevel")
			return queryBatteryInformation(true).level;

		if (name == "screenWidth" || name == "width")
			return Renderer::getScreenWidth();

		if (name == "screenHeight" || name == "height")
			return Renderer::getScreenHeight();

		if (name == "screenRatio" || name == "ratio")
			return Renderer::getScreenHeight() == 0 ? 0.0f : (float)Renderer::getScreenWidth() / (float)Renderer::getScreenHeight();

		if (name == "vertical")
			return Renderer::getScreenHeight() > Renderer::getScreenWidth();

		return BindableProperty::Null;
	}

	std::string getBindableTypeName() override { return "global"; }
};

class SettingsBinding : public IBindable
{
	BindableProperty getProperty(const std::string& name) override
	{
		switch (Settings::getInstance()->getSettingType(name))
		{
		case Settings::SettingType::String:
			return Settings::getInstance()->getString(name);
		case Settings::SettingType::Bool:
			return Settings::getInstance()->getBool(name);
		case Settings::SettingType::Int:
			return Settings::getInstance()->getInt(name);
		case Settings::SettingType::Float:
			return Settings::getInstance()->getFloat(name);
		default:
			break;
		}

		return BindableProperty::Null;
	}

	std::string getBindableTypeName() override { return "settings"; }
};

static GlobalBinding globalBinding;
static SettingsBinding settingsBinding;

// AmberELEC Utils::String::extractStrings (keepDelimiter = false)
static std::vector<std::string> extractStrings(const std::string& str, const std::string& startDelimiter, const std::string& endDelimiter)
{
	std::vector<std::string> ret;

	size_t pos = 0;
	while (pos != std::string::npos)
	{
		pos = str.find(startDelimiter, pos);
		if (pos == std::string::npos)
			break;

		auto end = str.find(endDelimiter, pos + startDelimiter.size());
		if (end == std::string::npos)
			break;

		std::string value = str.substr(pos + startDelimiter.size(), end - (pos + startDelimiter.size()));
		if (!value.empty())
			ret.push_back(value);

		pos = end + endDelimiter.size();
	}

	return ret;
}

// "{game:image}" alone is plain substitution, never evaluated
static bool isUniqueVariable(const std::string& xp)
{
	return !xp.empty() && xp.front() == '{' && xp.back() == '}' && std::count(xp.cbegin(), xp.cend(), '{') == 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// BindingManager
/////////////////////////////////////////////////////////////////////////////////////////////

void BindingManager::bindValues(IBindable* current, std::string& xp, bool showDefaultText, std::string& evaluableExpression)
{
	std::string typeName = current->getBindableTypeName();

	for (auto name : extractStrings(xp, "{" + typeName + ":", "}"))
	{
		std::string dataAsString;
		std::string dataAsEvaluable;

		auto value = current->getProperty(name);
		switch (value.type)
		{
		case BindablePropertyType::String:
		case BindablePropertyType::Path:
			dataAsString = value.s;
			dataAsEvaluable = "\"" + Utils::String::replace(value.s, "\"", "") + "\"";
			break;
		case BindablePropertyType::Bool:
			dataAsString = value.b ? _("YES") : _("NO");
			dataAsEvaluable = value.b ? "1" : "0";
			break;
		case BindablePropertyType::Int:
			dataAsString = std::to_string(value.i);
			dataAsEvaluable = dataAsString;
			break;
		case BindablePropertyType::Float:
			dataAsString = std::to_string(value.f);
			dataAsEvaluable = dataAsString;
			break;
		default:
			dataAsEvaluable = "\"\"";
			break;
		}

		if (showDefaultText && value.type != BindablePropertyType::Path)
			dataAsString = dataAsString.empty() ? _("Unknown") : dataAsString == "0" ? _("None") : dataAsString;

		xp = Utils::String::replace(xp, "{" + typeName + ":" + name + "}", dataAsString);
		evaluableExpression = Utils::String::replace(evaluableExpression, "{" + typeName + ":" + name + "}", dataAsEvaluable);
	}
}

std::string BindingManager::updateBoundExpression(std::string& xp, IBindable* bindable, bool showDefaultText)
{
	std::string evaluableExpression = xp;

	if (bindable == nullptr)
	{
		for (auto name : extractStrings(xp, "{", "}"))
		{
			if (name.find(":") == std::string::npos)
				continue;

			if (Utils::String::startsWith(name, "global:") || Utils::String::startsWith(name, "settings:"))
				continue;

			xp = Utils::String::replace(xp, "{" + name + "}", "");
		}

		evaluableExpression = xp;
	}
	else
	{
		xp = Utils::String::replace(xp, "{binding:", "{system:"); // Retrocompatibility for old {binding: which is {system
		evaluableExpression = xp;

		IBindable* current = bindable;

		while (current != nullptr)
		{
			bindValues(current, xp, showDefaultText, evaluableExpression);
			current = current->getBindableParent();
		}
	}

	bindValues(&globalBinding, xp, showDefaultText, evaluableExpression);
	bindValues(&settingsBinding, xp, showDefaultText, evaluableExpression);

	return evaluableExpression;
}

bool BindingManager::evaluateBoolean(const std::string& expression, IBindable* bindable)
{
	if (expression.empty())
		return true;

	std::string xp = expression;
	std::string evaluableExpression = updateBoundExpression(xp, bindable, false);

	if (evaluableExpression == "1")
		return true;

	if (evaluableExpression == "0" || isUniqueVariable(expression))
		return false;

	try
	{
		auto ret = Utils::MathExpr::evaluate(evaluableExpression.c_str());
		if (ret.type == Utils::MathExpr::NUMBER)
			return ret.number != 0;
	}
	catch (...) { }

	return false;
}

void BindingManager::updateBindings(GuiComponent* comp, IBindable* bindable, bool recursive)
{
	if (comp == nullptr)
		return;

	typedef ThemeData::ThemeElement::Property::PropertyType PropType;

	TextComponent* text = dynamic_cast<TextComponent*>(comp);
	bool showDefaultText = text != nullptr && text->getBindingDefaults();

	auto expressions = comp->getBindingExpressions();
	for (auto expression : expressions)
	{
		std::string xp = expression.second;
		if (xp.empty())
			continue;

		std::string propertyName = expression.first;

		auto existing = comp->getProperty(propertyName);
		if (existing.type == PropType::Unknown)
			continue;

		bool uniqueVariable = isUniqueVariable(xp);

		std::string evaluableExpression = updateBoundExpression(xp, bindable, showDefaultText);

		switch (existing.type)
		{
		case PropType::String:
			if (bindable != nullptr && !uniqueVariable)
			{
				try
				{
					auto ret = Utils::MathExpr::evaluate(evaluableExpression.c_str());
					if (ret.type == Utils::MathExpr::STRING)
						xp = ret.string;
					else if (ret.type == Utils::MathExpr::NUMBER)
						xp = std::to_string((int)ret.number);
				}
				catch (...) { } // not an expression, e.g. "Players: {game:players}" - plain substitution
			}

			comp->setProperty(propertyName, Utils::String::trim(xp));
			break;

		case PropType::Int:
			{
				int value = Utils::String::toInteger(xp);

				if (xp != "0" && xp != "1" && bindable != nullptr && !uniqueVariable)
				{
					try
					{
						auto ret = Utils::MathExpr::evaluate(evaluableExpression.c_str());
						if (ret.type == Utils::MathExpr::NUMBER)
							value = (int)ret.number;
					}
					catch (...) { }
				}

				comp->setProperty(propertyName, (unsigned int)value);
			}
			break;

		case PropType::Float:
			{
				float value = Utils::String::toFloat(xp);

				if (bindable != nullptr && !uniqueVariable)
				{
					try
					{
						auto ret = Utils::MathExpr::evaluate(evaluableExpression.c_str());
						if (ret.type == Utils::MathExpr::NUMBER)
							value = ret.number;
					}
					catch (...) { }
				}

				comp->setProperty(propertyName, value);
			}
			break;

		case PropType::Bool:
			if (evaluableExpression == "1")
				comp->setProperty(propertyName, true);
			else if (evaluableExpression == "0")
				comp->setProperty(propertyName, false);
			else
			{
				bool value = false;

				if (bindable != nullptr && !uniqueVariable)
				{
					try
					{
						auto ret = Utils::MathExpr::evaluate(evaluableExpression.c_str());
						if (ret.type == Utils::MathExpr::NUMBER)
							value = (ret.number != 0);
					}
					catch (...) { }
				}

				comp->setProperty(propertyName, value);
			}
			break;

		default:
			break;
		}
	}

	if (recursive)
	{
		for (unsigned int i = 0; i < comp->getChildCount(); i++)
			updateBindings(comp->getChild(i), bindable, recursive);

		StackPanelComponent* stack = dynamic_cast<StackPanelComponent*>(comp);
		if (stack != nullptr)
			stack->onSizeChanged();
	}
}
