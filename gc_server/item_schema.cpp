#include "stdafx.h"
#include "item_schema.hpp"
#include "keyvalue.hpp"
#include "gc_const_csgo.hpp" // mikkotodo remove?
#include "random.hpp"

// ideally this would get parsed from the item schema...
static uint32_t ItemRarityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> rarityNames[] =
        {
            {"default", ItemSchema::RarityDefault},
            {"common", ItemSchema::RarityCommon},
            {"uncommon", ItemSchema::RarityUncommon},
            {"rare", ItemSchema::RarityRare},
            {"mythical", ItemSchema::RarityMythical},
            {"legendary", ItemSchema::RarityLegendary},
            {"ancient", ItemSchema::RarityAncient},
            {"immortal", ItemSchema::RarityImmortal},
            {"unusual", ItemSchema::RarityUnusual},
        };

    for (const auto &pair : rarityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    // assert(false);
    return ItemSchema::RarityCommon;
}

AttributeInfo::AttributeInfo(const KeyValue &key)
{
    std::string_view type = key.GetString("attribute_type");

    // Debug print to see full text and type
    // logger::info("Attribute key/name: '%s'", std::string{ key.Name() }.c_str());
    // logger::info("Full KeyValue text: '%s'", std::string{ key.String() }.c_str());
    // logger::info("Attribute type: '%s'", std::string{ type }.c_str());

    if (type.size())
    {
        if (type == "float")
        {
            m_type = AttributeType::Float;
        }
        else if (type == "uint32")
        {
            m_type = AttributeType::Uint32;
        }
        else if (type == "string")
        {
            m_type = AttributeType::String;
        }
        else
        {
            // not supported, fall back to float
            logger::info("Unsupported attribute type %s", std::string{type}.c_str());
            m_type = AttributeType::Float;
        }
    }
    else
    {
        bool integer = key.GetNumber<int>("stored_as_integer");
        m_type = integer ? AttributeType::Uint32 : AttributeType::Float;
    }
}

ItemInfo::ItemInfo(uint32_t defIndex)
    : m_defIndex{defIndex}, m_rarity{ItemSchema::RarityCommon}, m_quality{ItemSchema::QualityUnique}, m_supplyCrateSeries{0}, m_tournamentEventId{0}
{
    // RecursiveParseItem parses the rest
}

PaintKitInfo::PaintKitInfo(const KeyValue &key)
    : m_defIndex{FromString<uint32_t>(key.Name())}, m_rarity{ItemSchema::RarityCommon} // rarity is not stored here, set it in ParsePaintKitRarities
{
    m_minFloat = key.GetNumber<float>("wear_remap_min", 0.0f);
    m_maxFloat = key.GetNumber<float>("wear_remap_max", 1.0f);

    // Get the name and description tag
    m_name = key.GetString("name");
    m_descriptionTag = key.GetString("description_tag");
}

StickerKitInfo::StickerKitInfo(const KeyValue &key)
    : m_defIndex{FromString<uint32_t>(key.Name())}, m_rarity{ItemSchema::RarityDefault} // mikkotodo revisit... currently using item rarity if this is default
{
    // Get the name and localization token
    m_name = key.GetString("name");
    m_itemName = key.GetString("item_name");

    std::string_view rarity = key.GetString("item_rarity");
    if (rarity.size())
    {
        m_rarity = ItemRarityFromString(rarity);
    }
}

MusicDefinitionInfo::MusicDefinitionInfo(const KeyValue &key)
    : m_defIndex{FromString<uint32_t>(key.Name())}
{
    assert(m_defIndex);

    // Get the name and localization token
    m_name = key.GetString("name");
    m_locName = key.GetString("loc_name");
}

ItemSchema::ItemSchema()
{
    KeyValue itemSchema{"root"};
    if (!itemSchema.ParseFromFile("items/items_game.txt"))
    {
        logger::info("Failed to load items_game.txt! OLLUM FIX IT");
        // assert(false);
        return;
    }

    const KeyValue *itemsGame = itemSchema.GetSubkey("items_game");
    if (!itemsGame)
    {
        // assert(false);
        return;
    }

    const KeyValue *itemsKey = itemsGame->GetSubkey("items");
    if (itemsKey)
    {
        ParseItems(itemsKey, itemsGame->GetSubkey("prefabs"));
    }

    const KeyValue *attributesKey = itemsGame->GetSubkey("attributes");
    if (attributesKey)
    {
        ParseAttributes(attributesKey);
    }

    const KeyValue *stickerKitsKey = itemsGame->GetSubkey("sticker_kits");
    if (stickerKitsKey)
    {
        ParseStickerKits(stickerKitsKey);
    }

    const KeyValue *paintKitsKey = itemsGame->GetSubkey("paint_kits");
    if (paintKitsKey)
    {
        ParsePaintKits(paintKitsKey);
    }

    const KeyValue *paintKitsRarityKey = itemsGame->GetSubkey("paint_kits_rarity");
    if (paintKitsRarityKey)
    {
        ParsePaintKitRarities(paintKitsRarityKey);
    }

    const KeyValue *musicDefinitionsKey = itemsGame->GetSubkey("music_definitions");
    if (musicDefinitionsKey)
    {
        ParseMusicDefinitions(musicDefinitionsKey);
    }

    // unusual loot lists are not included in client_loot_lists
    // we need to parse these after items and paint kits but before client_loot_lists
    {
        KeyValue unusualLootLists{"unusual_loot_lists"};

        if (unusualLootLists.ParseFromFile("items/unusual_loot_lists.txt"))
        {
            ParseLootLists(&unusualLootLists, true);
        }
        else
        {
            logger::info("Failed to load unusual_loot_lists.txt! OLLUM FIX IT");
        }
    }

    const KeyValue *lootListsKey = itemsGame->GetSubkey("client_loot_lists");
    if (lootListsKey)
    {
        ParseLootLists(lootListsKey, false);
    }

    const KeyValue *revolvingLootListsKey = itemsGame->GetSubkey("revolving_loot_lists");
    if (revolvingLootListsKey)
    {
        ParseRevolvingLootLists(revolvingLootListsKey);
    }
}

