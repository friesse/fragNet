#pragma once

#include "econ_gcmessages.pb.h"
#include "base_gcmessages.pb.h"
#include "gcsdk_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"
#include "keyvalue_english.hpp"

class KeyValue;

struct TournamentStickers {
    uint32_t teamSticker1;
    uint32_t teamSticker2;
    uint32_t playerSticker;
    uint32_t tournamentSticker;
};

enum class AttributeType
{
    Float,
    Uint32,
    String
};

class AttributeInfo
{
public:
    AttributeInfo(const KeyValue &key);

    AttributeType m_type;
};

class ItemInfo
{
public:
    ItemInfo(uint32_t defIndex);

    uint32_t m_defIndex;
    std::string m_name;           // Internal name (e.g., "weapon_ssg08")
    std::string m_itemName;       // Localization token (e.g., "#SFUI_WPNHUD_SSG08")
    uint32_t m_rarity;
    uint32_t m_quality;
    uint32_t m_supplyCrateSeries; // cases only
    uint32_t m_tournamentEventId; // souvenirs only

    // Get the localized display name
    std::string_view GetDisplayName() const
    {
        // If we have a localization token, use it
        if (!m_itemName.empty())
        {
            std::string_view localized = LocalizeToken(m_itemName);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        // Fallback to internal name
        return m_name;
    }
};

class PaintKitInfo
{
public:
    PaintKitInfo(const KeyValue &key);

    uint32_t m_defIndex;
    uint32_t m_rarity;
    float m_minFloat;
    float m_maxFloat;
    std::string m_name;              // Internal name (e.g., "hy_tiger")
    std::string m_descriptionTag;    // Localization token (e.g., "#PaintKit_hy_tiger_Tag")
    
    // Get the localized display name
    std::string_view GetDisplayName() const
    {
        if (!m_descriptionTag.empty())
        {
            std::string_view localized = LocalizeToken(m_descriptionTag);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        return m_name;
    }
};

class StickerKitInfo
{
public:
    StickerKitInfo(const KeyValue &key);

    uint32_t m_defIndex;
    uint32_t m_rarity;
    std::string m_name;              // Internal name (e.g., "cc_sig_unner_gold")
    std::string m_itemName;          // Localization token (e.g., "#StickerKit_cc_sig_unner_gold")
    std::string m_descriptionTag;    // For compatibility with the existing code
    
