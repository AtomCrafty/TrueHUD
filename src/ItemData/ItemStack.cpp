#include "ItemStack.h"

#include "ItemDefines.h"
#include "RE/RTTI.h"
#include "RE/A/AIFormulas.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/StandardItemData.h"
#include "SKSE/Impl/PCH.h"

namespace QuickLoot::Items
{
	ItemStack::ItemStack(RE::GFxMovieView* view, RE::ObjectRefHandle container, RE::InventoryEntryData* entry, RE::ObjectRefHandle dropRef) :
		_view(view),
		_object(entry->object),
		_entry(entry),
		_container(std::move(container)),
		_dropRef(std::move(dropRef))
	{
	}

	ItemStack::~ItemStack()
	{
		delete _entry;
	}

	RE::GFxValue& ItemStack::GetData()
	{
		if (!_data.IsObject()) {
			_view->CreateObject(&_data);

			SetVanillaData();
			SkseExtendItemData();
			SkyUiProcessEntry();
			SkyUiSelectIcon();
		}

		return _data;
	}

	std::vector<std::unique_ptr<ItemStack>> ItemStack::LoadContainerInventory(RE::GFxMovieView* view, RE::TESObjectREFR* container, const std::function<bool(RE::TESBoundObject&)>& filter)
	{
		if (!container) {
			return {};
		}

		const auto containerHandle = container->GetHandle();
		const auto changes = container->GetInventoryChanges();

		if (const auto actor = skyrim_cast<RE::Actor*>(container)) {
			RefreshEnchantedWeapons(actor, changes);
		}

		std::vector<std::unique_ptr<ItemStack>> inventory{};

		// Using GetInventoryEntryAt here instead of CLib's GetInventory because that's
		// how the vanilla menu does it. We own the InventoryEntries, so we need to delete
		// them in the ItemStack destructor.

		for (int i = 0;; i++) {
			const auto entry = GetInventoryEntryAt(changes, i);

			if (!entry) {
				break;
			}

			if (!filter(*entry->object)) {
				delete entry;
				continue;
			}

			inventory.emplace_back(std::make_unique<ItemStack>(view, containerHandle, entry));
		}

		// This does not deduplicate, so in the unlikely case that an NPC was dual wielding
		// the same weapon twice it would show as two separate entries.

		if (const auto extraDrops = container->extraList.GetByType<RE::ExtraDroppedItemList>()) {
			for (const auto& handle : extraDrops->droppedItemList) {
				const auto reference = handle.get();

				if (!reference) {
					continue;
				}

				const auto object = reference->GetObjectReference();

				if (!object || !filter(*object)) {
					continue;
				}

				const auto count = reference->extraList.GetCount();
				const auto entry = new RE::InventoryEntryData(object, count);

				// The game uses ExtraDataType::kItemDropper to attach an object reference to the entry,
				// but we can just save the handle directly.

				inventory.emplace_back(std::make_unique<ItemStack>(view, containerHandle, entry, handle));
			}
		}

		return inventory;
	}

	void ItemStack::SetVanillaData()
	{
		// Some hacking is required to create a StandardItemData instance.
		// TODO consider just reimplementing the virtual functions.
		char buffer[sizeof RE::StandardItemData]{};
		const auto vtable = RE::VTABLE_StandardItemData[0].address();
		*reinterpret_cast<uintptr_t*>(buffer) = vtable;

		auto itemData = reinterpret_cast<RE::StandardItemData*>(buffer);

		itemData->objDesc = _entry;
		itemData->owner = _container.native_handle();

		// ItemList::Item constructor
		_data.SetMember("text", itemData->GetName());
		_data.SetMember("count", itemData->GetCount());
		_data.SetMember("equipState", itemData->GetEquipState());
		_data.SetMember("filterFlag", itemData->GetFilterFlag());
		_data.SetMember("favorite", itemData->GetFavorite());
		_data.SetMember("enabled", itemData->GetEnabled());

		// InventoryEntryData::PopulateSoulLevel
		if (_object->Is(RE::FormType::SoulGem)) {
			_data.SetMember("soulLVL", _entry->GetSoulLevel());
		}

		// SetIsStealingFlags
		const auto container = _container.get().get();
		if (container != RE::PlayerCharacter::GetSingleton()) {
			auto owner = _entry->GetOwner();
			if (!owner) {
				const auto actor = skyrim_cast<RE::Actor*>(container);
				owner = actor ? actor : container->GetOwner();
			}

			const auto allowed = IsPlayerAllowedToTakeItemWithValue(RE::PlayerCharacter::GetSingleton(), owner, _entry->GetValue());
			_data.SetMember("isStealing", owner && !allowed);
		}
	}

	// Helpers

	ItemType ItemStack::GetItemType() const
	{
		switch (_object->formType.get()) {
		case RE::FormType::Armor:
			return ItemType::kArmor;

		case RE::FormType::Weapon:
		case RE::FormType::Ammo:
			return ItemType::kWeapon;

		case RE::FormType::Book:
			if (const auto book = skyrim_cast<RE::TESObjectBOOK*>(_object)) {
				return book->TeachesSpell() ? ItemType::kMagicItem : ItemType::kBook;
			}
			return ItemType::kBook;

		case RE::FormType::Ingredient:
			return ItemType::kIngredient;

		case RE::FormType::Misc:
		case RE::FormType::Light:
			return ItemType::kMisc;

		case RE::FormType::KeyMaster:
			return ItemType::kKey;

		case RE::FormType::AlchemyItem:
			if (const auto alchemyItem = skyrim_cast<RE::AlchemyItem*>(_object)) {
				return alchemyItem->IsFood() ? ItemType::kFood : ItemType::kMagicItem;
			}
			return ItemType::kMagicItem;

		case RE::FormType::SoulGem:
			return ItemType::kSoulGem;

		default:
			return ItemType::kNone;
		}
	}