// AttributeType ItemSchema::AttributeType(uint32_t defIndex) const
//{
//     auto it = m_attributeInfo.find(defIndex);
//     if (it != m_attributeInfo.end())
//     {
//         return it->second.type;
//     }
//
//     //assert(false);
//     return AttributeType::Float;
// }

float ItemSchema::AttributeFloat(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return 0;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return *reinterpret_cast<const float *>(attribute->value_bytes().data());

    case AttributeType::Uint32:
        return *reinterpret_cast<const uint32_t *>(attribute->value_bytes().data());

    case AttributeType::String:
        return FromString<float>(attribute->value_bytes());

    default:
        // assert(false);
        return 0;
    }
}

uint32_t ItemSchema::AttributeUint32(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return 0;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return *reinterpret_cast<const float *>(attribute->value_bytes().data());

    case AttributeType::Uint32:
        return *reinterpret_cast<const uint32_t *>(attribute->value_bytes().data());

    case AttributeType::String:
        return FromString<uint32_t>(attribute->value_bytes());

    default:
        // assert(false);
        return 0;
    }
}

std::string ItemSchema::AttributeString(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return {};
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return std::to_string(*reinterpret_cast<const float *>(attribute->value_bytes().data()));

    case AttributeType::Uint32:
        return std::to_string(*reinterpret_cast<const uint32_t *>(attribute->value_bytes().data()));

    case AttributeType::String:
        return attribute->value_bytes();

    default:
        // assert(false);
        return {};
    }
}

// we dont wanna write anything to any file, but maybe to a database
bool ItemSchema::SetAttributeFloat(CSOEconItemAttribute *attribute, float value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        attribute->set_value_bytes(&value, sizeof(value));
        break;
    }

    case AttributeType::Uint32:
    {
        uint32_t convert = static_cast<uint32_t>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::String:
    {
        std::string convert = std::to_string(value);
        attribute->set_value_bytes(std::move(convert));
        break;
    }

    default:
        // assert(false);
        return false;
    }

    return true;
}

bool ItemSchema::SetAttributeUint32(CSOEconItemAttribute *attribute, uint32_t value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        float convert = static_cast<float>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::Uint32:
    {
        attribute->set_value_bytes(&value, sizeof(value));
        break;
    }

    case AttributeType::String:
    {
        std::string convert = std::to_string(value);
        attribute->set_value_bytes(std::move(convert));
        break;
    }

    default:
        // assert(false);
        return false;
    }

    return true;
}

bool ItemSchema::SetAttributeString(CSOEconItemAttribute *attribute, std::string_view value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        // assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        float convert = FromString<float>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::Uint32:
    {
        uint32_t convert = FromString<uint32_t>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::String:
    {
        attribute->set_value_bytes(value.data(), value.size());
        break;
    }

    default:
        // assert(false);
        return false;
    }

    return true;
}

bool ItemSchema::EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, GenerateStatTrak generateStatTrak)
{
    bool statTrak;

    switch (generateStatTrak)
    {
    case GenerateStatTrak::Yes:
        statTrak = true;
        break;

    case GenerateStatTrak::Maybe:
        statTrak = (g_random.Uint32(1, 10) == 1);
        break;

    default:
        statTrak = false;
        break;
    }

    // NOTE: unusual stattraks only valid below id 1000
    if (statTrak && lootListItem.quality == QualityUnusual && lootListItem.itemInfo->m_defIndex >= 1000)
    {
        statTrak = false;
    }

    uint32_t quality = QualityUnique;

    // but unusual overrides everything
    if (lootListItem.quality == QualityUnusual)
    {
        quality = QualityUnusual;
    }

    // StatTrak affects quality only if not unusual
    if (statTrak && quality != QualityUnusual)
    {
        quality = QualityStrange;
    }

    assert(lootListItem.rarity);

    item.set_inventory(InventoryUnacknowledged(UnacknowledgedFoundInCrate));
    item.set_def_index(lootListItem.itemInfo->m_defIndex);
    item.set_quantity(1);
    item.set_level(1); // mikkotodo parse from item
    item.set_quality(quality);
    item.set_flags(0);
    item.set_origin(kEconItemOrigin_FoundInCrate);
    item.set_in_use(false);
    item.set_rarity(lootListItem.rarity);

    if (lootListItem.type == LootListItemSticker)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemSpray)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);

        // add AttributeSpraysRemaining when it's unsealed (mikkotodo how does the real gc do this)

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeSprayTintId);
        SetAttributeUint32(attribute, g_random.Uint32(GraffitiTintMin, GraffitiTintMax));
    }
    else if (lootListItem.type == LootListItemPatch)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemMusicKit)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeMusicId);
        SetAttributeUint32(attribute, lootListItem.musicDefinitionInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemPaintable)
    {
        const PaintKitInfo *paintKitInfo = lootListItem.paintKitInfo;

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeTexturePrefab);
        SetAttributeUint32(attribute, paintKitInfo->m_defIndex);

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeTextureSeed);
        SetAttributeUint32(attribute, g_random.Uint32(0, 1000));

        // mikkotodo how does the float distribution work?
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeTextureWear);
        SetAttributeFloat(attribute, g_random.Float(paintKitInfo->m_minFloat, paintKitInfo->m_maxFloat));
    }
    else if (lootListItem.type == LootListItemNoAttribute)
    {
        // nothing
    }
    else
    {
        // assert(false);
    }

    if (statTrak)
    {
        assert((lootListItem.type == LootListItemMusicKit) || (lootListItem.type == LootListItemPaintable));

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeKillEater);
        SetAttributeUint32(attribute, 0);

        // mikkotodo fix magic
        int scoreType = (lootListItem.type == LootListItemMusicKit) ? 1 : 0;

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeKillEaterScoreType);
        SetAttributeUint32(attribute, scoreType);
    }

    return true;
}

