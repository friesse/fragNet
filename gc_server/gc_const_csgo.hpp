#pragma once

// csgo specific gc constants

// hardcoded gscookieid
constexpr uint64_t GameServerCookieId = 0x293A206F6C6C6548;

// if an item id has the high 4 bits set, this is actually a def index
// and paint kit index in a trenchcoat instead of a valid item id!
// lowest 16 bits = def index
// next lowest 16 bits = paint kit index
constexpr uint64_t ItemIdDefaultItemMask = 0xfull << 60;

// technically 5 but there are attributes for 6 stickers...
constexpr int MaxStickers = 6;

enum RankType : uint32_t
{
    RankTypeCompetitive = 6,
    RankTypeWingman = 7,
    RankTypeDangerZone = 10
};

enum RankId : uint32_t
{
    RankNone,
    RankSilver1,
    RankSilver2,
    RankSilver3,
    RankSilver4,
    RankSilverElite,
    RankSilverEliteMaster,
    RankGoldNova1,
    RankGoldNova2,
    RankGoldNova3,
    RankGoldNovaMaster,
    RankMasterGuardian1,
    RankMasterGuardian2,
    RankMasterGuardianElite,
    RankDistinguishedMasterGuardian,
    RankLegendaryEagle,
    RankLegendaryEagleMaster,
    RankSupremeMasterFirstClass,
    RankGlobalElite
};

// shared object type (type_id field in CMsgSOCacheSubscribed_SubscribedType)
enum SOTypeId : uint32_t
{
    SOTypeItem = 1,
    SOTypePersonaDataPublic = 2,
    SOTypeEquipSlot = 3,

    SOTypeItemRecipe = 5,

    SOTypeGameAccountClient = 7,
    SOTypeGameAccount = 8,

    SOTypeItemDropRateBonus = 38,

    SOTypeAccountSeasonalOperation1 = 40,
    SOTypeAccountSeasonalOperation2 = 41,

    SOTypeDefaultEquippedDefinitionInstance = 42,
    SOTypeDefaultEquippedDefinitionInstanceClient = 43,

    SOTypeCoupon = 45,
    SOTypeQuestProgress = 46,
};

// CSOEconItem origin
enum kEconItemOrigin
{
    kEconItemOrigin_Invalid = -1,
    kEconItemOrigin_Drop = 0,
    kEconItemOrigin_Achievement = 1,
    kEconItemOrigin_Purchased = 2,
    kEconItemOrigin_Traded = 3,
    kEconItemOrigin_Crafted = 4,
    kEconItemOrigin_StorePromotion = 5,
    kEconItemOrigin_Gifted = 6,
    kEconItemOrigin_SupportGranted = 7,
    kEconItemOrigin_FoundInCrate = 8, // case opening
    kEconItemOrigin_Earned = 9,
    kEconItemOrigin_ThirdPartyPromotion = 10,
    kEconItemOrigin_GiftWrapped = 11,
    kEconItemOrigin_HalloweenDrop = 12,
    kEconItemOrigin_PackageItem = 13,
    kEconItemOrigin_Foreign = 14,
    kEconItemOrigin_CDKey = 15,
    kEconItemOrigin_CollectionReward = 16,
    kEconItemOrigin_PreviewItem = 17,
    kEconItemOrigin_SteamWorkshopContribution = 18,
    kEconItemOrigin_PeriodicScoreReward = 19,
    kEconItemOrigin_MvMMissionCompletionReward = 20,
    kEconItemOrigin_MvMSquadSurplusReward = 21,
    kEconItemOrigin_RecipeOutput = 22,
    kEconItemOrigin_QuestDrop = 23,
    kEconItemOrigin_QuestLoanerItem = 24,
    kEconItemOrigin_TradeUp = 25,
    kEconItemOrigin_ViralCompetitiveBetaPassSpread = 26,
    kEconItemOrigin_CYOABloodMoneyPurchase = 27,
    kEconItemOrigin_Paintkit = 28,
    kEconItemOrigin_UntradableFreeContractReward = 29,
    kEconItemOrigin_Max = 30
};

enum ElevatedState : uint32_t
{
    ElevatedStateNo = 0,
    ElevatedStatePrime = 5
};

// dumped from client.dll strings, most of these aren't used by us
enum UnacknowledgedType
{
    UnacknowledgedFound = 1,
    UnacknowledgedCrafted,
    UnacknowledgedTraded,
    UnacknowledgedUnused1,
    UnacknowledgedFoundInCrate,
    UnacknowledgedGifted,
    UnacknowledgedUnused2,
    UnacknowledgedUnused3,
    UnacknowledgedEarned,
    UnacknowledgedRefunded,
    UnacknowledgedGiftWrapped,
    UnacknowledgedForeign,
    UnacknowledgedCollectionReward,
    UnacknowledgedPreviewItem,
    UnacknowledgedPreviewItemPurchased,
    UnacknowledgedPeriodicScoreReward,
    UnacknowledgedRecycling,
    UnacknowledgedTournamentDrop,
    UnacknowledgedQuestReward,
    UnacknowledgedLevelUpReward,
    UnacknowledgedAd
};

// CSOEconItem inventory field when unacknowledged
constexpr uint32_t InventoryUnacknowledged(UnacknowledgedType type)
{
    constexpr uint32_t InventroyUnacknowledgedMask = (1u << 30);
    return type | InventroyUnacknowledgedMask;
}