	int ItemStack::GetPickpocketChance() const
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto victim = skyrim_cast<RE::Actor*>(_container.get().get());
		if (!victim) {
			return {};
		}

		const auto count = _entry->countDelta;
		const auto value = victim->GetStealValue(_entry, count, true);
		const auto weight = _object->GetWeight();
		const auto detected = victim->RequestDetectionLevel(player) != 0;
		const auto playerSkill = player->AsActorValueOwner()->GetClampedActorValue(RE::ActorValue::kPickpocket);
		const auto victimSkill = victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kPickpocket);

		return RE::AIFormulas::ComputePickpocketSuccess(playerSkill, victimSkill, value, weight, player, victim, detected, _object);
	}

	RE::GFxValue ItemStack::GetBasicFormInfo(RE::TESForm* form) const
	{
		RE::GFxValue data;
		_view->CreateObject(&data);

		data.SetMember("formType", form->formType.underlying());
		data.SetMember("formId", form->formID);

		if (const auto listForm = skyrim_cast<RE::BGSListForm*>(form)) {
			RE::GFxValue formArray;
			_view->CreateArray(&formArray);
			data.SetMember("forms", formArray);

			listForm->ForEachForm([&](RE::TESForm* childForm) {
				formArray.PushBack(GetBasicFormInfo(childForm));
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}

		return data;
	}

	RE::GFxValue ItemStack::GetKeywords() const
	{
		RE::GFxValue keywords;
		_view->CreateObject(&keywords);

		if (const auto keywordForm = skyrim_cast<RE::BGSKeywordForm*>(_object)) {
			for (uint32_t i = 0; i < keywordForm->GetNumKeywords(); i++) {
				const auto keyword = keywordForm->GetKeywordAt(i).value_or(nullptr);
				if (!keyword)
					continue;

				const auto editorId = keyword->GetFormEditorID();
				if (!editorId || !editorId[0])
					continue;

				keywords.SetMember(editorId, true);
			}
		}

		return keywords;
	}

	double ItemStack::RoundValue(double value)
	{
		return value >= 0 ? floor(value + 0.5) : ceil(value - 0.5);
	}

	RE::GFxValue ItemStack::TruncatePrecision(double value, bool allowNegative)
	{
		if (value <= 0 && !allowNegative) {
			return null;
		}

		return RoundValue(value * 100) / 100;
	}

	EnchantmentType ItemStack::GetEnchantmentType() const
	{
		// https://github.com/ahzaab/moreHUDSE/blob/0b6995a8628cec786f822d2e177eae46dcee0569/src/AHZTarget.cpp#L185

		static const auto magicDisallowEnchanting = KnownForms::MagicDisallowEnchanting.LookupForm<RE::BGSKeyword>();

		const auto enchantable = skyrim_cast<RE::TESEnchantableForm*>(_object);
		auto enchantment = enchantable ? enchantable->formEnchanting : nullptr;

		if (const auto reference = _dropRef.get()) {
			if (const auto extraEnchantment = reference->extraList.GetByType<RE::ExtraEnchantment>()) {
				enchantment = extraEnchantment->enchantment;
			}
		}

		if (!enchantment) {
			return EnchantmentType::kNone;
		}

		if (enchantment->HasKeyword(magicDisallowEnchanting)) {
			return EnchantmentType::kCannotDisenchant;
		}

		if (enchantment->formFlags & RE::TESForm::RecordFlags::kKnown) {
			return EnchantmentType::kKnown;
		}

		if (const auto baseEnchantment = enchantment->data.baseEnchantment) {
			if (baseEnchantment->HasKeyword(magicDisallowEnchanting)) {
				return EnchantmentType::kCannotDisenchant;
			}

			if (baseEnchantment->formFlags & RE::TESForm::RecordFlags::kKnown) {
				return EnchantmentType::kKnown;
			}
		}

		return EnchantmentType::kUnknown;
	}

	// Native calls

	void ItemStack::GetItemCardData(RE::ItemCard* itemCard, RE::InventoryEntryData* entry, bool isContainerItem)
	{
		using func_t = decltype(&GetItemCardData);
		REL::Relocation<func_t> func{ RELOCATION_ID(51019, 51897) };
		return func(itemCard, entry, isContainerItem);
	}

	bool ItemStack::IsPlayerAllowedToTakeItemWithValue(RE::PlayerCharacter* player, RE::TESForm* ownerNpcOrFaction, int value)
	{
		using func_t = decltype(&IsPlayerAllowedToTakeItemWithValue);
		REL::Relocation<func_t> func{ RELOCATION_ID(39584, 40670) };
		return func(player, ownerNpcOrFaction, value);
	}

	void ItemStack::RefreshEnchantedWeapons(RE::Actor* actor, RE::InventoryChanges* changes)
	{
		using func_t = decltype(&RefreshEnchantedWeapons);
		REL::Relocation<func_t> func{ RELOCATION_ID(50946, 51823) };
		return func(actor, changes);
	}

	RE::InventoryEntryData* ItemStack::GetInventoryEntryAt(RE::InventoryChanges* changes, int index)
	{
		using func_t = decltype(&GetInventoryEntryAt);
		REL::Relocation<func_t> func{ RELOCATION_ID(15866, 16106) };
		return func(changes, index);
	}

	RE::ExtraDataList* ItemStack::GetInventoryEntryExtraListForRemoval(RE::InventoryEntryData* entry, int count, bool isViewingContainer)
	{
		using func_t = decltype(&GetInventoryEntryExtraListForRemoval);
		REL::Relocation<func_t> func{ RELOCATION_ID(50948, 51825) };
		return func(entry, count, isViewingContainer);
	}
}