// returns true if there are unusuals (stattrak able)
static bool GetLootListItems(const LootList &lootList, std::vector<const LootListItem *> &items)
{
    bool unusuals = lootList.isUnusual;

    for (const LootList *other : lootList.subLists)
    {
        unusuals |= GetLootListItems(*other, items);
    }

    for (const LootListItem &item : lootList.items)
    {
        items.push_back(&item);
    }

    return unusuals;
}

bool ItemSchema::SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item)
{
    auto itemSearch = m_itemInfo.find(crate.def_index());
    if (itemSearch == m_itemInfo.end())
    {
        // assert(false);
        return false;
    }

    assert(itemSearch->second.m_supplyCrateSeries);

    auto lootListSearch = m_revolvingLootLists.find(itemSearch->second.m_supplyCrateSeries);
    if (lootListSearch == m_revolvingLootLists.end())
    {
        // assert(false);
        return false;
    }

    const LootList &lootList = lootListSearch->second;
    assert(lootList.subLists.empty() != lootList.items.empty());

    std::vector<const LootListItem *> lootListItems;
    lootListItems.reserve(32); // overkill
    bool containsUnusuals = GetLootListItems(lootList, lootListItems);

    if (!lootListItems.size())
    {
        // assert(false);
        return false;
    }

    // handle stattrak
    GenerateStatTrak generateStatTrak = GenerateStatTrak::No;
    if (lootList.willProduceStatTrak)
    {
        generateStatTrak = GenerateStatTrak::Yes;
    }
    else if (containsUnusuals)
    {
        generateStatTrak = GenerateStatTrak::Maybe;
    }

    // group items by rarity and map rarities to base weights
    std::unordered_map<uint32_t, std::vector<const LootListItem *>> itemsByRarity;
    std::unordered_map<uint32_t, uint32_t> rarityWeights;
    uint32_t totalWeight = 0;

    // rarity weight map
    const std::unordered_map<uint32_t, uint32_t> baseWeights = {
        {RarityDefault, 15625}, // Consumer (Gray)
        {RarityCommon, 3125},   // Industrial (Light Blue)
        {RarityUncommon, 625},  // Mil-Spec (Blue)
        {RarityRare, 125},      // Restricted (Purple)
        {RarityMythical, 25},   // Classified (Pink)
        {RarityLegendary, 5}    // Covert (Red)
    };

    // group items by rarity and calculate total weight
    for (const auto *lootItem : lootListItems)
    {
        if (lootItem->quality != QualityUnusual)
        {
            // if we find an item of this rarity, add its base weight to total
            if (itemsByRarity[lootItem->rarity].empty() && baseWeights.count(lootItem->rarity))
            {
                rarityWeights[lootItem->rarity] = baseWeights.at(lootItem->rarity);
                totalWeight += baseWeights.at(lootItem->rarity);
            }
            itemsByRarity[lootItem->rarity].push_back(lootItem);
        }
    }

    // check for industrial grade items (for souvenir packages)
    bool hasIndustrialItems = false;
    if (itemsByRarity.count(RarityCommon) > 0 && !itemsByRarity[RarityCommon].empty())
    {
        hasIndustrialItems = true;
        logger::info("Found consumer grade items (%zu)", itemsByRarity[RarityCommon].size());
    }

    // check for golds
    if (!lootList.isUnusual && containsUnusuals)
    {
        std::vector<const LootListItem *> unusualItems;
        if (g_random.Uint32(0, totalWeight + 2) < 2) // 2 weight
        {
            for (const auto *lootItem : lootListItems)
            {
                if (lootItem->quality == QualityUnusual)
                {
                    unusualItems.push_back(lootItem);
                }
            }

            if (!unusualItems.empty())
            {
                size_t index = g_random.RandomIndex(unusualItems.size());
                return EconItemFromLootListItem(*unusualItems[index], item, generateStatTrak);
            }
        }
    }

    uint32_t roll = g_random.Uint32(0, totalWeight);
    uint32_t currentWeight = 0;

    for (const auto &[rarity, items] : itemsByRarity)
    {
        if (rarityWeights.count(rarity))
        {
            currentWeight += rarityWeights[rarity];
            if (roll < currentWeight)
            {
                // random item from this rarity
                size_t index = g_random.RandomIndex(items.size());
                bool result = EconItemFromLootListItem(*items[index], item, generateStatTrak);

                // check if we need to make this a souvenir item
                if (result && itemSearch->second.m_tournamentEventId != 0 && hasIndustrialItems)
                {
                    logger::info("Setting quality to Tournament");
                    item.set_quality(QualityTournament);

                    // apply stickers based on tournament event ID
                    ApplySouvenirStickers(item, itemSearch->second.m_tournamentEventId, items[index]->itemInfo);
                }

                return result;
            }
        }
    }

    // assert(false);
    return false;
}

