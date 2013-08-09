#include "ItemList.h"
#include "GameFactory.h"
#include "Network.h"

#include <algorithm>

using namespace std;
using namespace RakNet;

#ifdef VAULTMP_DEBUG
DebugInput<ItemList> ItemList::debug;
#endif

ItemList::ItemList(NetworkID source) : source(source)
{
#ifdef VAULTSERVER
	if (!source)
	{
		SetNetworkIDManager(Network::Manager());
		this->source = GetNetworkID();
	}
#else
	if (!source)
		throw VaultException("Source NetworkID must not be null").stacktrace();
#endif
}

ItemList::~ItemList()
{
	this->FlushContainer();
}

NetworkID ItemList::FindStackableItem(unsigned int baseID, double condition) const
{
	for (const NetworkID& id : container)
		if (GameFactory::Operate<Item>(id, [baseID, condition](FactoryItem& item) {
			return item->GetBase() == baseID && Utils::DoubleCompare(item->GetItemCondition(), condition, CONDITION_EPS);
		}))
			return id;

	return 0;
}

NetworkID ItemList::AddItem(NetworkID id)
{
	auto data = GameFactory::Operate<Item>(id, [this, id](FactoryItem& item) {
		NetworkID container = item->GetItemContainer();

		if (container)
		{
			// Alternative code path if the item has been received over network
			if (container == this->source && find(this->container.begin(), this->container.end(), id) == this->container.end())
			{
				this->container.emplace_back(id);
				return make_pair(0u, 0.0);
			}

			throw VaultException("Item is already owned by container %llu", item->GetItemContainer()).stacktrace();
		}

		return make_pair(item->GetBase(), item->GetItemCondition());
	});

	if (!data.first)
		return id;

	NetworkID stackable = FindStackableItem(data.first, data.second);

	if (stackable)
	{
		auto data = GameFactory::Operate<Item>(id, [](FactoryItem& item) {
			auto data = make_pair(item->GetItemEquipped(), item->GetItemCount());
			GameFactory::DestroyInstance(item);
			return data;
		});

		GameFactory::Operate<Item>(stackable, [&data](FactoryItem& item) {
			if (data.first)
				item->SetItemEquipped(true);

			item->SetItemCount(item->GetItemCount() + data.second);
		});
	}
	else
	{
		GameFactory::Operate<Item>(id, [this](FactoryItem& item) {
			item->SetItemContainer(this->source);
		});

		container.emplace_back(id);
	}

	return stackable ? stackable : id;
}

ItemList::AddOp ItemList::AddItem(unsigned int baseID, unsigned int count, double condition, bool silent)
{
	AddOp result;

	if (!count)
		return result;

	NetworkID stackable = FindStackableItem(baseID, condition);

	if (stackable)
	{
		result.first = false;
		result.second = stackable;

		GameFactory::Operate<Item>(result.second, [count, silent](FactoryItem& item) {
			item->SetItemCount(item->GetItemCount() + count);
			item->SetItemSilent(silent);
		});
	}
	else
	{
		result.first = true;
		result.second = GameFactory::CreateInstance(ID_ITEM, baseID);

		GameFactory::Operate<Item>(result.second, [this, count, condition, silent](FactoryItem& item) {
			item->SetItemCount(count);
			item->SetItemCondition(condition);
			item->SetItemSilent(silent);
			item->SetItemContainer(this->source);
		});

		container.emplace_back(result.second);
	}

	return result;
}

void ItemList::RemoveItem(NetworkID id)
{
	auto it = find(container.begin(), container.end(), id);

	if (it == container.end())
		throw VaultException("Unknown Item with NetworkID %llu in ItemList", id).stacktrace();

	GameFactory::Operate<Item>(id, [](FactoryItem& item) {
		item->SetItemContainer(0);
	});

	container.erase(it);
}

ItemList::RemoveOp ItemList::RemoveItem(unsigned int baseID, unsigned int count, bool silent)
{
	RemoveOp result;
	unsigned int count_ = count;

	for (const NetworkID& id : container)
	{
		if (!count)
			break;
		else
			GameFactory::Operate<Item>(id, [&result, id, baseID, &count, silent](FactoryItem& item) {
				if (item->GetBase() != baseID)
					return;

				if (item->GetItemCount() > count)
				{
					item->SetItemCount(item->GetItemCount() - count);
					item->SetItemSilent(silent);
					get<2>(result) = id;
					count = 0;
				}
				else
				{
					get<1>(result).emplace_back(id);
					count -= item->GetItemCount();
					GameFactory::DestroyInstance(item);
				}
			});
	}

	const auto& deleted = get<1>(result);

	if (!deleted.empty())
		remove_if(container.begin(), container.end(), [&deleted](const NetworkID& item) { return find(deleted.begin(), deleted.end(), item) != deleted.end(); });

	get<0>(result) = count_ - count;

	return result;
}

ItemList::Impl ItemList::RemoveAllItems()
{
	for (const NetworkID& id : container)
		GameFactory::DestroyInstance(id);

	return move(container);
}

NetworkID ItemList::EquipItem(unsigned int baseID, bool silent, bool stick) const
{
	NetworkID result = 0;

	if (!IsEquipped(baseID))
	{
		for (const NetworkID& id : container)
			if (GameFactory::Operate<Item>(id, [id, baseID, silent, stick](FactoryItem& item) {
				if (item->GetBase() != baseID)
					return 0ull;

				item->SetItemEquipped(true);
				item->SetItemSilent(silent);
				item->SetItemStick(stick);
				return id;
			}))
				return id;
	}

	return result;
}

NetworkID ItemList::UnequipItem(unsigned int baseID, bool silent, bool stick) const
{
	NetworkID id = IsEquipped(baseID);

	if (id)
		GameFactory::Operate<Item>(id, [silent, stick](FactoryItem& item) {
			item->SetItemEquipped(false);
			item->SetItemSilent(silent);
			item->SetItemStick(stick);
		});

	return id;
}

NetworkID ItemList::IsEquipped(unsigned int baseID) const
{
	for (const NetworkID& id : container)
		if (GameFactory::Operate<Item>(id, [baseID](FactoryItem& item) {
			return item->GetBase() == baseID && item->GetItemEquipped();
		}))
			return id;

	return 0;
}

void ItemList::Copy(ItemList& IL) const
{
	IL.FlushContainer();

	for (const NetworkID& id : this->container)
	{
		FactoryItem item = GameFactory::GetObject<Item>(id).get();
		IL.AddItem(item->Copy());
	}
}

bool ItemList::IsEmpty() const
{
	return container.empty();
}

unsigned int ItemList::GetItemCount(unsigned int baseID) const
{
	unsigned int count = 0;

	for (const NetworkID& id : container)
	{
		FactoryItem item = GameFactory::GetObject<Item>(id).get();

		if (!baseID || item->GetBase() == baseID)
			count += item->GetItemCount();
	}

	return count;
}

void ItemList::FlushContainer()
{
	for (const NetworkID& id : container)
		GameFactory::DestroyInstance(id);

	container.clear();
}

const ItemList::Impl& ItemList::GetItemList() const
{
	return container;
}

#ifdef VAULTSERVER
ItemList::Impl ItemList::GetItemTypes(const string& type) const
{
	Impl result;

	for (const NetworkID& id : container)
	{
		FactoryItem item = GameFactory::GetObject<Item>(id).get();

		if (DB::Record::Lookup(item->GetBase(), type))
			result.emplace_back(id);
	}

	return result;
}
#endif
