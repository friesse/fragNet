#pragma once
#include "gc_const.hpp"
#include "gc_const_csgo.hpp"
#include "networking.hpp"
#include "steam_network_message.hpp"
#include "item_schema.hpp"
#include "cc_gcmessages.pb.h"
#include <sstream>
#include <iomanip>
#include <mariadb/mysql.h>

extern ItemSchema *g_itemSchema;

class GCNetwork_Inventory
{
public:
    // init ItemSchema
    static bool Init();
    static void Cleanup();

    static uint32_t GetItemSlot(uint32_t defIndex);
    static std::vector<uint32_t> GetDefindexFromItemSlot(uint32_t slotId);
    static void SendSOCache(SNetSocket_t p2psocket, uint64_t steamId, MYSQL *inventory_db);

    // item notif
    static bool CheckAndSendNewItemsSince(
        SNetSocket_t p2psocket,
        uint64_t steamId,
        uint64_t &lastItemId,
        MYSQL *inventory_db);

    static uint64_t GetLatestItemIdForUser(
        uint64_t steamId,
        MYSQL *inventory_db);

    // handles client -> gc msg
    static int ProcessClientAcknowledgment(
        SNetSocket_t p2psocket,
        uint64_t steamId,
        const CMsgGC_CC_CL2GC_ItemAcknowledged &message,
        MYSQL *inventory_db);

    // create item helpers (these are for creating CSOEconItems)
    static CSOEconItem *CreateItemFromDatabaseRow(
        uint64_t steamId,
        MYSQL_ROW &row,
        int overrideAcknowledged = -1);

    static CSOEconItem *FetchItemFromDatabase(
        uint64_t itemId,
        uint64_t steamId,
        MYSQL *inventory_db,
        int overrideAcknowledged = -1);

    // Network message helpers
    static bool DeleteItem(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, MYSQL *inventory_db);
    static bool SendSOSingleObject(SNetSocket_t p2psocket, uint64_t steamId, SOTypeId type, const google::protobuf::MessageLite &object, uint32_t messageType = k_EMsgGC_CC_GC2CL_SOSingleObject);
    static void AddToMultipleObjectsMessage(CMsgSOMultipleObjects &message, SOTypeId type, const google::protobuf::MessageLite &object, const std::string &collection = "modified");
    static void InitMultipleObjectsMessage(CMsgSOMultipleObjects &message, uint64_t steamId);
    static bool SendSOMultipleObjects(SNetSocket_t p2psocket, const CMsgSOMultipleObjects &message);

    // base items
    static CSOEconItem *CreateBaseItem(uint32_t defIndex, uint64_t steamId, MYSQL *inventory_db, bool saveToDb, const std::string &customName);
    static bool IsDefaultItemId(uint64_t itemId, uint32_t &defIndex, uint32_t &paintKitIndex);

    // Case unboxing and item creation
    static bool HandleUnboxCrate(SNetSocket_t p2psocket, uint64_t steamId, uint64_t crateItemId, MYSQL *inventory_db);
    static uint64_t SaveNewItemToDatabase(const CSOEconItem &item, uint64_t steamId, MYSQL *inventory_db, bool isBaseWeapon = false);
    static bool GetWeaponInfo(uint32_t defIndex, std::string &weaponName, std::string &weaponId);

    // Equipping and unequipping
    static bool EquipItem(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, uint32_t classId, uint32_t slotId, MYSQL *inventory_db);
    static bool UnequipItem(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, MYSQL *inventory_db);
    static bool UnequipItemsInSlot(uint64_t steamId, uint32_t classId, uint32_t slotId, MYSQL *inventory_db);
    static bool SendEquipUpdate(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, uint32_t classId, uint32_t slotId, MYSQL *inventory_db);
    static bool SendUnequipUpdate(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, MYSQL *inventory_db, bool was_equipped_ct, bool was_equipped_t, uint32_t def_index);

    // Item naming
    static bool HandleNameItem(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, const std::string &name, MYSQL *inventory_db);
    static bool HandleNameBaseItem(SNetSocket_t p2psocket, uint64_t steamId, uint32_t defIndex, const std::string &name, MYSQL *inventory_db);
    static bool HandleRemoveItemName(SNetSocket_t p2psocket, uint64_t steamId, uint64_t itemId, MYSQL *inventory_db);