TournamentStickers ItemSchema::GenerateTournamentStickers(uint32_t tournamentEventId, const ItemInfo *itemInfo) const
{
    TournamentStickers config = {0, 0, 0, 0};

    switch (tournamentEventId)
    {
    case 1: // DreamHack 2013
    {
        static const uint32_t commonStickers[] = {1, 3, 5, 7, 9, 11};
        static const uint32_t rareStickers[] = {2, 4, 6, 8, 10, 12};

        // 75% chance for common, 25% for rare
        bool useCommon = (g_random.Uint32(1, 100) <= 75);
        const uint32_t *stickerSet = useCommon ? commonStickers : rareStickers;
        uint32_t setSize = useCommon ? sizeof(commonStickers) / sizeof(commonStickers[0]) : sizeof(rareStickers) / sizeof(rareStickers[0]);

        // Select one sticker
        config.tournamentSticker = stickerSet[g_random.Uint32(0, setSize - 1)];

        // In this early tournament, only one sticker was applied
        return config;
    }

    // case 2 doesnt exist (?)

    case 3: // EMS One 2014
    {
        static const uint32_t teamStickers[] = {83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98};
        static const uint32_t tournamentStickers[] = {99, 100};

        // Choose two random team stickers
        size_t teamStickerCount = sizeof(teamStickers) / sizeof(teamStickers[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamStickerCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamStickerCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);
        }

        config.teamSticker1 = teamStickers[teamIndex1];
        config.teamSticker2 = teamStickers[teamIndex2];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 4: // ESL One Cologne 2014
    {
        static const uint32_t teamStickers[] = {156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171};
        static const uint32_t tournamentStickers[] = {172};

        // Choose two random team stickers
        size_t teamStickerCount = sizeof(teamStickers) / sizeof(teamStickers[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamStickerCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamStickerCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);
        }

        config.teamSticker1 = teamStickers[teamIndex1];
        config.teamSticker2 = teamStickers[teamIndex2];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 5: // DreamHack 2014
    {
        static const uint32_t teamStickers[] = {237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 257};
        static const uint32_t tournamentStickers[] = {231};

        // Choose two random team stickers
        size_t teamStickerCount = sizeof(teamStickers) / sizeof(teamStickers[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamStickerCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamStickerCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);
        }

        config.teamSticker1 = teamStickers[teamIndex1];
        config.teamSticker2 = teamStickers[teamIndex2];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 6: // ESL One Katowice 2015
    {
        static const uint32_t teamStickers[] = {289, 293, 297, 305, 309, 313, 317, 321, 325, 329, 333, 337, 341, 345, 349, 353};
        static const uint32_t tournamentStickers[] = {301};

        // Choose two random team stickers
        size_t teamStickerCount = sizeof(teamStickers) / sizeof(teamStickers[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamStickerCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamStickerCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);
        }

        config.teamSticker1 = teamStickers[teamIndex1];
        config.teamSticker2 = teamStickers[teamIndex2];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 7: // ESL One Cologne 2015
    {
        static const uint32_t teamStickers[] = {622, 625, 628, 631, 634, 637, 640, 643, 646, 649, 652, 655, 658, 661, 664, 667};
        static const uint32_t tournamentStickers[] = {670};

        // Choose two random team stickers
        size_t teamStickerCount = sizeof(teamStickers) / sizeof(teamStickers[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamStickerCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamStickerCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamStickerCount - 1);
        }

        config.teamSticker1 = teamStickers[teamIndex1];
        config.teamSticker2 = teamStickers[teamIndex2];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 8: // DreamHack Cluj-Napoca 2015
    {
        // Team sticker IDs
        static const uint32_t teams[] = {
            913, 916, 919, 922, 925, 928, 931, 934, 937, 940, 943, 946, 949, 952, 955, 958};

        // Player stickers for each team
        static const uint32_t playerStickers[16][5] = {
            {808, 811, 814, 817, 820}, // Ninjas in Pyjamas
            {823, 826, 829, 832, 835}, // Team Dignitas
            {673, 676, 679, 682, 685}, // Counter Logic Gaming
            {883, 886, 889, 892, 895}, // Vexed Gaming
            {718, 721, 724, 727, 730}, // Flipsid3 Tactics
            {838, 841, 844, 847, 850}, // Team Liquid
            {778, 781, 784, 787, 790}, // mousesports
            {793, 796, 799, 802, 805}, // Natus Vincere
            {898, 901, 904, 907, 910}, // Virtus.Pro
            {688, 691, 694, 697, 700}, // Cloud9
            {748, 751, 754, 757, 760}, // G2 Esports
            {853, 856, 859, 862, 865}, // Titan
            {868, 871, 874, 877, 880}, // Team SoloMid
            {703, 706, 709, 712, 715}, // Team EnVyUs
            {733, 736, 739, 742, 745}, // Fnatic
            {763, 766, 769, 772, 775}  // Luminosity Gaming
        };

        static const uint32_t tournamentStickers[] = {961};

        // Select two unique teams
        size_t teamCount = sizeof(teams) / sizeof(teams[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamCount - 1);
        }

        // Get team stickers
        config.teamSticker1 = teams[teamIndex1];
        config.teamSticker2 = teams[teamIndex2];

        // Select player sticker from one of the teams (50/50 chance)
        uint32_t playerTeamIndex = (g_random.Uint32(0, 1) == 0) ? teamIndex1 : teamIndex2;
        config.playerSticker = playerStickers[playerTeamIndex][g_random.Uint32(0, 4)];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    case 9: // MLG Columbus 2016
    {
        // Team sticker IDs
        static const uint32_t teams[] = {
            1010, 1014, 1018, 1022, 1026, 1030, 1034, 1038, 1042, 1046, 1050, 1054, 1058, 1062, 1066, 1070};

        // Player stickers for each team
        static const uint32_t playerStickers[16][5] = {
            {1212, 1215, 1218, 1221, 1224}, // Ninjas in Pyjamas
            {1227, 1230, 1233, 1236, 1239}, // Splyce
            {1077, 1080, 1083, 1086, 1089}, // Counter Logic Gaming
            {1287, 1290, 1293, 1296, 1299}, // Gambit Esports
            {1122, 1125, 1128, 1131, 1134}, // Flipsid3 Tactics
            {1242, 1245, 1248, 1251, 1254}, // Team Liquid
            {1182, 1185, 1188, 1191, 1194}, // mousesports
            {1197, 1200, 1203, 1206, 1209}, // Natus Vincere
            {1302, 1305, 1308, 1311, 1314}, // Virtus.Pro
            {1092, 1095, 1098, 1101, 1104}, // Cloud9
            {1257, 1260, 1263, 1266, 1269}, // G2 Esports
            {1152, 1155, 1158, 1161, 1164}, // FaZe Clan
            {1272, 1275, 1278, 1281, 1284}, // Astralis
            {1107, 1110, 1113, 1116, 1119}, // Team EnVyUs
            {1137, 1140, 1143, 1146, 1149}, // Fnatic
            {1167, 1170, 1173, 1176, 1179}  // Luminosity Gaming
        };

        static const uint32_t tournamentStickers[] = {1074};

        // Select two unique teams
        size_t teamCount = sizeof(teams) / sizeof(teams[0]);
        uint32_t teamIndex1 = g_random.Uint32(0, teamCount - 1);
        uint32_t teamIndex2 = g_random.Uint32(0, teamCount - 1);

        // Make sure they're different
        while (teamIndex1 == teamIndex2 && teamCount > 1)
        {
            teamIndex2 = g_random.Uint32(0, teamCount - 1);
        }

        // Get team stickers
        config.teamSticker1 = teams[teamIndex1];
        config.teamSticker2 = teams[teamIndex2];

        // Select player sticker from one of the teams (50/50 chance)
        uint32_t playerTeamIndex = (g_random.Uint32(0, 1) == 0) ? teamIndex1 : teamIndex2;
        config.playerSticker = playerStickers[playerTeamIndex][g_random.Uint32(0, 4)];

        // Choose tournament sticker
        config.tournamentSticker = tournamentStickers[g_random.Uint32(0, sizeof(tournamentStickers) / sizeof(tournamentStickers[0]) - 1)];

        return config;
    }

    default:
        logger::info("Unknown tournament ID: %d", tournamentEventId);
        break;
    }

    return config;
}

void ItemSchema::ApplySouvenirStickers(CSOEconItem &item, uint32_t tournamentEventId, const ItemInfo *itemInfo) const
{
    if (tournamentEventId == 0)
    {
        return;
    }

    // Get weapon name for max sticker slot determination
    std::string_view weaponName = itemInfo->m_name;

    // Most weapons have 4 sticker slots, R8 Revolver and G3SG1 have 5
    bool hasExtraSlot = (weaponName == "weapon_revolver" || weaponName == "weapon_g3sg1");
    uint32_t maxStickerSlots = hasExtraSlot ? 5 : 4;

    // Generate tournament stickers
    TournamentStickers stickerConfig = GenerateTournamentStickers(tournamentEventId, itemInfo);

    // If we have no stickers to apply, return
    if (stickerConfig.teamSticker1 == 0 && stickerConfig.teamSticker2 == 0 &&
        stickerConfig.playerSticker == 0 && stickerConfig.tournamentSticker == 0)
    {
        return;
    }

    // Create a list of available slots
    std::vector<uint32_t> availableSlots;
    for (uint32_t i = 0; i < maxStickerSlots; ++i)
    {
        availableSlots.push_back(i);
    }

    // Shuffle the available slots
    for (size_t i = availableSlots.size() - 1; i > 0; --i)
    {
        size_t j = g_random.Uint32(0, i);
        std::swap(availableSlots[i], availableSlots[j]);
    }

    // Apply stickers in shuffled order
    size_t slotIndex = 0;

    // Apply team sticker 1 if available
    if (stickerConfig.teamSticker1 != 0 && slotIndex < availableSlots.size())
    {
        uint32_t slot = availableSlots[slotIndex++];

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0 + (slot * 4));
        SetAttributeUint32(attribute, stickerConfig.teamSticker1);

        // Set wear to 0 (factory new)
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerWear0 + (slot * 4));
        SetAttributeFloat(attribute, 0.0f);
    }

    // Apply team sticker 2 if available
    if (stickerConfig.teamSticker2 != 0 && slotIndex < availableSlots.size())
    {
        uint32_t slot = availableSlots[slotIndex++];

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0 + (slot * 4));
        SetAttributeUint32(attribute, stickerConfig.teamSticker2);

        // Set wear to 0 (factory new)
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerWear0 + (slot * 4));
        SetAttributeFloat(attribute, 0.0f);
    }

    // Apply player sticker if available
    if (stickerConfig.playerSticker != 0 && slotIndex < availableSlots.size())
    {
        uint32_t slot = availableSlots[slotIndex++];

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0 + (slot * 4));
        SetAttributeUint32(attribute, stickerConfig.playerSticker);

        // Set wear to 0 (factory new)
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerWear0 + (slot * 4));
        SetAttributeFloat(attribute, 0.0f);
    }

    // Apply tournament sticker if available
    if (stickerConfig.tournamentSticker != 0 && slotIndex < availableSlots.size())
    {
        uint32_t slot = availableSlots[slotIndex++];

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0 + (slot * 4));
        SetAttributeUint32(attribute, stickerConfig.tournamentSticker);

        // Set wear to 0 (factory new)
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerWear0 + (slot * 4));
        SetAttributeFloat(attribute, 0.0f);
    }
}

