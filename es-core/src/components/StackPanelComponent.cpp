#include "components/StackPanelComponent.h"
#include "Window.h"
#include "renderers/Renderer.h"
#include "math/Misc.h"

StackPanelComponent::StackPanelComponent(Window* window) : GuiComponent(window), mHorizontal(true), mReverse(false), mClipChildren(true), mSeparator(0.0f)
{
}

void StackPanelComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	if (mClipChildren)
		Renderer::pushClipRect(Vector2i((int)trans.translation().x(), (int)trans.translation().y()), Vector2i((int)mSize.x(), (int)mSize.y()));

	GuiComponent::renderChildren(trans);

	if (mClipChildren)
		Renderer::popClipRect();
}

void StackPanelComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "stackpanel");
	if (!elem)
		return;

	if (elem->has("orientation"))
		mHorizontal = (elem->get<std::string>("orientation") != "vertical");

	if (elem->has("reverse"))
		mReverse = elem->get<bool>("reverse");

	if (elem->has("clipChildren"))
		mClipChildren = elem->get<bool>("clipChildren");

	if (elem->has("separator"))
	{
		mSeparator = elem->get<float>("separator");
		if (mSeparator > 0 && mSeparator < 1)
		{
			Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mSeparator *= mHorizontal ? scale.y() : scale.x();
		}
	}

	performLayout();
}

void StackPanelComponent::onSizeChanged()
{
	performLayout();
}

void StackPanelComponent::performLayout()
{
	float pos = mReverse ? (mHorizontal ? mSize.x() : mSize.y()) : 0.0f;

	for (auto child : mChildren)
	{
		if (!child->isVisible())
			continue;

		if (mHorizontal)
		{
			child->setSize(child->getSize().x(), mSize.y());

			if (mReverse)
			{
				float childpos = pos - child->getSize().x() + child->getSize().x() * child->getOrigin().x();
				child->setPosition(childpos, child->getPosition().y());
				pos -= child->getSize().x() + mSeparator;
			}
			else
			{
				float childpos = pos + child->getSize().x() * child->getOrigin().x();
				child->setPosition(childpos, child->getPosition().y());
				pos += child->getSize().x() + mSeparator;
			}
		}
		else
		{
			child->setSize(mSize.x(), child->getSize().y());

			if (mReverse)
			{
				float childpos = pos - child->getSize().y() + child->getSize().y() * child->getOrigin().y();
				child->setPosition(child->getPosition().x(), childpos);
				pos -= child->getSize().y() + mSeparator;
			}
			else
			{
				float childpos = pos + child->getSize().y() * child->getOrigin().y();
				child->setPosition(child->getPosition().x(), childpos);
				pos += child->getSize().y() + mSeparator;
			}
		}
	}
}

void StackPanelComponent::update(int deltaTime)
{
	Vector2f szBefore;
	for (auto child : mChildren)
		if (child->isVisible())
			szBefore = szBefore + child->getSize();

	GuiComponent::update(deltaTime);

	Vector2f szAfter;
	for (auto child : mChildren)
		if (child->isVisible())
			szAfter = szAfter + child->getSize();

	if (szBefore != szAfter || (mLastSize != Vector2f::Zero() && mLastSize != szAfter))
		performLayout();

	mLastSize = szAfter;
}