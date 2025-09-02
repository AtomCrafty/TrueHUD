#include "Widgets/RecentLoot.h"
#include "Offsets.h"
#include "Settings.h"
#include "ItemData/ItemStack.h"

namespace Scaleform
{
	void RecentLoot::Update([[maybe_unused]] float a_deltaTime)
	{
		_object.Invoke("update");
	}

	void RecentLoot::Initialize()
	{
		LoadConfig();
	}

	void RecentLoot::Dispose()
	{
		_object.Invoke("cleanUp");
	}

	void RecentLoot::AddMessage(std::string_view a_name, uint32_t a_count, const char* a_iconLabel, uint32_t a_iconColor, const QuickLoot::Items::ItemStack* a_stack)
	{
		const RE::GFxValue args[] = {
			RE::GFxValue(a_name),
			RE::GFxValue(a_count),
			a_iconLabel,
			static_cast<int>(a_iconColor),
			a_stack->BuildDataObject(_view.get())
		};

		if (!_object.Invoke("addMessage", nullptr, args, 5)) {
			logger::warn("Failed to invoke addMessage");
		}

		delete a_stack;
	}

	void RecentLoot::UpdatePosition()
	{
		RE::GFxValue::DisplayInfo displayInfo;
		float scale = Settings::fRecentLootScale * 100.f;
		displayInfo.SetScale(scale, scale);

		RE::GRectF rect = _view->GetVisibleFrameRect();
		RE::NiPoint2 screenPos;

		auto def = _view->GetMovieDef();
		if (def) {
			screenPos.x = def->GetWidth();
			screenPos.y = def->GetHeight();
		}

		screenPos.x *= Settings::fRecentLootX;
		screenPos.y *= Settings::fRecentLootY;

		displayInfo.SetPosition(screenPos.x, screenPos.y);

		_object.SetDisplayInfo(displayInfo);
	}

	void RecentLoot::LoadConfig()
	{
		RE::NiPoint2 stageSize;
		auto def = _view->GetMovieDef();
		if (def) {
			stageSize.x = def->GetWidth();
			stageSize.y = def->GetHeight();
		}

		RE::GFxValue args[4];
		args[0].SetNumber(Settings::uRecentLootMaxMessageCount);
		args[1].SetNumber(Settings::fRecentLootMessageDuration);
		args[2].SetNumber((Settings::bRecentLootUseHUDOpacity ? *g_fHUDOpacity : Settings::fRecentLootOpacity) * 100.f);
		args[3].SetNumber(static_cast<uint32_t>(Settings::uRecentLootListDirection));
		_object.Invoke("loadConfig", nullptr, args, 4);

		UpdatePosition();
	}
}