void ItemSchema::ParseItems(const KeyValue *itemsKey, const KeyValue *prefabsKey)
{
    m_itemInfo.reserve(itemsKey->SubkeyCount());

    for (const KeyValue &itemKey : *itemsKey)
    {
        if (itemKey.Name() == "default")
        {
            // ignore this
            continue;
        }

        uint32_t defIndex = FromString<uint32_t>(itemKey.Name());
        auto emplace = m_itemInfo.try_emplace(defIndex, defIndex);

        ParseItemRecursive(emplace.first->second, itemKey, prefabsKey);
    }
}

// ideally this would get parsed from the item schema...
static uint32_t ItemQualityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> qualityNames[] =
        {
            {"normal", ItemSchema::QualityNormal},
            {"genuine", ItemSchema::QualityGenuine},
            {"vintage", ItemSchema::QualityVintage},
            {"unusual", ItemSchema::QualityUnusual},
            {"unique", ItemSchema::QualityUnique},
            {"community", ItemSchema::QualityCommunity},
            {"developer", ItemSchema::QualityDeveloper},
            {"selfmade", ItemSchema::QualitySelfmade},
            {"customized", ItemSchema::QualityCustomized},
            {"strange", ItemSchema::QualityStrange},
            {"completed", ItemSchema::QualityCompleted},
            {"haunted", ItemSchema::QualityHaunted},
            {"tournament", ItemSchema::QualityTournament},
        };

    for (const auto &pair : qualityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    // assert(false);
    return ItemSchema::QualityUnique; // i guess???
}

