#pragma once
#ifndef ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H
#define ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

class GuiComponent;
struct ThemeStoryboard;
struct ThemeAnimation;

// Runs one ThemeStoryboard on one component. The owning GuiComponent restores its
// base (pre-storyboard) values before starting and on every storyboard loop.
class StoryboardAnimator
{
public:
	StoryboardAnimator(GuiComponent* comp, const std::shared_ptr<ThemeStoryboard>& storyboard,
		const std::function<bool(const std::string&)>& enabledFn);

	void update(int deltaTime);
	bool isFinished() const { return mFinished; }

private:
	struct Track
	{
		const ThemeAnimation* anim;
		bool  enabled;
		bool  started;
		bool  finished;
		float from[2];
		float to[2];
	};

	void resetTracks();
	void processTrack(Track& track);

	GuiComponent* mComponent;
	std::shared_ptr<ThemeStoryboard> mStoryboard;
	std::vector<Track> mTracks;

	int  mTime;
	int  mLoop;
	bool mFinished;
};

#endif // ES_CORE_ANIMATIONS_STORYBOARD_ANIMATOR_H