    // Stickers
    static bool ProcessStickerAction(SNetSocket_t p2psocket, uint64_t steamId, const CMsgGC_CC_CL2GC_ApplySticker &message, MYSQL *inventory_db);
    static bool HandleApplySticker(SNetSocket_t p2psocket, uint64_t steamId, const CMsgGC_CC_CL2GC_ApplySticker &message, MYSQL *inventory_db);
    static bool HandleScrapeSticker(SNetSocket_t p2psocket, uint64_t steamId, const CMsgGC_CC_CL2GC_ApplySticker &message, MYSQL *inventory_db);

    // Purchases
    static bool HandleStorePurchaseInit(SNetSocket_t p2psocket, uint64_t steamId, const CMsgGC_CC_CL2GC_StorePurchaseInit &message, MYSQL *inventory_db);
    static bool ProcessStorePurchase(SNetSocket_t p2psocket, uint64_t steamId, const CMsgGC_CC_CL2GC_StorePurchaseInit &message, MYSQL *inventory_db, uint64_t &txnId, std::vector<uint64_t> &itemIds);

    // ATTRIBUTE HELPERS
    static void AddFloatAttribute(CSOEconItem *item, uint32_t defIndex, float value)
    {
        auto attr = item->add_attribute();
        attr->set_def_index(defIndex);
        attr->set_value_bytes(&value, sizeof(value));
    }
    static void AddUint32Attribute(CSOEconItem *item, uint32_t defIndex, uint32_t value)
    {
        auto attr = item->add_attribute();
        attr->set_def_index(defIndex);
        attr->set_value_bytes(&value, sizeof(value));
    }
    static void AddStringAttribute(CSOEconItem *item, uint32_t defIndex, const std::string &value)
    {
        auto attr = item->add_attribute();
        attr->set_def_index(defIndex);
        attr->set_value_bytes(value);
    }
    static float GetFloatAttribute(const CSOEconItemAttribute *attr)
    {
        if (attr->value_bytes().size() >= sizeof(float))
        {
            return *reinterpret_cast<const float *>(attr->value_bytes().data());
        }
        return 0.0f;
    }
    static uint32_t GetUint32Attribute(const CSOEconItemAttribute *attr)
    {
        if (attr->value_bytes().size() >= sizeof(uint32_t))
        {
            return *reinterpret_cast<const uint32_t *>(attr->value_bytes().data());
        }
        return 0;
    }
    static std::string GetStringAttribute(const CSOEconItemAttribute *attr)
    {
        return attr->value_bytes();
    }

    // Constants
    static constexpr uint32_t ATTR_PAINT_INDEX = 6;
    static constexpr uint32_t ATTR_PAINT_SEED = 7;
    static constexpr uint32_t ATTR_PAINT_WEAR = 8;
    static constexpr uint32_t ATTR_TRADE_RESTRICTION = 75;
    static constexpr uint32_t ATTR_KILLEATER_SCORE = 80;
    static constexpr uint32_t ATTR_KILLEATER_TYPE = 81;
    static constexpr uint32_t ATTR_NAME_TAG = 111;

    static constexpr uint32_t ATTR_STICKER_ID_START = 113;
    static constexpr uint32_t ATTR_STICKER_WEAR_START = 114;

    static constexpr uint32_t ATTR_ITEM_STICKER_ID = 113;  // "sticker slot 0 id"
    static constexpr uint32_t ATTR_ITEM_MUSICKIT_ID = 166; // "music id"

    static constexpr uint32_t SLOT_PRIMARY = 0;
    static constexpr uint32_t SLOT_SECONDARY = 1;
    static constexpr uint32_t SLOT_KNIFE = 2;
    static constexpr uint32_t SLOT_GRENADE = 3;
    static constexpr uint32_t SLOT_BOMB = 4; // literally just for renaming the bomb

    static constexpr uint32_t CLASS_NONE = 0;
    static constexpr uint32_t CLASS_T = 2;
    static constexpr uint32_t CLASS_CT = 3;

private:
    struct ItemAttribute
    {
        uint32_t def_index;
        float value;
    };
    static bool ParseItemId(const std::string &item_id, uint32_t &def_index, uint32_t &paint_index);
    static void AddStickerAttributes(CSOEconItem *item, MYSQL_ROW &row, int sticker_index);
    static void AddEquippedState(CSOEconItem *item, bool equipped, uint32_t class_id, uint32_t def_index);
};