    // Get the localized display name
    std::string_view GetDisplayName() const
    {
        if (!m_itemName.empty())
        {
            std::string_view localized = LocalizeToken(m_itemName);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        if (!m_descriptionTag.empty())
        {
            std::string_view localized = LocalizeToken(m_descriptionTag);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        return m_name;
    }
};

class MusicDefinitionInfo
{
public:
    MusicDefinitionInfo(const KeyValue &key);

    uint32_t m_defIndex;
    std::string m_name;              // Internal name (e.g., "bladee_01")
    std::string m_locName;           // Localization token (e.g., "#musickit_bladee_01")
    std::string m_nameTag;           // For compatibility with the existing code
    
    // Get the localized display name
    std::string_view GetDisplayName() const
    {
        if (!m_locName.empty())
        {
            std::string_view localized = LocalizeToken(m_locName);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        if (!m_nameTag.empty())
        {
            std::string_view localized = LocalizeToken(m_nameTag);
            if (!localized.empty())
            {
                return localized;
            }
        }
        
        return m_name;
    }
};

enum LootListItemType
{
    LootListItemNoAttribute,
    LootListItemPaintable,
    LootListItemSticker,
    LootListItemSpray,
    LootListItemPatch,
    LootListItemMusicKit,
};

struct LootListItem
{
    const ItemInfo *itemInfo{};
    LootListItemType type{ LootListItemNoAttribute };

    // these could be sticked into a variant to save a grand total of few bytes
    const PaintKitInfo *paintKitInfo{};
    const StickerKitInfo *stickerKitInfo{};
    const MusicDefinitionInfo *musicDefinitionInfo{};

    // might differ from those specified in itemInfo
    // (based on paint kits, stattrak etc.)
    uint32_t rarity{};
    uint32_t quality{};
};

struct LootList
{
    // we either have items or sublists, never both
    std::vector<LootListItem> items;
    std::vector<const LootList *> subLists;
    bool willProduceStatTrak{};
    bool isUnusual{};
};

// mikkotodo unfuck
enum class GenerateStatTrak
{
    No,
    Yes,
    Maybe
};

class ItemSchema
{
public:
    ItemSchema();

    float AttributeFloat(const CSOEconItemAttribute *attribute) const;
    uint32_t AttributeUint32(const CSOEconItemAttribute *attribute) const;
    std::string AttributeString(const CSOEconItemAttribute *attribute) const;

    bool SetAttributeFloat(CSOEconItemAttribute *attribute, float value) const;
    bool SetAttributeUint32(CSOEconItemAttribute *attribute, uint32_t value) const;
    bool SetAttributeString(CSOEconItemAttribute *attribute, std::string_view value) const;

    // case opening
    bool SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item);

public:
    // these could be parsed from the item schema but reduce code complexity by hardcoding them
    enum Rarity
    {
        RarityDefault = 0,
        RarityCommon = 1,
        RarityUncommon = 2,
        RarityRare = 3,
        RarityMythical = 4,
        RarityLegendary = 5,
        RarityAncient = 6,
        RarityImmortal = 7,

        RarityUnusual = 99
    };

    enum Quality
    {
        QualityNormal = 0,
        QualityGenuine = 1,
        QualityVintage = 2,
        QualityUnusual = 3,
        QualityUnique = 4,
        QualityCommunity = 5,
        QualityDeveloper = 6,
        QualitySelfmade = 7,
        QualityCustomized = 8,
        QualityStrange = 9,
        QualityCompleted = 10,
        QualityHaunted = 11,
        QualityTournament = 12
    };

    enum GraffitiTint
    {
        GraffitiTintMin = 1,
        GraffitiTintMax = 19
    };

    enum LoadoutSlot
    {
        LoadoutSlotGraffiti = 56
    };

    enum Item
    {
        ItemSpray = 1348,
        ItemSprayPaint = 1349,
        ItemPatch = 4609
    };

    enum Attribute
    {
        AttributeTexturePrefab = 6,
        AttributeTextureSeed = 7,
        AttributeTextureWear = 8,
        AttributeKillEater = 80,
        AttributeKillEaterScoreType = 81,

        AttributeCustomName = 111,

        // ugh
        AttributeStickerId0 = 113,
        AttributeStickerWear0 = 114,
        AttributeStickerScale0 = 115,
        AttributeStickerRotation0 = 116,
        AttributeStickerId1 = 117,
        AttributeStickerWear1 = 118,
        AttributeStickerScale1 = 119,
        AttributeStickerRotation1 = 120,
        AttributeStickerId2 = 121,
        AttributeStickerWear2 = 122,
        AttributeStickerScale2 = 123,
        AttributeStickerRotation2 = 124,
        AttributeStickerId3 = 125,
        AttributeStickerWear3 = 126,
        AttributeStickerScale3 = 127,
        AttributeStickerRotation3 = 128,
        AttributeStickerId4 = 129,
        AttributeStickerWear4 = 130,
        AttributeStickerScale4 = 131,
        AttributeStickerRotation4 = 132,
        AttributeStickerId5 = 133,
        AttributeStickerWear5 = 134,
        AttributeStickerScale5 = 135,
        AttributeStickerRotation5 = 136,

        AttributeMusicId = 166,
        AttributeQuestId = 168,

        AttributeSpraysRemaining = 232,
        AttributeSprayTintId = 233,
    };


private:
    void ParseItems(const KeyValue *itemsKey, const KeyValue *prefabsKey);
    void ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey);
    void ParseAttributes(const KeyValue *attributesKey);
    void ParseStickerKits(const KeyValue *stickerKitsKey);
    void ParsePaintKits(const KeyValue *paintKitsKey);
    void ParsePaintKitRarities(const KeyValue *raritiesKey);
    void ParseMusicDefinitions(const KeyValue *musicDefinitionsKey);
    void ParseLootLists(const KeyValue *lootListsKey, bool unusual);
    void ParseRevolvingLootLists(const KeyValue *revolvingLootListsKey);

    bool ParseLootListItem(LootListItem &item, std::string_view name);

    // internal slop
    ItemInfo *ItemInfoByName(std::string_view name);
    StickerKitInfo *StickerKitInfoByName(std::string_view name);
    PaintKitInfo *PaintKitInfoByName(std::string_view name);
    MusicDefinitionInfo *MusicDefinitionInfoByName(std::string_view name);

    // case opening
    bool EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, GenerateStatTrak statTrak);

    // tournament stickers
    TournamentStickers GenerateTournamentStickers(uint32_t tournamentEventId, const ItemInfo* itemInfo) const;
    void ApplySouvenirStickers(CSOEconItem &item, uint32_t tournamentEventId, const ItemInfo* itemInfo) const;

public:
    std::unordered_map<uint32_t, ItemInfo> m_itemInfo;
    std::unordered_map<uint32_t, AttributeInfo> m_attributeInfo;

    std::unordered_map<std::string, StickerKitInfo> m_stickerKitInfo;
    std::unordered_map<std::string, PaintKitInfo> m_paintKitInfo;
    std::unordered_map<std::string, MusicDefinitionInfo> m_musicDefinitionInfo;
    std::unordered_map<std::string, LootList> m_lootLists;

    std::unordered_map<uint32_t, const LootList &> m_revolvingLootLists;
};
