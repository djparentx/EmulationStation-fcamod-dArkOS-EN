#include "animations/StoryboardAnimator.h"
#include "animations/ThemeStoryboard.h"
#include "GuiComponent.h"
#include "renderers/Renderer.h"
#include "math/Misc.h"
#include "utils/StringUtil.h"
#include <cmath>
#include <cstdlib>

static float ease(StoryboardEasing easing, float t)
{
	switch (easing)
	{
	case StoryboardEasing::EaseIn:       return t * t;
	case StoryboardEasing::EaseInCubic:  return t * t * t;
	case StoryboardEasing::EaseInQuint:  return t * t * t * t * t;
	case StoryboardEasing::EaseOut:      return 1.0f - (1.0f - t) * (1.0f - t);
	case StoryboardEasing::EaseOutCubic: return 1.0f - powf(1.0f - t, 3.0f);
	case StoryboardEasing::EaseOutQuint: return 1.0f - powf(1.0f - t, 5.0f);
	case StoryboardEasing::EaseInOut:    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
	case StoryboardEasing::Bump:
		{
			// ease-out with overshoot
			const float c1 = 1.70158f, c3 = c1 + 1.0f;
			return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
		}
	default:
		return t;
	}
}

static Vector2f parentSize(GuiComponent* comp)
{
	return comp->getParent() ? comp->getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
}

// number of values a property takes (0 = unsupported)
static int propertyDims(const std::string& prop)
{
	if (prop == "opacity" || prop == "scale" || prop == "x" || prop == "y" || prop == "rotation")
		return 1;
	if (prop == "pos")
		return 2;
	return 0;
}

static void getProperty(GuiComponent* comp, const std::string& prop, float out[2])
{
	Vector2f ps = parentSize(comp);
	Vector3f pos = comp->getPosition();

	if (prop == "opacity")       out[0] = comp->getOpacity() / 255.0f;
	else if (prop == "scale")    out[0] = comp->getScale().x();
	else if (prop == "rotation") out[0] = (float)ES_RAD_TO_DEG(comp->getRotation());
	else if (prop == "x")        out[0] = ps.x() != 0 ? pos.x() / ps.x() : 0;
	else if (prop == "y")        out[0] = ps.y() != 0 ? pos.y() / ps.y() : 0;
	else if (prop == "pos")
	{
		out[0] = ps.x() != 0 ? pos.x() / ps.x() : 0;
		out[1] = ps.y() != 0 ? pos.y() / ps.y() : 0;
	}
}

static void setProperty(GuiComponent* comp, const std::string& prop, const float v[2])
{
	Vector2f ps = parentSize(comp);
	Vector3f pos = comp->getPosition();

	if (prop == "opacity")       comp->setOpacity((unsigned char)(Math::clamp(v[0], 0.0f, 1.0f) * 255.0f));
	else if (prop == "scale")    comp->setScale(Vector3f(v[0], v[0], 1.0f));
	else if (prop == "rotation") comp->setRotationDegrees(v[0]);
	else if (prop == "x")        comp->setPosition(v[0] * ps.x(), pos.y(), pos.z());
	else if (prop == "y")        comp->setPosition(pos.x(), v[0] * ps.y(), pos.z());
	else if (prop == "pos")      comp->setPosition(v[0] * ps.x(), v[1] * ps.y(), pos.z());
}

static void parseValue(const std::string& str, int dims, float out[2])
{
	// split on spaces, skipping empty parts ("0.975  0.775")
	std::vector<std::string> parts;
	for (const auto& p : Utils::String::split(Utils::String::trim(str), ' '))
		if (!p.empty())
			parts.push_back(p);

	out[0] = parts.size() > 0 ? (float)atof(parts[0].c_str()) : 0.0f;
	out[1] = (dims == 2 && parts.size() > 1) ? (float)atof(parts[1].c_str()) : out[0];
}

StoryboardAnimator::StoryboardAnimator(GuiComponent* comp, const std::shared_ptr<ThemeStoryboard>& storyboard,
	const std::function<bool(const std::string&)>& enabledFn)
	: mComponent(comp), mStoryboard(storyboard), mTime(0), mLoop(0), mFinished(false)
{
	for (const auto& anim : mStoryboard->animations)
	{
		Track t;
		t.anim = &anim;
		t.enabled = anim.enabled && propertyDims(anim.property) > 0;

		if (t.enabled && !anim.enabledExpr.empty())
			t.enabled = enabledFn ? enabledFn(anim.enabledExpr) : true;

		mTracks.push_back(t);
	}

	resetTracks();
}

void StoryboardAnimator::resetTracks()
{
	for (auto& t : mTracks)
	{
		t.started = false;
		t.finished = false;
	}
}

void StoryboardAnimator::processTrack(Track& t)
{
	if (!t.enabled || t.finished)
		return;

	const ThemeAnimation& a = *t.anim;
	if (mTime < a.begin)
		return;

	const int dims = propertyDims(a.property);

	if (!t.started)
	{
		t.started = true;

		float cur[2] = { 0, 0 };
		getProperty(mComponent, a.property, cur);

		if (a.hasFrom) parseValue(a.from, dims, t.from); else { t.from[0] = cur[0]; t.from[1] = cur[1]; }
		if (a.hasTo)   parseValue(a.to, dims, t.to);     else { t.to[0] = cur[0];   t.to[1] = cur[1]; }
	}

	const int local = mTime - a.begin;
	float p;
	bool done = false;

	if (a.duration <= 0)
	{
		p = 1.0f;
		done = true;
	}
	else
	{
		const int cycleLen = a.duration * (a.autoReverse ? 2 : 1);
		if (a.repeat > 0 && local >= cycleLen * a.repeat)
		{
			p = a.autoReverse ? 0.0f : 1.0f;
			done = true;
		}
		else
		{
			float q = (float)(local % cycleLen) / (float)a.duration;
			if (q > 1.0f)
				q = 2.0f - q;
			p = q;
		}
	}

	const float e = ease(a.easing, p);
	float v[2];
	v[0] = t.from[0] + (t.to[0] - t.from[0]) * e;
	v[1] = t.from[1] + (t.to[1] - t.from[1]) * e;
	setProperty(mComponent, a.property, v);

	if (done)
		t.finished = true;
}

void StoryboardAnimator::update(int deltaTime)
{
	if (mFinished)
		return;

	mTime += deltaTime;

	// at most 2 passes: current cycle, then (on loop) the begin=0 animations of the next cycle
	for (int pass = 0; pass < 2; pass++)
	{
		int end = 0;
		bool infinite = false;

		for (auto& t : mTracks)
		{
			processTrack(t);

			if (!t.enabled)
				continue;

			const ThemeAnimation& a = *t.anim;
			if (a.repeat == 0 && a.duration > 0)
				infinite = true;
			else
				end = std::max(end, a.begin + a.duration * (a.autoReverse ? 2 : 1) * std::max(a.repeat, 1));
		}

		if (infinite || mTime < end)
			return;

		mLoop++;
		if (end <= 0 || (mStoryboard->repeat != 0 && mLoop >= mStoryboard->repeat))
		{
			mFinished = true;
			return;
		}

		// loop: restore base values and restart, carrying over the overshoot
		mTime -= end;
		mComponent->restoreStoryboardBase();
		resetTracks();
	}
}