void ItemSchema::ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey)
{
    // Process prefabs first so they can be overridden by the current item
    std::string_view prefabName = itemKey.GetString("prefab");
    if (prefabName.size() && prefabsKey)
    {
        const KeyValue *prefabKey = prefabsKey->GetSubkey(prefabName);
        if (prefabKey)
        {
            ParseItemRecursive(info, *prefabKey, prefabsKey);
        }
    }

    std::string_view name = itemKey.GetString("name");
    if (name.size())
    {
        info.m_name = name;
    }

    std::string_view item_name = itemKey.GetString("item_name");
    if (item_name.size())
    {
        info.m_itemName = item_name;
    }

    std::string_view quality = itemKey.GetString("item_quality");
    if (quality.size())
    {
        info.m_quality = ItemQualityFromString(quality);
    }

    std::string_view rarity = itemKey.GetString("item_rarity");
    if (rarity.size())
    {
        info.m_rarity = ItemRarityFromString(rarity);
    }

    const KeyValue *attributes = itemKey.GetSubkey("attributes");
    if (attributes)
    {
        const KeyValue *supplyCrateSeries = attributes->GetSubkey("set supply crate series");
        if (supplyCrateSeries)
        {
            info.m_supplyCrateSeries = supplyCrateSeries->GetNumber<uint32_t>("value");
        }

        const KeyValue *tournamentEventId = attributes->GetSubkey("tournament event id");
        if (tournamentEventId)
        {
            info.m_tournamentEventId = tournamentEventId->GetNumber<uint32_t>("value");
        }
    }
}

void ItemSchema::ParseAttributes(const KeyValue *attributesKey)
{
    m_attributeInfo.reserve(attributesKey->SubkeyCount());

    for (const KeyValue &attributeKey : *attributesKey)
    {
        uint32_t defIndex = FromString<uint32_t>(attributeKey.Name());
        assert(defIndex);
        m_attributeInfo.try_emplace(defIndex, attributeKey);
    }
}

void ItemSchema::ParseStickerKits(const KeyValue *stickerKitsKey)
{
    m_stickerKitInfo.reserve(stickerKitsKey->SubkeyCount());

    for (const KeyValue &stickerKitKey : *stickerKitsKey)
    {
        std::string_view name = stickerKitKey.GetString("name");

        auto pair = m_stickerKitInfo.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(name),
                                             std::forward_as_tuple(stickerKitKey));

        // save name and description
        pair.first->second.m_name = name;
        std::string_view descriptionTag = stickerKitKey.GetString("description_tag");
        if (descriptionTag.size())
        {
            pair.first->second.m_descriptionTag = descriptionTag;
        }
    }
}

void ItemSchema::ParsePaintKits(const KeyValue *paintKitsKey)
{
    m_paintKitInfo.reserve(paintKitsKey->SubkeyCount());

    for (const KeyValue &paintKitKey : *paintKitsKey)
    {
        std::string_view name = paintKitKey.GetString("name");

        auto pair = m_paintKitInfo.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(name),
                                           std::forward_as_tuple(paintKitKey));

        // save name and description
        pair.first->second.m_name = name;
        std::string_view descriptionTag = paintKitKey.GetString("description_tag");
        if (descriptionTag.size())
        {
            pair.first->second.m_descriptionTag = descriptionTag;
        }
    }
}

void ItemSchema::ParsePaintKitRarities(const KeyValue *raritiesKey)
{
    for (const KeyValue &key : *raritiesKey)
    {
        PaintKitInfo *paintKitInfo = PaintKitInfoByName(key.Name());
        if (!paintKitInfo)
        {
            ////assert(false);
            // logger::info("No such paint kit '%s'!!!", std::string{ key.Name() }.c_str());
            continue;
        }

        assert(paintKitInfo->m_rarity == RarityCommon);
        paintKitInfo->m_rarity = ItemRarityFromString(key.String());
    }
}

void ItemSchema::ParseMusicDefinitions(const KeyValue *musicDefinitionsKey)
{
    m_musicDefinitionInfo.reserve(musicDefinitionsKey->SubkeyCount());

    for (const KeyValue &musicDefinitionKey : *musicDefinitionsKey)
    {
        std::string_view name = musicDefinitionKey.GetString("name");

        auto pair = m_musicDefinitionInfo.emplace(std::piecewise_construct,
                                                  std::forward_as_tuple(name),
                                                  std::forward_as_tuple(musicDefinitionKey));

        // save name and name tag
        pair.first->second.m_name = name;
        std::string_view nameTag = musicDefinitionKey.GetString("loc_name");
        if (nameTag.size())
        {
            pair.first->second.m_nameTag = nameTag;
        }
    }
}

static void ParseAttributeAndItemName(std::string_view input, std::string_view &attribute, std::string_view &item)
{
    // fallbacks (mikkotodo unfuck)
    attribute = {};
    item = input;

    if (input[0] != '[')
        return;

    size_t attribEnd = input.find(']', 1);
    if (attribEnd == std::string_view::npos)
    {
        // assert(false);
        return;
    }

    attribute = input.substr(1, attribEnd - 1);
    item = input.substr(attribEnd + 1);

    assert(attribute.size() && item.size());
}

static LootListItemType LootListItemTypeFromName(std::string_view name, std::string_view attributeName)
{
    if (attributeName.empty())
    {
        return LootListItemNoAttribute;
    }

    const std::pair<std::string_view, LootListItemType> mapNames[] =
        {
            {"sticker", LootListItemSticker},
            {"spray", LootListItemSpray},
            {"patch", LootListItemPatch},
            {"musickit", LootListItemMusicKit}};

    for (const auto &pair : mapNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    return LootListItemPaintable;
}

void ItemSchema::ParseLootLists(const KeyValue *lootListsKey, bool parentIsUnusual)
{
    m_lootLists.reserve(lootListsKey->SubkeyCount());

    for (const KeyValue &lootListKey : *lootListsKey)
    {
        std::string_view listName = lootListKey.Name();

        // check if this list should be treated as unusual
        bool isUnusual = parentIsUnusual && (listName.find("unusual") != std::string_view::npos);

        auto emplace = m_lootLists.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(lootListKey.Name()),
                                           std::forward_as_tuple());

        LootList &lootList = emplace.first->second;
        lootList.isUnusual = isUnusual; // only set unusual if parent is unusual AND name contains "unusual"

        for (const KeyValue &entryKey : lootListKey)
        {
            std::string_view entryName = entryKey.Name();

            // check for options that we ignore
            if (entryName == "will_produce_stattrak")
            {
                lootList.willProduceStatTrak = true;
                continue;
            }

            // check for options that we ignore
            if (entryName == "all_entries_as_additional_drops" || entryName == "contains_patches_representing_organizations" || entryName == "contains_stickers_autographed_by_proplayers" || entryName == "contains_stickers_representing_organizations" || entryName == "limit_description_to_number_rnd" || entryName == "public_list_contents")
            {
                continue;
            }

            std::string entryNameKey{entryKey.Name()};

            // check if it's another loot list
            auto listSearch = m_lootLists.find(entryNameKey);
            if (listSearch != m_lootLists.end())
            {
                lootList.subLists.push_back(&listSearch->second);
                continue;
            }

            // check for an item
            LootListItem item;
            if (ParseLootListItem(item, entryName))
            {
                if (isUnusual)
                {
                    // override the quality here...
                    item.quality = QualityUnusual;
                }

                lootList.items.push_back(item);
            }
            else
            {
                // what the fuck is this...
                logger::info("Unhandled loot list entry %s!!!!", entryNameKey.c_str());
            }
        }
    }
}

static uint32_t PaintedItemRarity(uint32_t itemRarity, uint32_t paintKitRarity)
{
    int rarity = (itemRarity - 1) + paintKitRarity;
    if (rarity < 0)
    {
        return 0;
    }

    if (rarity > ItemSchema::RarityAncient)
    {
        if (paintKitRarity == ItemSchema::RarityImmortal)
        {
            return ItemSchema::RarityImmortal;
        }

        return ItemSchema::RarityAncient;
    }

    return rarity;
}

// mikkotodo rewrite this function
bool ItemSchema::ParseLootListItem(LootListItem &item, std::string_view name)
{
    // check for an attribute + item combo
    std::string_view attributeName, itemName;
    ParseAttributeAndItemName(name, attributeName, itemName);

    const ItemInfo *itemInfo = ItemInfoByName(itemName);
    if (!itemInfo)
    {
        logger::info("No such item %s!!!", std::string{itemName}.c_str());
        return false;
    }

    item.itemInfo = itemInfo;
    item.type = LootListItemTypeFromName(itemName, attributeName);

    // until proven otherwise...
    item.rarity = itemInfo->m_rarity;
    item.quality = itemInfo->m_quality;

    if (item.type == LootListItemNoAttribute)
    {
        // no attribute
    }
    else if (item.type == LootListItemSticker || item.type == LootListItemSpray || item.type == LootListItemPatch)
    {
        // the attribute is the sticker name
        item.stickerKitInfo = StickerKitInfoByName(attributeName);
        if (!item.stickerKitInfo)
        {
            logger::info("WARNING: No such sticker kit %s", std::string{attributeName}.c_str());
            return false;
        }

        // sticker kits affect the item rarity (mikkotodo how do these work, something like PaintedItemRarity???)
        assert(itemInfo->m_rarity == 1);

        if (item.stickerKitInfo->m_rarity)
        {
            item.rarity = item.stickerKitInfo->m_rarity;
        }
    }
    else if (item.type == LootListItemMusicKit)
    {
        // the attribute is the music definition name
        item.musicDefinitionInfo = MusicDefinitionInfoByName(attributeName);
        if (!item.musicDefinitionInfo)
        {
            logger::info("WARNING: No such music definition %s", std::string{attributeName}.c_str());
            return false;
        }
    }
    else
    {
        // probably a paint kit
        assert(item.type == LootListItemPaintable);
        item.paintKitInfo = PaintKitInfoByName(attributeName);
        if (!item.paintKitInfo)
        {
            // assert(false);
            logger::info("WARNING: No such paint kit %s", std::string{attributeName}.c_str());
            return false;
        }

        // paint kits affect the item rarity
        item.rarity = PaintedItemRarity(itemInfo->m_rarity, item.paintKitInfo->m_rarity);
    }

    return true;
}

void ItemSchema::ParseRevolvingLootLists(const KeyValue *revolvingLootListsKey)
{
    m_revolvingLootLists.reserve(revolvingLootListsKey->SubkeyCount());

    for (const KeyValue &revolvingLootListKey : *revolvingLootListsKey)
    {
        uint32_t index = FromString<uint32_t>(revolvingLootListKey.Name());
        assert(index);

        // ugh
        std::string lootListName = std::string{revolvingLootListKey.String()};

        auto it = m_lootLists.find(lootListName);
        if (it == m_lootLists.end())
        {
            // logger::info("Ignoring revolving loot list %s", lootListName.c_str());
            continue;
        }

        m_revolvingLootLists.try_emplace(index, it->second);
    }
}

ItemInfo *ItemSchema::ItemInfoByName(std::string_view name)
{
    for (auto &pair : m_itemInfo)
    {
        const ItemInfo &info = pair.second;
        if (info.m_name == name)
        {
            return &pair.second;
        }
    }

    // assert(false);
    return nullptr;
}

StickerKitInfo *ItemSchema::StickerKitInfoByName(std::string_view name)
{
    auto it = m_stickerKitInfo.find(std::string{name});
    if (it == m_stickerKitInfo.end())
    {
        // assert(false);
        return nullptr;
    }

    return &it->second;
}

PaintKitInfo *ItemSchema::PaintKitInfoByName(std::string_view name)
{
    auto it = m_paintKitInfo.find(std::string{name});
    if (it == m_paintKitInfo.end())
    {
        ////assert(false);
        return nullptr;
    }

    return &it->second;
}

MusicDefinitionInfo *ItemSchema::MusicDefinitionInfoByName(std::string_view name)
{
    auto it = m_musicDefinitionInfo.find(std::string{name});
    if (it == m_musicDefinitionInfo.end())
    {
        // assert(false);
        return nullptr;
    }

    return &it->second;
}
