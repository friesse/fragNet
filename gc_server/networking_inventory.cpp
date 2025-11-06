#include "networking_inventory.hpp"
#include "gc_const_csgo.hpp"
#include "keyvalue_english.hpp"
#include "logger.hpp"

ItemSchema *g_itemSchema = nullptr;

bool GCNetwork_Inventory::Init()
{
    if (g_itemSchema != nullptr)
    {
        return true;
    }

    // note: localization initializes on first use through LocalizationSystem::GetInstance()

    // init ItemSchema
    g_itemSchema = new ItemSchema();
    if (g_itemSchema)
    {
        logger::info("GCNetwork_Inventory::Init: ItemSchema initialized successfully");

        // Verify that localization system is working
        std::string_view testString = LocalizeToken("SFUI_WPNHUD_SSG08", "Scout");
        logger::info("GCNetwork_Inventory::Init: Localization test - SSG08 resolves to '%s'",
                     std::string{testString}.c_str());
        return true;
    }
    else
    {
        logger::error("GCNetwork_Inventory::Init: Failed to create ItemSchema instance");
        return false;
    }
}

void GCNetwork_Inventory::Cleanup()
{
    if (g_itemSchema)
    {
        delete g_itemSchema;
        g_itemSchema = nullptr;
    }
}

/**
 * Parses an item ID string to extract definition index and paint index
 *
 * @param item_id String representation of the item ID (e.g. "skin_123_456")
 * @param def_index Output parameter for the definition index
 * @param paint_index Output parameter for the paint index
 * @return True if parsing was successful, false otherwise
 */
bool GCNetwork_Inventory::ParseItemId(const std::string &item_id, uint32_t &def_index, uint32_t &paint_index)
{
    try
    {
        size_t dash_pos = item_id.find('-');
        if (dash_pos != std::string::npos)
        {
            std::string type = item_id.substr(0, dash_pos);
            std::string number_part = item_id.substr(dash_pos + 1);
            uint32_t item_number = std::stoi(number_part);

            if (type == "music_kit")
            {
                def_index = 1314;
                paint_index = item_number; // just reusing paint_index cause uhhh yes
                return true;
            }
            else if (type == "sticker")
            {
                def_index = 1209;
                paint_index = item_number;
                return true;
            }
            else if (type == "crate")
            {
                def_index = item_number;
                paint_index = 0;
                return true;
            }
            else if (type == "key")
            {
                def_index = item_number;
                paint_index = 0;
                return true;
            }
            else if (type == "collectible")
            {
                def_index = item_number;
                paint_index = 0;
                return true;
            }
        }

        // weapon skins:
        size_t first_underscore = item_id.find('_');
        size_t second_underscore = item_id.find('_', first_underscore + 1);
        if (first_underscore == std::string::npos || second_underscore == std::string::npos)
        {
            logger::error("ParseItemId: Failed to find required underscores");
            return false;
        }

        std::string def_index_str = item_id.substr(5, first_underscore - 5);
        std::string paint_index_str = item_id.substr(first_underscore + 1, second_underscore - first_underscore - 1);
        def_index = std::stoi(def_index_str);
        paint_index = std::stoi(paint_index_str);

        // logger::info("ParseItemId: parsed item def_index: %u, paint_index: %u", def_index, paint_index);
        return true;
    }
    catch (const std::exception &e)
    {
        logger::error("ParseItemId: Exception caught: %s", e.what());
        return false;
    }
    catch (...)
    {
        logger::error("ParseItemId: Unknown exception caught");
        return false;
    }
}

/**
 * Adds sticker attributes to an item based on database values
 *
 * @param item Pointer to the CSOEconItem to modify
 * @param row MySQL row containing sticker data
 * @param sticker_index Index of the sticker position (0-4)
 */
void GCNetwork_Inventory::AddStickerAttributes(CSOEconItem *item, MYSQL_ROW &row, int sticker_index)
{
    int sticker_col = 8 + (sticker_index * 2);
    int sticker_wear_col = sticker_col + 1;

    if (row[sticker_col] && atoi(row[sticker_col]) > 0)
    {
        uint32_t sticker_id_attr = 113 + (sticker_index * 4);
        uint32_t sticker_wear_attr = sticker_id_attr + 1;

        // sticker_id
        AddUint32Attribute(item, sticker_id_attr, atoi(row[sticker_col]));

        // sticker_wear
        if (row[sticker_wear_col] && strlen(row[sticker_wear_col]) > 0)
        {
            AddFloatAttribute(item, sticker_wear_attr, std::stof(row[sticker_wear_col]));
        }
        else
        {
            AddFloatAttribute(item, sticker_wear_attr, 0.0f);
        }
    }
}

/**
 * Adds equipped state to an item if it is equipped
 *
 * @param item Pointer to the CSOEconItem to modify
 * @param equipped Boolean indicating if the item is equipped
 * @param class_id Class ID for which the item is equipped (CT/T)
 * @param def_index Definition index of the item
 */
void GCNetwork_Inventory::AddEquippedState(CSOEconItem *item, bool equipped, uint32_t class_id, uint32_t def_index)
{
    if (equipped)
    {
        auto equipped_state = item->add_equipped_state();
        equipped_state->set_new_class(class_id);
        equipped_state->set_new_slot(GetItemSlot(def_index));
    }
}

/**
 * Determines the appropriate slot ID for a given defindex
 *
 * @param defIndex The definition index of the item
 * @return The slot ID where this item should be equipped
 */
uint32_t GCNetwork_Inventory::GetItemSlot(uint32_t defIndex)
{
    if (defIndex >= 500 && defIndex <= 552)
        return 0; // Custom Knives
    if (defIndex == 42 || defIndex == 59)
        return 0; // Default knives

    if (defIndex == 49)
        return 1; // C4

    // Pistols
    if (defIndex == 4 || defIndex == 32 || defIndex == 61)
        return 2; // Glock / P2000 / USP-S
    if (defIndex == 2)
        return 3; // Dual Berettas
    if (defIndex == 36)
        return 4; // P250
    if (defIndex == 3 || defIndex == 30 || defIndex == 63)
        return 5; // Five-SeveN / Tec-9 / CZ75
    if (defIndex == 1 || defIndex == 64)
        return 6; // Deagle / R8

    // SMGs
    if (defIndex == 34 || defIndex == 17)
        return 8; // MP9 / MAC-10
    if (defIndex == 33 || defIndex == 23)
        return 9; // MP7 / MP5-SD
    if (defIndex == 24)
        return 10; // UMP-45
    if (defIndex == 19)
        return 11; // P90
    if (defIndex == 26)
        return 12; // PP-Bizon

    // Rifles
    if (defIndex == 10 || defIndex == 13)
        return 14; // FAMAS / Galil AR
    if (defIndex == 7 || defIndex == 16 || defIndex == 60)
        return 15; // AK-47 / M4A4 / M4A1-S
    if (defIndex == 40)
        return 16; // SSG 08 (Scout)
    if (defIndex == 39 || defIndex == 8)
        return 17; // SG 553 / AUG
    if (defIndex == 9)
        return 18; // AWP
    if (defIndex == 11 || defIndex == 38)
        return 19; // G3SG1 / SCAR-20

    // Heavy
    if (defIndex == 35)
        return 20; // Nova
    if (defIndex == 25)
        return 21; // XM1014
    if (defIndex == 29 || defIndex == 27)
        return 22; // Sawed-Off / MAG-7
    if (defIndex == 14)
        return 23; // M249
    if (defIndex == 28)
        return 24; // Negev

    // no gloves cuz its classiccounter

    if (defIndex == 1314)
        return 54; // Music kits

    return 55; // probably a collectible
}

/**
 * Returns a vector of defindexes that correspond to the given item slot
 * This is the reverse of GetItemSlot
 *
 * @param slotId The slot ID to look up
 * @return Vector of definition indexes that would be equipped in this slot
 */
std::vector<uint32_t> GCNetwork_Inventory::GetDefindexFromItemSlot(uint32_t slotId)
{
    std::vector<uint32_t> result;

    switch (slotId)
    {
    case 0: // Knives
        // Default knives
        result.push_back(42); // Default CT Knife
        result.push_back(59); // Default T Knife

        // Custom knives (500-552)
        for (uint32_t i = 500; i <= 552; i++)
        {
            result.push_back(i);
        }
        break;

    case 1: // C4
        result.push_back(49);
        break;

    case 2:                   // Glock / P2000 / USP-S
        result.push_back(4);  // Glock-18
        result.push_back(32); // P2000
        result.push_back(61); // USP-S
        break;

    case 3: // Dual Berettas
        result.push_back(2);
        break;

    case 4: // P250
        result.push_back(36);
        break;

    case 5:                   // Five-SeveN / Tec-9 / CZ75
        result.push_back(3);  // Five-SeveN
        result.push_back(30); // Tec-9
        result.push_back(63); // CZ75-Auto
        break;

    case 6:                   // Deagle / R8
        result.push_back(1);  // Desert Eagle
        result.push_back(64); // R8 Revolver
        break;

    case 8:                   // MP9 / MAC-10
        result.push_back(34); // MP9
        result.push_back(17); // MAC-10
        break;

    case 9:                   // MP7 / MP5-SD
        result.push_back(33); // MP7
        result.push_back(23); // MP5-SD
        break;

    case 10: // UMP-45
        result.push_back(24);
        break;

    case 11: // P90
        result.push_back(19);
        break;

    case 12: // PP-Bizon
        result.push_back(26);
        break;

    case 14:                  // FAMAS / Galil AR
        result.push_back(10); // FAMAS
        result.push_back(13); // Galil AR
        break;

    case 15:                  // AK-47 / M4A4 / M4A1-S
        result.push_back(7);  // AK-47
        result.push_back(16); // M4A4
        result.push_back(60); // M4A1-S
        break;

    case 16: // SSG 08 (Scout)
        result.push_back(40);
        break;

    case 17:                  // SG 553 / AUG
        result.push_back(39); // SG 553
        result.push_back(8);  // AUG
        break;

    case 18: // AWP
        result.push_back(9);
        break;

    case 19:                  // G3SG1 / SCAR-20
        result.push_back(11); // G3SG1
        result.push_back(38); // SCAR-20
        break;

    case 20: // Nova
        result.push_back(35);
        break;

    case 21: // XM1014
        result.push_back(25);
        break;

    case 22:                  // Sawed-Off / MAG-7
        result.push_back(29); // Sawed-Off
        result.push_back(27); // MAG-7
        break;

    case 23: // M249
        result.push_back(14);
        break;

    case 24: // Negev
        result.push_back(28);
        break;

    case 54: // Music kits
        result.push_back(1314);
        break;

    case 55: // Collectibles (Generic catch-all)
        // bruh
        for (uint32_t i = 1000; i <= 5000; i++)
        {
            result.push_back(i);
        }
        break;
    }

    return result;
}

/**
 * Sends the CMsgSOCacheSubscribed message to a client
 * Populates and sends the full inventory state including items, equipped states, and player data
 *
 * @param p2psocket The socket to send the cache to
 * @param steamId The steam ID of the player
 * @param inventory_db Database connection to fetch inventory data
 */
void GCNetwork_Inventory::SendSOCache(SNetSocket_t p2psocket, uint64_t steamId, MYSQL *inventory_db)
{
    CMsgSOCacheSubscribed cacheMsg;

    cacheMsg.set_version(InventoryVersion);
    cacheMsg.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    cacheMsg.mutable_owner_soid()->set_id(steamId);

    // CSOEconItem
    {
        CMsgSOCacheSubscribed_SubscribedType *object = cacheMsg.add_objects();
        object->set_type_id(SOTypeItem);

        // everyone gets a nametag
        {
            auto nametag = CSOEconItem();
            nametag.set_id(1);
            nametag.set_account_id(steamId & 0xFFFFFFFF);
            nametag.set_def_index(1200);
            nametag.set_inventory(1);
            nametag.set_level(1);
            nametag.set_quality(0);
            nametag.set_flags(0);
            nametag.set_origin(kEconItemOrigin_Purchased);
            nametag.set_rarity(1);

            object->add_object_data(nametag.SerializeAsString());
        }

        char query[1024];
        snprintf(query, sizeof(query),
                 "SELECT id, item_id, floatval, rarity, quality, tradable, "
                 "stattrak, stattrak_kills, "
                 "sticker_1, sticker_1_wear, sticker_2, sticker_2_wear, "
                 "sticker_3, sticker_3_wear, sticker_4, sticker_4_wear, "
                 "sticker_5, sticker_5_wear, nametag, pattern_index, "
                 "equipped_ct, equipped_t, acknowledged, acquired_by "
                 "FROM csgo_items "
                 "WHERE owner_steamid2 = '%s'",
                 GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        logger::info("SendSOCache: Executing query: %s", query);

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("SendSOCache: MySQL query failed: %s", mysql_error(inventory_db));
            return;
        }

        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (!result)
        {
            logger::error("SendSOCache: Failed to store MySQL result");
            return;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result)))
        {
            if (!row[1])
            {
                logger::error("SendSOCache: Item ID is NULL in database row");
                continue;
            }

            try
            {
                CSOEconItem *item = CreateItemFromDatabaseRow(steamId, row);
                if (item)
                {
                    object->add_object_data(item->SerializeAsString());
                    delete item; // Clean up
                }
            }
            catch (const std::exception &e)
            {
                logger::error("SendSOCache: Exception while processing item: %s", e.what());
                continue;
            }
        }

        mysql_free_result(result);
    }

    // SOTypeDefaultEquippedDefinitionInstanceClient
    {
        // mysql stuff
        char check_exists_query[256];
        snprintf(check_exists_query, sizeof(check_exists_query),
                 "INSERT IGNORE INTO csgo_defaultequips (owner_id) VALUES (%llu)", steamId);

        if (mysql_query(inventory_db, check_exists_query) != 0)
        {
            logger::error("SendSOCache: MySQL default equips insert check failed: %s", mysql_error(inventory_db));
            return;
        }

        // query
        char default_equips_query[256];
        snprintf(default_equips_query, sizeof(default_equips_query),
                 "SELECT * FROM csgo_defaultequips WHERE owner_id = %llu", steamId);

        if (mysql_query(inventory_db, default_equips_query) != 0)
        {
            logger::error("SendSOCache: MySQL default equips query failed: %s", mysql_error(inventory_db));
            return;
        }

        // bruh
        MYSQL_RES *default_equips_result = mysql_store_result(inventory_db);
        if (!default_equips_result)
        {
            logger::error("SendSOCache: Failed to store MySQL default equips result");
            return;
        }

        {
            CMsgSOCacheSubscribed_SubscribedType *object = cacheMsg.add_objects();
            object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);

            MYSQL_ROW default_equips_row = mysql_fetch_row(default_equips_result);
            if (default_equips_row)
            {
                uint32_t account_id = steamId & 0xFFFFFFFF;

                // USP-S for CT (def_index 61, slot 2)
                if (atoi(default_equips_row[1]) == 1) // default_usp_ct
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(61);
                    defaultEquip->set_class_id(CLASS_CT);
                    defaultEquip->set_slot_id(2);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }

                // M4A1-S for CT (def_index 60, slot 15)
                if (atoi(default_equips_row[2]) == 1) // default_m4a1s_ct
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(60);
                    defaultEquip->set_class_id(CLASS_CT);
                    defaultEquip->set_slot_id(15);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }

                // R8 Revolver
                if (atoi(default_equips_row[3]) == 1) // default_r8_ct
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(64);
                    defaultEquip->set_class_id(CLASS_CT);
                    defaultEquip->set_slot_id(6);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }

                if (atoi(default_equips_row[4]) == 1) // default_r8_t
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(64);
                    defaultEquip->set_class_id(CLASS_T);
                    defaultEquip->set_slot_id(6);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }

                // CZ75-Auto
                if (atoi(default_equips_row[5]) == 1) // default_cz75_ct
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(63);
                    defaultEquip->set_class_id(CLASS_CT);
                    defaultEquip->set_slot_id(5);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }

                if (atoi(default_equips_row[6]) == 1) // default_cz75_t
                {
                    auto defaultEquip = new CSOEconDefaultEquippedDefinitionInstanceClient();
                    defaultEquip->set_account_id(account_id);
                    defaultEquip->set_item_definition(63);
                    defaultEquip->set_class_id(CLASS_T);
                    defaultEquip->set_slot_id(5);
                    object->add_object_data(defaultEquip->SerializeAsString());
                    delete defaultEquip;
                }
            }
        }

        mysql_free_result(default_equips_result);
    }

    // PersonaData
    {
        CSOPersonaDataPublic personaData;
        personaData.set_player_level(1); // todo: fetch from db
        personaData.set_elevated_state(true);

        CMsgSOCacheSubscribed_SubscribedType *object = cacheMsg.add_objects();
        object->set_type_id(SOTypePersonaDataPublic);
        object->add_object_data(personaData.SerializeAsString());
    }

    // GameAccountClient (if (!server))
    {
        CSOEconGameAccountClient accountClient;
        accountClient.set_additional_backpack_slots(0);
        accountClient.set_bonus_xp_timestamp_refresh(static_cast<uint32_t>(time(nullptr)));
        accountClient.set_bonus_xp_usedflags(16); // caught cheater lobbies, overwatch bonus etc
        accountClient.set_elevated_state(ElevatedStatePrime);
        accountClient.set_elevated_timestamp(ElevatedStatePrime); // is this actually 5???

        CMsgSOCacheSubscribed_SubscribedType *object = cacheMsg.add_objects();
        object->set_type_id(SOTypeGameAccountClient);
        object->add_object_data(accountClient.SerializeAsString());
    }

    NetworkMessage responseMsg = NetworkMessage::FromProto(cacheMsg, k_EMsgGC_CC_GC2CL_SOCacheSubscribed);

    // Log total objects and serialized size of cached objects
    logger::info("SendSOCache: Sending SOCache - Total objects: %d", cacheMsg.objects_size());

    // Log individual object details
    for (int i = 0; i < cacheMsg.objects_size(); i++)
    {
        const auto &obj = cacheMsg.objects(i);
        logger::info("Object %d - Type: %u, Data count: %d, Object size: %d",
                     i, obj.type_id(), obj.object_data_size(),
                     obj.ByteSizeLong());
    }

    uint32_t totalSize = responseMsg.GetTotalSize();
    logger::info("SendSOCache: Total message size: %u bytes", totalSize);

    responseMsg.WriteToSocket(p2psocket, true);

    logger::info("SendSOCache: Sent SOCache for steamid %llu", steamId);
}

/**
 * Helper function to create a fully populated CSOEconItem from database row
 *
 * @param steamId The steam ID of the item's owner
 * @param row MySQL row containing the item data
 * @param overrideAcknowledged Optional value to override the acknowledged/inventory position
 * @return Pointer to a new CSOEconItem object (caller must manage memory)
 */
CSOEconItem *GCNetwork_Inventory::CreateItemFromDatabaseRow(
    uint64_t steamId,
    MYSQL_ROW &row,
    int overrideAcknowledged)
{
    if (!row)
    {
        logger::error("CreateItemFromDatabaseRow: NULL row pointer");
        return nullptr;
    }

    try
    {
        CSOEconItem *item = new CSOEconItem();

        // Parse item_id and get def_index and paint_index
        uint32_t def_index, paint_index;
        if (!row[1] || !ParseItemId(row[1], def_index, paint_index))
        {
            logger::error("CreateItemFromDatabaseRow: Failed to parse item_id: %s", row[1] ? row[1] : "null");
            delete item;
            return nullptr;
        }

        // Base properties
        item->set_id(row[0] ? strtoull(row[0], nullptr, 10) : 0);
        item->set_account_id(steamId & 0xFFFFFFFF);
        item->set_def_index(def_index);

        // Set inventory position (acknowledged)
        if (overrideAcknowledged >= 0)
        {
            item->set_inventory(overrideAcknowledged);
        }
        else
        {
            item->set_inventory(row[22] ? static_cast<uint32_t>(atoi(row[22])) : 0);
        }

        item->set_level(1);
        item->set_quality(row[4] ? static_cast<uint32_t>(atoi(row[4])) : 0);
        item->set_flags(0);

        // Item origin
        int originType = kEconItemOrigin_FoundInCrate;
        if (row[23])
        {
            std::string acquiredBy = row[23];
            if (acquiredBy == "trade")
            {
                originType = kEconItemOrigin_Traded;
            }
            else if (acquiredBy == "trade_up")
            {
                originType = kEconItemOrigin_Crafted;
            }
            else if (acquiredBy == "ingame_drop")
            {
                originType = kEconItemOrigin_Drop;
            }
            else if (acquiredBy == "purchased")
            {
                originType = kEconItemOrigin_Purchased;
            }
            else if (acquiredBy == "0" || acquiredBy.empty())
            {
                originType = kEconItemOrigin_FoundInCrate;
            }
        }
        item->set_origin(originType);

        // Custom name
        if (row[18] && strlen(row[18]) > 0)
        {
            item->set_custom_name(row[18]);
        }

        // Rarity (add 1 to match expected range)
        item->set_rarity(row[3] ? static_cast<uint32_t>(atoi(row[3]) + 1) : 0);

        // Set attributes based on item type
        if (def_index == 1209)
        {
            // Sticker item
            AddUint32Attribute(item, ATTR_ITEM_STICKER_ID, paint_index);
        }
        else if (def_index == 1314)
        {
            // Music kit item
            AddUint32Attribute(item, ATTR_ITEM_MUSICKIT_ID, paint_index);
        }
        else
        {
            // Weapon skins
            if (paint_index > 0)
            {
                AddFloatAttribute(item, ATTR_PAINT_INDEX, paint_index);

                if (row[2] && strlen(row[2]) > 0)
                {
                    AddFloatAttribute(item, ATTR_PAINT_WEAR, std::stof(row[2]));
                }

                if (row[19] && strlen(row[19]) > 0)
                {
                    AddFloatAttribute(item, ATTR_PAINT_SEED, atoi(row[19]));
                }
            }

            // StatTrak
            if (row[6] && atoi(row[6]) == 1)
            {
                AddUint32Attribute(item, ATTR_KILLEATER_SCORE, row[7] ? atoi(row[7]) : 0);
                AddUint32Attribute(item, ATTR_KILLEATER_TYPE, 0);
            }

            // Untradable
            if (row[5] && atoi(row[5]) == 0)
            {
                AddUint32Attribute(item, ATTR_TRADE_RESTRICTION, 3133696800); // 4/20/2069
            }

            // Stickers for weapons only
            if (def_index != 1209 && def_index != 1314)
            {
                for (int i = 0; i < 5; i++)
                {
                    AddStickerAttributes(item, row, i);
                }
            }
        }

        // Equipment state
        bool equipped_ct = row[20] ? atoi(row[20]) == 1 : false;
        bool equipped_t = row[21] ? atoi(row[21]) == 1 : false;

        std::string item_id = row[1] ? row[1] : "";
        bool isCollectible = (item_id.length() >= 12) ? (item_id.substr(0, 12) == "collectible-") : false;
        bool isMusicKit = (def_index == 1314);

        if (isCollectible || isMusicKit)
        {
            if (equipped_t)
            {
                AddEquippedState(item, true, 0, def_index);
            }
        }
        // Weapon skins (not stickers)
        else if (def_index != 1209)
        {
            AddEquippedState(item, equipped_ct, CLASS_CT, def_index);
            AddEquippedState(item, equipped_t, CLASS_T, def_index);
        }

        return item;
    }
    catch (const std::exception &e)
    {
        logger::error("CreateItemFromDatabaseRow: Exception caught: %s", e.what());
        return nullptr;
    }
    catch (...)
    {
        logger::error("CreateItemFromDatabaseRow: Unknown exception caught");
        return nullptr;
    }
}

/**
 * Helper to fetch an item from database and create a CSOEconItem
 *
 * @param itemId The unique ID of the item
 * @param steamId The owner's Steam ID
 * @param inventory_db Database connection
 * @param overrideAcknowledged Optional value to override the acknowledged/inventory position
 * @return Pointer to a new CSOEconItem object (caller must manage memory)
 */
CSOEconItem *GCNetwork_Inventory::FetchItemFromDatabase(
    uint64_t itemId,
    uint64_t steamId,
    MYSQL *inventory_db,
    int overrideAcknowledged)
{
    if (!inventory_db)
    {
        logger::error("FetchItemFromDatabase: NULL database connection");
        return nullptr;
    }

    // Query to fetch the item data
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT id, item_id, floatval, rarity, quality, tradable, "
             "stattrak, stattrak_kills, "
             "sticker_1, sticker_1_wear, sticker_2, sticker_2_wear, "
             "sticker_3, sticker_3_wear, sticker_4, sticker_4_wear, "
             "sticker_5, sticker_5_wear, nametag, pattern_index, "
             "equipped_ct, equipped_t, acknowledged, acquired_by "
             "FROM csgo_items "
             "WHERE id = %llu AND owner_steamid2 = '%s'",
             itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

    if (mysql_query(inventory_db, query) != 0)
    {
        logger::error("FetchItemFromDatabase: MySQL query failed: %s", mysql_error(inventory_db));
        return nullptr;
    }

    MYSQL_RES *result = mysql_store_result(inventory_db);
    if (!result)
    {
        logger::error("FetchItemFromDatabase: Failed to store MySQL result");
        return nullptr;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    CSOEconItem *item = nullptr;

    if (row)
    {
        item = CreateItemFromDatabaseRow(steamId, row, overrideAcknowledged);
    }
    else
    {
        logger::error("FetchItemFromDatabase: Item not found: %llu", itemId);
    }

    mysql_free_result(result);
    return item;
}

/**
 * Checks for new items and sends them to the client
 * For items with acquired_by="0", also sends the same item as an UnlockCrateResponse
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param lastItemId Reference to the last known item ID, which will be updated
 * @param inventory_db Database connection to fetch inventory data
 * @return True if new items were found and sent
 */
bool GCNetwork_Inventory::CheckAndSendNewItemsSince(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t &lastItemId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("CheckAndSendNewItemsSince: Database connection is null");
        return false;
    }

    // query to find new items with an id higher than lastItemId
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT id, item_id, floatval, rarity, quality, tradable, "
             "stattrak, stattrak_kills, "
             "sticker_1, sticker_1_wear, sticker_2, sticker_2_wear, "
             "sticker_3, sticker_3_wear, sticker_4, sticker_4_wear, "
             "sticker_5, sticker_5_wear, nametag, pattern_index, "
             "equipped_ct, equipped_t, acknowledged, acquired_by "
             "FROM csgo_items "
             "WHERE owner_steamid2 = '%s' AND id > %llu "
             "ORDER BY id ASC",
             GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str(), lastItemId);

    if (mysql_query(inventory_db, query) != 0)
    {
        logger::error("CheckAndSendNewItemsSince: MySQL query failed: %s", mysql_error(inventory_db));
        return false;
    }

    MYSQL_RES *result = mysql_store_result(inventory_db);
    if (!result)
    {
        logger::error("CheckAndSendNewItemsSince: Failed to store MySQL result");
        return false;
    }

    int numRows = mysql_num_rows(result);
    if (numRows == 0)
    {
        // no items
        mysql_free_result(result);
        return false;
    }

    logger::info("CheckAndSendNewItemsSince: Found %d new items for player %llu", numRows, steamId);

    bool updateSuccess = false;
    uint64_t highestItemId = lastItemId;

    // find highest item id
    MYSQL_ROW row;
    mysql_data_seek(result, 0);
    while ((row = mysql_fetch_row(result)))
    {
        uint64_t itemId = row[0] ? strtoull(row[0], nullptr, 10) : 0;
        if (itemId > highestItemId)
        {
            highestItemId = itemId;
        }
    }
    mysql_data_seek(result, 0);

    // one item - SOSingleObject
    row = mysql_fetch_row(result);
    if (row)
    {
        CSOEconItem *item = CreateItemFromDatabaseRow(steamId, row);
        if (item)
        {
            bool isFromCrate = (row[23] && strcmp(row[23], "0") == 0);

            if (isFromCrate)
            {
                // If the item was from a crate opening, only send it as UnlockCrateResponse
                logger::info("CheckAndSendNewItemsSince: Item has acquired_by='0', sending UnlockCrateResponse for item %llu", item->id());
                updateSuccess = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item, k_EMsgGC_CC_GC2CL_UnlockCrateResponse);
                //updateSuccess = true;
            }
            else
            {
                // For other items, send as standard SOSingleObject
                logger::info("CheckAndSendNewItemsSince: Sending 1 new item with SOSingleObject");
                updateSuccess = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item);
            }

            delete item;
        }
    }

    mysql_free_result(result);

    if (highestItemId > lastItemId)
    {
        if (updateSuccess)
        {
            logger::info("CheckAndSendNewItemsSince: Successfully sent new items to player %llu", steamId);
        }
        else
        {
            logger::warning("CheckAndSendNewItemsSince: Failed to send new items to player %llu, updating lastItemId anyway", steamId);
        }

        logger::info("CheckAndSendNewItemsSince: Updated lastItemId from %llu to %llu", lastItemId, highestItemId);
        lastItemId = highestItemId;
    }

    return updateSuccess;
}

// helper for new item notif
uint64_t GCNetwork_Inventory::GetLatestItemIdForUser(
    uint64_t steamId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("GetLatestItemIdForUser: Database connection is null");
        return 0;
    }

    // Query to find the highest item ID for this user
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT MAX(id) FROM csgo_items WHERE owner_steamid2 = '%s'",
             GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

    if (mysql_query(inventory_db, query) != 0)
    {
        logger::error("GetLatestItemIdForUser: MySQL query failed: %s", mysql_error(inventory_db));
        return 0;
    }

    MYSQL_RES *result = mysql_store_result(inventory_db);
    if (!result)
    {
        logger::error("GetLatestItemIdForUser: Failed to store MySQL result");
        return 0;
    }

    uint64_t maxId = 0;
    MYSQL_ROW row = mysql_fetch_row(result);

    if (row && row[0])
    {
        maxId = strtoull(row[0], nullptr, 10);
    }

    mysql_free_result(result);

    logger::info("GetLatestItemIdForUser: Found highest item ID %llu for user %llu",
                 maxId, steamId);

    return maxId;
}

/**
 * Process an item acknowledgment message from the client
 * Handles multiple items being acknowledged at once
 *
 * @param p2psocket Socket for the client
 * @param steamId The steam ID of the player
 * @param message The acknowledgment message from the client
 * @param inventory_db Database connection
 * @return The number of items successfully acknowledged
 */
int GCNetwork_Inventory::ProcessClientAcknowledgment(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_ItemAcknowledged &message,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("ProcessClientAcknowledgment: Database connection is null");
        return 0;
    }

    if (message.item_id_size() == 0)
    {
        logger::warning("ProcessClientAcknowledgment: Empty acknowledgment message received");
        return 0;
    }

    logger::info("ProcessClientAcknowledgment: Processing acknowledgment for %d items from player %llu",
                 message.item_id_size(), steamId);

    // get the current highest inventory position for this user
    char max_pos_query[256];
    snprintf(max_pos_query, sizeof(max_pos_query),
             "SELECT COALESCE(MAX(acknowledged), 1) FROM csgo_items WHERE owner_steamid2 = '%s'",
             GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

    if (mysql_query(inventory_db, max_pos_query) != 0)
    {
        logger::error("ProcessClientAcknowledgment: Failed to get max position: %s", mysql_error(inventory_db));
        return 0;
    }

    MYSQL_RES *max_result = mysql_store_result(inventory_db);
    if (!max_result)
    {
        logger::error("ProcessClientAcknowledgment: Failed to store max position result");
        return 0;
    }

    MYSQL_ROW max_row = mysql_fetch_row(max_result);
    uint32_t next_position = 1; // default start at 1

    if (max_row && max_row[0])
    {
        next_position = atoi(max_row[0]);
    }

    mysql_free_result(max_result);

    // start transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("ProcessClientAcknowledgment: Failed to start transaction: %s", mysql_error(inventory_db));
        return 0;
    }

    int successCount = 0;

    // one item?
    bool isSingleItem = (message.item_id_size() == 1);
    CSOEconItem *singleItem = nullptr;

    // For multiple items, prepare the multiple objects message
    CMsgSOMultipleObjects updateMsg;
    if (!isSingleItem)
    {
        InitMultipleObjectsMessage(updateMsg, steamId);
    }

    // for each item
    for (int i = 0; i < message.item_id_size(); i++)
    {
        uint64_t itemId = message.item_id(i);

        next_position++;
        if (next_position == 1)
            next_position = 2; // skip position 1 (cause thats the nametag)

        // set acknowledged status to next available position for acknowledged id overhead
        char query[512];
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET acknowledged = %u "
                 "WHERE id = %llu AND owner_steamid2 = '%s' AND (acknowledged = 0 OR acknowledged IS NULL)",
                 next_position, itemId,
                 GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("ProcessClientAcknowledgment: MySQL query failed for item %llu: %s",
                          itemId, mysql_error(inventory_db));
            continue;
        }

        if (mysql_affected_rows(inventory_db) == 0)
        {
            logger::warning("ProcessClientAcknowledgment: Item %llu not found or already acknowledged", itemId);
            next_position--;
            continue;
        }

        // acknowledge success
        successCount++;

        // make item
        CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db, next_position);
        if (item)
        {
            if (isSingleItem)
            {
                // For a single item, just store it for later use
                singleItem = item;
            }
            else
            {
                // For multiple items, add to the message
                AddToMultipleObjectsMessage(updateMsg, SOTypeItem, *item);
                delete item;
            }
        }
    }

    // process
    if (successCount > 0)
    {
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("ProcessClientAcknowledgment: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");

            // Clean up if we have a single item
            if (singleItem)
            {
                delete singleItem;
            }

            return 0;
        }

        if (isSingleItem && singleItem)
        {
            // Send single item update
            logger::info("ProcessClientAcknowledgment: Sending single item update with SOSingleObject for item %llu",
                         singleItem->id());
            SendSOSingleObject(p2psocket, steamId, SOTypeItem, *singleItem);
            delete singleItem;
        }
        else if (!isSingleItem && updateMsg.objects_modified_size() > 0)
        {
            // Send multiple items update
            logger::info("ProcessClientAcknowledgment: Sending %d modified items with SOMultipleObjects",
                         updateMsg.objects_modified_size());
            SendSOMultipleObjects(p2psocket, updateMsg);
        }

        logger::info("ProcessClientAcknowledgment: Successfully acknowledged %d items for player %llu",
                     successCount, steamId);
    }
    else
    {
        mysql_query(inventory_db, "ROLLBACK");
        logger::warning("ProcessClientAcknowledgment: No items were acknowledged, transaction rolled back");

        // Clean up if we have a single item
        if (singleItem)
        {
            delete singleItem;
        }
    }

    return successCount;
}

/**
 * Handles the unboxing of a crate, generating a new item and saving it to the database
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param crateItemId The ID of the crate being opened
 * @param inventory_db Database connection to update
 * @return True if the crate was successfully opened
 */
bool GCNetwork_Inventory::HandleUnboxCrate(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t crateItemId,
    MYSQL *inventory_db)
{
    if (!g_itemSchema || !inventory_db)
    {
        logger::error("HandleUnboxCrate: ItemSchema or database connection is null");
        return false;
    }

    // verify
    CSOEconItem *crateItem = FetchItemFromDatabase(crateItemId, steamId, inventory_db);
    if (!crateItem)
    {
        logger::error("HandleUnboxCrate: Player %llu doesn't own crate %llu", steamId, crateItemId);
        return false;
    }

    // make item
    CSOEconItem newItem;
    bool result = g_itemSchema->SelectItemFromCrate(*crateItem, newItem);
    if (!result)
    {
        logger::error("HandleUnboxCrate: Failed to select item from crate %llu", crateItemId);
        delete crateItem;
        return false;
    }

    newItem.set_account_id(steamId & 0xFFFFFFFF);
    newItem.set_inventory(0);

    uint64_t newItemId = SaveNewItemToDatabase(newItem, steamId, inventory_db);
    if (newItemId == 0)
    {
        logger::error("HandleUnboxCrate: Failed to save new item to database");
        delete crateItem;
        return false;
    }

    // setting id to newest
    newItem.set_id(newItemId);

    // delete crate
    if (!DeleteItem(p2psocket, steamId, crateItemId, inventory_db))
    {
        logger::warning("HandleUnboxCrate: Failed to delete crate %llu after opening", crateItemId);
    }

    // this shit is fucked
    /*bool success = SendSOSingleObject(p2psocket, steamId, SOTypeItem, newItem, k_EMsgGC_CC_GC2CL_UnlockCrateResponse);
    if (!success)
    {
        logger::error("HandleUnboxCrate: Failed to send unlock notification to client");
    }*/

    delete crateItem;
    logger::info("HandleUnboxCrate: Successfully unboxed crate %llu for player %llu, got item %llu",
                 crateItemId, steamId, newItemId);
    return true;
}

/**
 * Saves a newly generated item to the database
 *
 * @param item The CSOEconItem to save
 * @param steamId The steam ID of the owner
 * @param inventory_db Database connection
 * @param isBaseWeapon Whether this is a base weapon without a skin (default: false)
 * @return The ID of the newly created item, or 0 on failure
 */
uint64_t GCNetwork_Inventory::SaveNewItemToDatabase(
    const CSOEconItem &item,
    uint64_t steamId,
    MYSQL *inventory_db,
    bool isBaseWeapon)
{
    if (!g_itemSchema || !inventory_db)
    {
        logger::error("SaveNewItemToDatabase: ItemSchema or database connection is null");
        return 0;
    }

    // extract item info
    uint32_t defIndex = item.def_index();
    uint32_t quality = item.quality();

    // for base weapons use NULL for
    bool isBaseItem = isBaseWeapon;
    uint32_t rarity = isBaseItem ? 0 : (item.rarity() > 0 ? item.rarity() - 1 : 0);

    // default, some will get populated later
    std::string itemIdStr = "";
    float floatValue = 0.0f;
    uint32_t paintIndex = 0;
    uint32_t patternIndex = 0;
    bool statTrak = false;
    uint32_t statTrakKills = 0;
    std::string nameTag = item.has_custom_name() ? item.custom_name() : "";
    bool tradable = isBaseItem ? false : true;             // Set to 0 for base items
    std::string acquiredBy = isBaseItem ? "default" : "0"; // Set to "default" for base items

    std::string itemName = "";
    std::string weaponType = "";
    std::string weaponId = "";
    std::string weaponSlot = "0";
    std::string wearName = "Factory New";

    // attributes
    // Modify the SaveNewItemToDatabase function to correctly handle souvenir items
    // Only showing the relevant part that needs to be changed

    // attributes
    for (int i = 0; i < item.attribute_size(); i++)
    {
        const CSOEconItemAttribute &attr = item.attribute(i);
        uint32_t attrDefIndex = attr.def_index();

        switch (attrDefIndex)
        {
        case ATTR_PAINT_INDEX: // paint kit - this is the texture prefab
            paintIndex = g_itemSchema->AttributeUint32(&attr);
            break;

        case ATTR_PAINT_WEAR: // float
            floatValue = g_itemSchema->AttributeFloat(&attr);
            // wear name
            if (floatValue < 0.07f)
                wearName = "Factory New";
            else if (floatValue < 0.15f)
                wearName = "Minimal Wear";
            else if (floatValue < 0.38f)
                wearName = "Field-Tested";
            else if (floatValue < 0.45f)
                wearName = "Well-Worn";
            else
                wearName = "Battle-Scarred";
            break;

        case ATTR_PAINT_SEED: // seed
            patternIndex = g_itemSchema->AttributeUint32(&attr);
            break;

        case ATTR_KILLEATER_SCORE: // StatTrak
            statTrak = true;
            statTrakKills = g_itemSchema->AttributeUint32(&attr);
            break;

        case ATTR_ITEM_STICKER_ID:
            if (defIndex == 1209)
            {
                paintIndex = g_itemSchema->AttributeUint32(&attr);
            }
            break;

        case ATTR_ITEM_MUSICKIT_ID:
            if (defIndex == 1314)
            {
                paintIndex = g_itemSchema->AttributeUint32(&attr);
            }
            break;
        }
    }

    // get attributes for stickers only
    std::vector<std::pair<uint32_t, float>> stickers;
    stickers.resize(5, {0, 0.0f}); // 5 max

    for (int i = 0; i < item.attribute_size(); i++)
    {
        const CSOEconItemAttribute &attr = item.attribute(i);
        uint32_t attrDefIndex = attr.def_index();

        // sticker attributes (113, 117, 121, 125, 129)
        if (attrDefIndex >= 113 && attrDefIndex <= 133 && (attrDefIndex - 113) % 4 == 0)
        {
            uint32_t stickerPos = (attrDefIndex - 113) / 4;
            if (stickerPos < stickers.size())
            {
                uint32_t stickerId = g_itemSchema->AttributeUint32(&attr);

                float stickerWear = 0.0f;
                for (int j = 0; j < item.attribute_size(); j++)
                {
                    const CSOEconItemAttribute &wearAttr = item.attribute(j);
                    if (wearAttr.def_index() == attrDefIndex + 1)
                    {
                        stickerWear = g_itemSchema->AttributeFloat(&wearAttr);
                        break;
                    }
                }

                stickers[stickerPos] = {stickerId, stickerWear};
            }
        }
    }

    // get info
    if (!GetWeaponInfo(defIndex, weaponType, weaponId))
    {
        // If GetWeaponInfo fails, try to get info from the ItemSchema
        auto itemInfoIter = g_itemSchema->m_itemInfo.find(defIndex);
        if (itemInfoIter != g_itemSchema->m_itemInfo.end())
        {
            const auto &itemInfo = itemInfoIter->second;
            std::string_view displayName = itemInfo.GetDisplayName();
            weaponType = std::string(displayName);
            weaponId = "weapon_" + std::string(itemInfo.m_name);
        }
        else
        {
            // If all else fails, use generic naming
            weaponType = "Unknown Weapon";
            weaponId = "weapon_" + std::to_string(defIndex);
        }
    }

    // Set item name based on weapon type and paint index
    itemName = weaponType;

    // Construct item_id string based on item type
    if (defIndex == 1209) // Sticker
    {
        itemIdStr = "sticker-" + std::to_string(paintIndex);
    }
    else if (defIndex == 1314) // Music kit
    {
        itemIdStr = "music_kit-" + std::to_string(paintIndex);
    }
    else if (defIndex >= 500 && defIndex <= 552) // Knife
    {
        if (isBaseWeapon || paintIndex == 0)
        {
            // Base knife with no skin
            itemIdStr = "skin-" + std::to_string(defIndex) + "_0_0";
        }
        else
        {
            // Knife with skin
            itemIdStr = "skin-" + std::to_string(defIndex) + "_" +
                        std::to_string(paintIndex) + "_0";
        }
    }
    else // Regular weapon
    {
        if (isBaseWeapon || paintIndex == 0)
        {
            // Base weapon with no skin
            itemIdStr = "skin-" + std::to_string(defIndex) + "_0_0";
        }
        else
        {
            // Weapon with skin
            itemIdStr = "skin-" + std::to_string(defIndex) + "_" +
                        std::to_string(paintIndex) + "_0";
        }
    }

    // For weapons with a paint kit (skin), add the skin name to the item name
    if (paintIndex > 0 && defIndex != 1209 && defIndex != 1314 && !isBaseWeapon)
    {
        // Look for the paint kit info
        for (const auto &[name, paintKit] : g_itemSchema->m_paintKitInfo)
        {
            if (paintKit.m_defIndex == paintIndex)
            {
                // Get the skin name
                std::string_view skinName = paintKit.GetDisplayName();
                if (!skinName.empty())
                {
                    itemName = itemName + " | " + std::string(skinName);
                    break;
                }
            }
        }
    }

    // Calculate sticker slots based on weapon type (most weapons have 4-5 slots)
    int stickerSlots = 0;
    if (defIndex != 1209 && defIndex != 1314)
    {
        // Special cases for weapons with 5 sticker slots
        if (defIndex == 11 || defIndex == 64)
        { // G3SG1 or R8 Revolver
            stickerSlots = 5;
        }
        else
        {
            // Default for most weapons: 4 sticker slots
            stickerSlots = 4;
        }
    }

    // Build the SQL query as a string directly instead of using stringstream
    std::string query = "INSERT INTO csgo_items (";
    query += "owner_steamid2, item_id, name, nametag, weapon_type, weapon_id, weapon_slot, ";
    query += "wear, floatval, paint_index, pattern_index, rarity, quality, tradable, ";
    query += "commodity, stattrak, stattrak_kills, sticker_slots";

    // Add sticker columns based on how many stickers we have
    for (size_t i = 0; i < stickers.size(); i++)
    {
        if (stickers[i].first > 0)
        {
            query += ", sticker_" + std::to_string(i + 1);
            query += ", sticker_" + std::to_string(i + 1) + "_wear";
        }
    }

    // Add remaining columns
    query += ", market_price, equipped_ct, equipped_t, acquired_by, acknowledged) VALUES (";

    // Add values
    query += "'" + GCNetwork_Users::SteamID64ToSteamID2(steamId) + "', "; // owner_steamid2
    query += "'" + itemIdStr + "', ";                                     // item_id

    // Escape strings to prevent SQL injection
    std::string escapedName(itemName.length() * 2 + 1, '\0');
    mysql_real_escape_string(inventory_db, &escapedName[0], itemName.c_str(), itemName.length());
    escapedName.resize(strlen(escapedName.c_str()));

    query += "'" + escapedName + "', "; // name

    // Handle name tag (if present)
    if (!nameTag.empty())
    {
        std::string escapedNameTag(nameTag.length() * 2 + 1, '\0');
        mysql_real_escape_string(inventory_db, &escapedNameTag[0], nameTag.c_str(), nameTag.length());
        escapedNameTag.resize(strlen(escapedNameTag.c_str()));
        query += "'" + escapedNameTag + "', ";
    }
    else
    {
        query += "NULL, ";
    }

    // Weapon information
    query += "'" + weaponType + "', "; // weapon_type
    query += "'" + weaponId + "', ";   // weapon_id
    query += "'" + weaponSlot + "', "; // weapon_slot

    // Wear and float
    query += "'" + wearName + "', "; // wear

    // Set floatval to NULL for base items
    if (isBaseItem)
    {
        query += "NULL, "; // floatval NULL for base items
    }
    else
    {
        std::stringstream floatStr;
        floatStr << std::fixed << std::setprecision(14) << floatValue;
        query += "'" + floatStr.str() + "', "; // floatval with value
    }

    // Paint and pattern
    query += "'" + std::to_string(paintIndex) + "', ";   // paint_index
    query += "'" + std::to_string(patternIndex) + "', "; // pattern_index

    // Rarity - set to NULL for base weapons
    if (isBaseItem)
    {
        query += "NULL, "; // rarity NULL for base weapons
    }
    else
    {
        query += "'" + std::to_string(rarity) + "', "; // rarity with value
    }

    query += "'" + std::to_string(quality) + "', ";          // quality
    query += "'" + std::to_string(tradable ? 1 : 0) + "', "; // tradable (0 for base items)
    query += "'0', ";                                        // commodity (default 0)
    query += "'" + std::to_string(statTrak ? 1 : 0) + "', "; // stattrak

    // StatTrak kills
    if (statTrak)
    {
        query += "'" + std::to_string(statTrakKills) + "', ";
    }
    else
    {
        query += "NULL, ";
    }

    // Sticker slots
    query += "'" + std::to_string(stickerSlots) + "'";

    // Sticker data (if any)
    for (size_t i = 0; i < stickers.size(); i++)
    {
        if (stickers[i].first > 0)
        {
            query += ", '" + std::to_string(stickers[i].first) + "'";
            query += ", '" + std::to_string(stickers[i].second) + "'";
        }
    }

    // Finish the query with remaining fields
    query += ", '0.00', '0', '0', '" + acquiredBy + "', '0')";

    // Execute the query
    logger::info("SaveNewItemToDatabase: SQL Query: %s", query.c_str());

    if (mysql_query(inventory_db, query.c_str()) != 0)
    {
        logger::error("SaveNewItemToDatabase: MySQL query failed: %s", mysql_error(inventory_db));
        return 0;
    }

    // Get the newly inserted item ID
    uint64_t newItemId = mysql_insert_id(inventory_db);
    logger::info("SaveNewItemToDatabase: Successfully inserted new item with ID %llu", newItemId);

    return newItemId;
}

/**
 * Gets the display name and weapon identifier for a given defIndex
 *
 * @param defIndex The definition index of the weapon
 * @param weaponName Output parameter for the weapon's display name
 * @param weaponId Output parameter for the weapon's identifier
 * @return True if the information was found, false otherwise
 */
bool GCNetwork_Inventory::GetWeaponInfo(
    uint32_t defIndex,
    std::string &weaponName,
    std::string &weaponId)
{
    weaponName = "";
    weaponId = "";

    // Check if this is a known weapon type
    switch (defIndex)
    {
    // Pistols
    case 1:
        weaponName = "Desert Eagle";
        weaponId = "weapon_deagle";
        break;
    case 2:
        weaponName = "Dual Berettas";
        weaponId = "weapon_elite";
        break;
    case 3:
        weaponName = "Five-SeveN";
        weaponId = "weapon_fiveseven";
        break;
    case 4:
        weaponName = "Glock-18";
        weaponId = "weapon_glock";
        break;
    case 30:
        weaponName = "Tec-9";
        weaponId = "weapon_tec9";
        break;
    case 32:
        weaponName = "P2000";
        weaponId = "weapon_hkp2000";
        break;
    case 36:
        weaponName = "P250";
        weaponId = "weapon_p250";
        break;
    case 61:
        weaponName = "USP-S";
        weaponId = "weapon_usp_silencer";
        break;
    case 63:
        weaponName = "CZ75-Auto";
        weaponId = "weapon_cz75a";
        break;
    case 64:
        weaponName = "R8 Revolver";
        weaponId = "weapon_revolver";
        break;

    // Rifles
    case 7:
        weaponName = "AK-47";
        weaponId = "weapon_ak47";
        break;
    case 8:
        weaponName = "AUG";
        weaponId = "weapon_aug";
        break;
    case 9:
        weaponName = "AWP";
        weaponId = "weapon_awp";
        break;
    case 10:
        weaponName = "FAMAS";
        weaponId = "weapon_famas";
        break;
    case 11:
        weaponName = "G3SG1";
        weaponId = "weapon_g3sg1";
        break;
    case 13:
        weaponName = "Galil AR";
        weaponId = "weapon_galilar";
        break;
    case 16:
        weaponName = "M4A4";
        weaponId = "weapon_m4a1";
        break;
    case 38:
        weaponName = "SCAR-20";
        weaponId = "weapon_scar20";
        break;
    case 39:
        weaponName = "SG 553";
        weaponId = "weapon_sg556";
        break;
    case 40:
        weaponName = "SSG 08";
        weaponId = "weapon_ssg08";
        break;
    case 60:
        weaponName = "M4A1-S";
        weaponId = "weapon_m4a1_silencer";
        break;

    // SMGs
    case 17:
        weaponName = "MAC-10";
        weaponId = "weapon_mac10";
        break;
    case 19:
        weaponName = "P90";
        weaponId = "weapon_p90";
        break;
    case 23:
        weaponName = "MP5-SD";
        weaponId = "weapon_mp5sd";
        break;
    case 24:
        weaponName = "UMP-45";
        weaponId = "weapon_ump45";
        break;
    case 26:
        weaponName = "PP-Bizon";
        weaponId = "weapon_bizon";
        break;
    case 33:
        weaponName = "MP7";
        weaponId = "weapon_mp7";
        break;
    case 34:
        weaponName = "MP9";
        weaponId = "weapon_mp9";
        break;

    // Heavy
    case 14:
        weaponName = "M249";
        weaponId = "weapon_m249";
        break;
    case 25:
        weaponName = "XM1014";
        weaponId = "weapon_xm1014";
        break;
    case 27:
        weaponName = "MAG-7";
        weaponId = "weapon_mag7";
        break;
    case 28:
        weaponName = "Negev";
        weaponId = "weapon_negev";
        break;
    case 29:
        weaponName = "Sawed-Off";
        weaponId = "weapon_sawedoff";
        break;
    case 35:
        weaponName = "Nova";
        weaponId = "weapon_nova";
        break;

    // Default Knives
    case 42:
        weaponName = "Knife (CT)";
        weaponId = "weapon_knife";
        break;
    case 59:
        weaponName = "Knife (T)";
        weaponId = "weapon_knife_t";
        break;

    // Special Knives
    case 500:
        weaponName = "Bayonet";
        weaponId = "weapon_bayonet";
        break;
    case 503:
        weaponName = "Classic Knife";
        weaponId = "weapon_knife_css";
        break;
    case 505:
        weaponName = "Flip Knife";
        weaponId = "weapon_knife_flip";
        break;
    case 506:
        weaponName = "Gut Knife";
        weaponId = "weapon_knife_gut";
        break;
    case 507:
        weaponName = "Karambit";
        weaponId = "weapon_knife_karambit";
        break;
    case 508:
        weaponName = "M9 Bayonet";
        weaponId = "weapon_knife_m9_bayonet";
        break;
    case 509:
        weaponName = "Huntsman Knife";
        weaponId = "weapon_knife_tactical";
        break;
    case 512:
        weaponName = "Falchion Knife";
        weaponId = "weapon_knife_falchion";
        break;
    case 514:
        weaponName = "Bowie Knife";
        weaponId = "weapon_knife_survival_bowie";
        break;
    case 515:
        weaponName = "Butterfly Knife";
        weaponId = "weapon_knife_butterfly";
        break;
    case 516:
        weaponName = "Shadow Daggers";
        weaponId = "weapon_knife_push";
        break;
    case 517:
        weaponName = "Paracord Knife";
        weaponId = "weapon_knife_cord";
        break;
    case 518:
        weaponName = "Survival Knife";
        weaponId = "weapon_knife_canis";
        break;
    case 519:
        weaponName = "Ursus Knife";
        weaponId = "weapon_knife_ursus";
        break;
    case 520:
        weaponName = "Navaja Knife";
        weaponId = "weapon_knife_gypsy_jackknife";
        break;
    case 521:
        weaponName = "Nomad Knife";
        weaponId = "weapon_knife_outdoor";
        break;
    case 522:
        weaponName = "Stiletto Knife";
        weaponId = "weapon_knife_stiletto";
        break;
    case 523:
        weaponName = "Talon Knife";
        weaponId = "weapon_knife_widowmaker";
        break;
    case 525:
        weaponName = "Skeleton Knife";
        weaponId = "weapon_knife_skeleton";
        break;

    // Equipment
    case 31:
        weaponName = "Zeus x27";
        weaponId = "weapon_taser";
        break;
    case 49:
        weaponName = "C4";
        weaponId = "weapon_c4";
        break;

    // Other special items
    case 1209:
        weaponName = "Sticker";
        weaponId = "sticker";
        break;
    case 1314:
        weaponName = "Music Kit";
        weaponId = "music_kit";
        break;

    default:
        // Try to fetch from ItemSchema for unknown defIndex
        if (g_itemSchema)
        {
            auto itemInfoIter = g_itemSchema->m_itemInfo.find(defIndex);
            if (itemInfoIter != g_itemSchema->m_itemInfo.end())
            {
                const auto &itemInfo = itemInfoIter->second;
                std::string_view displayName = itemInfo.GetDisplayName();
                weaponName = std::string(displayName);
                weaponId = "weapon_" + std::string(itemInfo.m_name);
                return !weaponName.empty();
            }
        }
        return false;
    }

    return true;
}

//
// ITEM UPDATES
//

/**
 * Deletes an item from the database and notifies the client
 *
 * @param p2psocket The socket to notify (optional)
 * @param steamId The steam ID of the item's owner
 * @param itemId The unique ID of the item to delete
 * @param inventory_db Database connection to delete the item from
 * @return True if the item was successfully deleted
 */
bool GCNetwork_Inventory::DeleteItem(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("DeleteItem: Database connection is null");
        return false;
    }

    // Fetch the item before deleting it, so we can send its information
    CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);
    if (!item)
    {
        logger::error("DeleteItem: Item %llu not found or doesn't belong to user %llu",
                      itemId, steamId);
        return false;
    }

    // Delete from database
    char query[256];
    snprintf(query, sizeof(query),
             "DELETE FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
             itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

    if (mysql_query(inventory_db, query) != 0)
    {
        logger::error("DeleteItem: MySQL delete query failed: %s", mysql_error(inventory_db));
        delete item;
        return false;
    }

    if (mysql_affected_rows(inventory_db) == 0)
    {
        logger::warning("DeleteItem: No rows affected when deleting item %llu", itemId);
        delete item;
        return false;
    }

    logger::info("DeleteItem: Successfully deleted item %llu from database", itemId);

    if (p2psocket != 0)
    {
        logger::info("DeleteItem: Sending delete notification for item %llu to player %llu",
                     itemId, steamId);

        bool success = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item, k_EMsgGC_CC_DeleteItem);
        if (!success)
        {
            logger::error("DeleteItem: Failed to send delete notification to client");
            delete item;
            return false;
        }
    }

    delete item;
    return true;
}

/**
 * Sends a single object update to the client
 *
 * @param p2psocket The socket to send the message on
 * @param steamId The steam ID of the player to update
 * @param type_id The type of object being updated (e.g. SOTypeItem)
 * @param object The protobuf object to send
 * @param messageType The message type to use (defaults to k_EMsgGC_CC_GC2CL_SOSingleObject)
 * @return True if message was sent successfully
 */
bool GCNetwork_Inventory::SendSOSingleObject(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    SOTypeId type,
    const google::protobuf::MessageLite &object,
    uint32_t messageType)
{
    CMsgSOSingleObject message;

    // Set the type ID
    message.set_type_id(type);

    // Serialize the object to binary and set as object_data
    message.set_object_data(object.SerializeAsString());

    // Set the version
    message.set_version(InventoryVersion);

    // Set the owner ID
    auto *owner = message.mutable_owner_soid();
    owner->set_type(SoIdTypeSteamId);
    owner->set_id(steamId);

    // Create a network message and send it
    NetworkMessage responseMsg = NetworkMessage::FromProto(message, messageType);

    logger::info("SendSOSingleObject: Sending object of type %d to %llu with message type %u, size: %u bytes",
                 type, steamId, messageType, responseMsg.GetTotalSize());

    bool success = responseMsg.WriteToSocket(p2psocket, true);
    if (!success)
    {
        logger::error("SendSOSingleObject: Failed to write message to socket - client likely disconnected");
    }

    return success;
}

/**
 * Helper to add an object to a multiple objects message
 *
 * @param message The multiple objects message to add to
 * @param type The type of object to add
 * @param object The object to serialize and add
 * @param collection Which collection to add to (added, modified, removed)
 */
void GCNetwork_Inventory::AddToMultipleObjectsMessage(
    CMsgSOMultipleObjects &message,
    SOTypeId type,
    const google::protobuf::MessageLite &object,
    const std::string &collection)
{
    CMsgSOMultipleObjects::SingleObject *single;

    if (collection == "added")
    {
        single = message.add_objects_added();
    }
    else if (collection == "removed")
    {
        single = message.add_objects_removed();
    }
    else
    {
        // Default to "modified"
        single = message.add_objects_modified();
    }

    single->set_type_id(type);
    single->set_object_data(object.SerializeAsString());
}

/**
 * Initialize a multiple objects message with owner ID and version
 *
 * @param message The message to initialize
 * @param steamId The steam ID of the owner
 */
void GCNetwork_Inventory::InitMultipleObjectsMessage(CMsgSOMultipleObjects &message, uint64_t steamId)
{
    message.set_version(InventoryVersion);
    auto *owner = message.mutable_owner_soid();
    owner->set_type(SoIdTypeSteamId);
    owner->set_id(steamId);
}

/**
 * Sends a multiple objects update to the client
 *
 * @param p2psocket The socket to send the message on
 * @param message The prepared multiple objects message
 * @return True if message was sent successfully
 */
bool GCNetwork_Inventory::SendSOMultipleObjects(SNetSocket_t p2psocket, const CMsgSOMultipleObjects &message)
{
    // Create a network message and send it
    NetworkMessage responseMsg = NetworkMessage::FromProto(message, k_EMsgGC_CC_GC2CL_SOMultipleObjects);

    uint32_t totalModified = message.objects_modified_size();
    uint32_t totalAdded = message.objects_added_size();
    uint32_t totalRemoved = message.objects_removed_size();

    logger::info("SendSOMultipleObjects: Sending update with %u modified, %u added, %u removed objects",
                 totalModified, totalAdded, totalRemoved);
    logger::info("SendSOMultipleObjects: Total message size: %u bytes", responseMsg.GetTotalSize());

    bool success = responseMsg.WriteToSocket(p2psocket, true);
    if (!success)
    {
        logger::error("SendSOMultipleObjects: Failed to write message to socket - client likely disconnected");
    }

    return success;
}

// DEFAULT ITEM HANDLING

/**
 * Creates a base weapon item with default properties
 *
 * @param defIndex The definition index of the weapon to create
 * @param steamId The steam ID of the owner
 * @param inventory_db Database connection to save the item
 * @param saveToDb Whether to save the item to the database immediately (default: true)
 * @param customName Optional custom name for the weapon
 * @return A pointer to the newly created CSOEconItem (caller is responsible for freeing memory)
 *         or nullptr if creation failed
 */
CSOEconItem *GCNetwork_Inventory::CreateBaseItem(
    uint32_t defIndex,
    uint64_t steamId,
    MYSQL *inventory_db,
    bool saveToDb,
    const std::string &customName)
{
    if (!g_itemSchema)
    {
        logger::error("CreateBaseItem: ItemSchema is null");
        return nullptr;
    }

    // Create the base item
    CSOEconItem *item = new CSOEconItem();

    // Set basic properties
    item->set_account_id(steamId & 0xFFFFFFFF);
    item->set_def_index(defIndex);
    item->set_inventory(0);
    item->set_level(1);
    item->set_quantity(1);
    item->set_quality(ItemSchema::QualityNormal);
    item->set_flags(0);
    item->set_origin(kEconItemOrigin_Purchased);
    item->set_rarity(ItemSchema::RarityDefault);

    // Set custom name if provided
    if (!customName.empty())
    {
        item->set_custom_name(customName);
    }

    // If saving to database is requested
    if (saveToDb && inventory_db != nullptr)
    {
        uint64_t newItemId = SaveNewItemToDatabase(*item, steamId, inventory_db, true); // Pass true for isBaseWeapon
        if (newItemId == 0)
        {
            logger::error("CreateBaseItem: Failed to save base item to database (defIndex: %u)", defIndex);
            delete item;
            return nullptr;
        }

        // Set the newly assigned ID
        item->set_id(newItemId);
        logger::info("CreateBaseItem: Created base item with defIndex %u, ID %llu for player %llu",
                     defIndex, newItemId, steamId);
    }
    else
    {
        logger::info("CreateBaseItem: Created unsaved base item with defIndex %u for player %llu",
                     defIndex, steamId);
    }

    return item;
}

bool GCNetwork_Inventory::IsDefaultItemId(uint64_t itemId, uint32_t &defIndex, uint32_t &paintKitIndex)
{
    if ((itemId & ItemIdDefaultItemMask) == ItemIdDefaultItemMask)
    {
        defIndex = itemId & 0xffff;
        paintKitIndex = (itemId >> 16) & 0xffff;
        return true;
    }
    return false;
}

// EQUIPS

/**
 * Equips an item to a specific class (CT/T) and slot
 * (this function is so fucked)
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item to equip
 * @param classId The class ID to equip for (CLASS_CT or CLASS_T)
 * @param slotId The slot ID to equip to
 * @param inventory_db Database connection to update
 * @return True if the item was successfully equipped
 */
bool GCNetwork_Inventory::EquipItem(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    uint32_t classId,
    uint32_t slotId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("EquipItem: Database connection is null");
        return false;
    }

    // Special case for unequipping
    if (slotId == 0xFFFFFFFF || slotId == 65535)
    {
        return UnequipItem(p2psocket, steamId, itemId, inventory_db);
    }

    // Check if this is a default item ID
    uint32_t defIndex = 0, paintKitIndex = 0;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        logger::info("EquipItem: Handling default item with defIndex %u, paintKitIndex %u for player %llu",
                     defIndex, paintKitIndex, steamId);

        // Begin transaction
        if (mysql_query(inventory_db, "START TRANSACTION") != 0)
        {
            logger::error("EquipItem: Failed to start transaction: %s", mysql_error(inventory_db));
            return false;
        }

        try
        {
            // First, unequip any items in this slot for this class
            if (!UnequipItemsInSlot(steamId, classId, slotId, inventory_db))
            {
                logger::warning("EquipItem: Failed to unequip items in slot %u for class %u", slotId, classId);
                // Continue anyway, this isn't fatal
            }

            // Special handling for items that share slots with default items
            if (defIndex == 16 && slotId == 15) // M4A4 (which conflicts with M4A1-S)
            {
                // Unset the default M4A1-S if it's currently equipped
                char unsetQuery[512];
                snprintf(unsetQuery, sizeof(unsetQuery),
                         "UPDATE csgo_defaultequips SET default_m4a1s_ct = 0 WHERE owner_id = %llu AND default_m4a1s_ct = 1",
                         steamId);

                if (mysql_query(inventory_db, unsetQuery) != 0)
                {
                    logger::error("EquipItem: Failed to unset M4A1-S default equip: %s", mysql_error(inventory_db));
                    // Continue anyway, not fatal
                }
                else
                {
                    int affected = mysql_affected_rows(inventory_db);
                    if (affected > 0)
                    {
                        logger::info("EquipItem: Unset M4A1-S default equip for player %llu", steamId);
                    }
                }
            }
            else if (defIndex == 1 && slotId == 6) // Desert Eagle (conflicts with R8)
            {
                // Unset R8 for both CT and T
                char unsetQuery[512];
                snprintf(unsetQuery, sizeof(unsetQuery),
                         "UPDATE csgo_defaultequips SET default_r8_ct = 0, default_r8_t = 0 WHERE owner_id = %llu AND (default_r8_ct = 1 OR default_r8_t = 1)",
                         steamId);

                if (mysql_query(inventory_db, unsetQuery) != 0)
                {
                    logger::error("EquipItem: Failed to unset R8 default equip: %s", mysql_error(inventory_db));
                }
                else
                {
                    int affected = mysql_affected_rows(inventory_db);
                    if (affected > 0)
                    {
                        logger::info("EquipItem: Unset R8 default equip for player %llu", steamId);
                    }
                }
            }
            else if (defIndex == 3 && slotId == 5) // Five-SeveN (conflicts with CZ75 for CT)
            {
                if (classId == CLASS_CT)
                {
                    char unsetQuery[512];
                    snprintf(unsetQuery, sizeof(unsetQuery),
                             "UPDATE csgo_defaultequips SET default_cz75_ct = 0 WHERE owner_id = %llu AND default_cz75_ct = 1",
                             steamId);

                    if (mysql_query(inventory_db, unsetQuery) != 0)
                    {
                        logger::error("EquipItem: Failed to unset CZ75 CT default equip: %s", mysql_error(inventory_db));
                    }
                    else
                    {
                        int affected = mysql_affected_rows(inventory_db);
                        if (affected > 0)
                        {
                            logger::info("EquipItem: Unset CZ75 CT default equip for player %llu", steamId);
                        }
                    }
                }
            }
            else if (defIndex == 30 && slotId == 5) // Tec-9 (conflicts with CZ75 for T)
            {
                if (classId == CLASS_T)
                {
                    char unsetQuery[512];
                    snprintf(unsetQuery, sizeof(unsetQuery),
                             "UPDATE csgo_defaultequips SET default_cz75_t = 0 WHERE owner_id = %llu AND default_cz75_t = 1",
                             steamId);

                    if (mysql_query(inventory_db, unsetQuery) != 0)
                    {
                        logger::error("EquipItem: Failed to unset CZ75 T default equip: %s", mysql_error(inventory_db));
                    }
                    else
                    {
                        int affected = mysql_affected_rows(inventory_db);
                        if (affected > 0)
                        {
                            logger::info("EquipItem: Unset CZ75 T default equip for player %llu", steamId);
                        }
                    }
                }
            }
            else if (defIndex == 4 && slotId == 2) // Glock (conflicts with USP-S for T)
            {
                // Glocks are only for T, so no need to check classId
                char unsetQuery[512];
                snprintf(unsetQuery, sizeof(unsetQuery),
                         "UPDATE csgo_defaultequips SET default_usp_ct = 0 WHERE owner_id = %llu AND default_usp_ct = 1",
                         steamId);

                if (mysql_query(inventory_db, unsetQuery) != 0)
                {
                    logger::error("EquipItem: Failed to unset USP-S default equip: %s", mysql_error(inventory_db));
                }
                else
                {
                    int affected = mysql_affected_rows(inventory_db);
                    if (affected > 0)
                    {
                        logger::info("EquipItem: Unset USP-S default equip for player %llu", steamId);
                    }
                }
            }
            else if (defIndex == 32 && slotId == 2) // P2000 (conflicts with USP-S for CT)
            {
                // P2000 is for CT, so no need to check classId
                char unsetQuery[512];
                snprintf(unsetQuery, sizeof(unsetQuery),
                         "UPDATE csgo_defaultequips SET default_usp_ct = 0 WHERE owner_id = %llu AND default_usp_ct = 1",
                         steamId);

                if (mysql_query(inventory_db, unsetQuery) != 0)
                {
                    logger::error("EquipItem: Failed to unset USP-S default equip: %s", mysql_error(inventory_db));
                }
                else
                {
                    int affected = mysql_affected_rows(inventory_db);
                    if (affected > 0)
                    {
                        logger::info("EquipItem: Unset USP-S default equip for player %llu", steamId);
                    }
                }
            }

            // Handle default equip state in database
            char query[512];

            // Determine which column to update based on what's being equipped
            const char *columnName = nullptr;
            if (defIndex == 61 && slotId == 2) // USP-S
                columnName = "default_usp_ct";
            else if (defIndex == 60 && slotId == 15) // M4A1-S
                columnName = "default_m4a1s_ct";
            else if (defIndex == 64 && slotId == 6)
            { // R8 Revolver
                if (classId == CLASS_CT)
                    columnName = "default_r8_ct";
                else if (classId == CLASS_T)
                    columnName = "default_r8_t";
            }
            else if (defIndex == 63 && slotId == 5)
            { // CZ75-Auto
                if (classId == CLASS_CT)
                    columnName = "default_cz75_ct";
                else if (classId == CLASS_T)
                    columnName = "default_cz75_t";
            }

            if (columnName)
            {
                // First ensure the row exists
                snprintf(query, sizeof(query),
                         "INSERT IGNORE INTO csgo_defaultequips (owner_id) VALUES (%llu)",
                         steamId);

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("EquipItem: Failed to ensure defaultequips exists: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                // Update the appropriate column
                snprintf(query, sizeof(query),
                         "UPDATE csgo_defaultequips SET %s = 1 WHERE owner_id = %llu",
                         columnName, steamId);

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("EquipItem: Failed to update default equip state: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }
            }

            // Commit the transaction
            if (mysql_query(inventory_db, "COMMIT") != 0)
            {
                logger::error("EquipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
                mysql_query(inventory_db, "ROLLBACK");
                return false;
            }

            // Create and send default equip update to client
            CMsgSOMultipleObjects updateMsg;
            InitMultipleObjectsMessage(updateMsg, steamId);

            // Create the default equip object
            CSOEconDefaultEquippedDefinitionInstanceClient defaultEquip;
            defaultEquip.set_account_id(steamId & 0xFFFFFFFF);
            defaultEquip.set_item_definition(defIndex);
            defaultEquip.set_class_id(classId);
            defaultEquip.set_slot_id(slotId);

            // Add to the message
            AddToMultipleObjectsMessage(updateMsg, SOTypeDefaultEquippedDefinitionInstanceClient, defaultEquip);

            // If we unequipped any items, they'll be included in the update as well

            // Send the update
            bool success = SendSOMultipleObjects(p2psocket, updateMsg);

            logger::info("EquipItem: Successfully equipped default item with defIndex %u to slot %u for class %u (player %llu)",
                         defIndex, slotId, classId, steamId);

            return success;
        }
        catch (const std::exception &e)
        {
            logger::error("EquipItem: Exception caught: %s", e.what());
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }
        catch (...)
        {
            logger::error("EquipItem: Unknown exception caught");
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }
    }

    // Begin transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("EquipItem: Failed to start transaction: %s", mysql_error(inventory_db));
        return false;
    }

    bool success = false;

    try
    {
        // First, unequip any items in this slot for this class
        if (!UnequipItemsInSlot(steamId, classId, slotId, inventory_db))
        {
            logger::warning("EquipItem: Failed to unequip items in slot %u for class %u", slotId, classId);
            // Continue anyway, this isn't fatal
        }

        // Special handling for regular items that might replace default items
        if (slotId == 15) // M4A4/M4A1-S slot
        {
            // Unset M4A1-S default equip if it's set
            char unsetQuery[512];
            snprintf(unsetQuery, sizeof(unsetQuery),
                     "UPDATE csgo_defaultequips SET default_m4a1s_ct = 0 WHERE owner_id = %llu AND default_m4a1s_ct = 1",
                     steamId);

            if (mysql_query(inventory_db, unsetQuery) != 0)
            {
                logger::error("EquipItem: Failed to unset M4A1-S default equip: %s", mysql_error(inventory_db));
            }
            else
            {
                int affected = mysql_affected_rows(inventory_db);
                if (affected > 0)
                {
                    logger::info("EquipItem: Unset M4A1-S default equip for player %llu", steamId);
                }
            }
        }
        else if (slotId == 6) // Deagle/R8 slot
        {
            // Unset R8 default equips
            char unsetQuery[512];
            snprintf(unsetQuery, sizeof(unsetQuery),
                     "UPDATE csgo_defaultequips SET default_r8_ct = 0, default_r8_t = 0 WHERE owner_id = %llu AND (default_r8_ct = 1 OR default_r8_t = 1)",
                     steamId);

            if (mysql_query(inventory_db, unsetQuery) != 0)
            {
                logger::error("EquipItem: Failed to unset R8 default equips: %s", mysql_error(inventory_db));
            }
            else
            {
                int affected = mysql_affected_rows(inventory_db);
                if (affected > 0)
                {
                    logger::info("EquipItem: Unset R8 default equips for player %llu", steamId);
                }
            }
        }
        else if (slotId == 5) // Five-SeveN/Tec-9/CZ75 slot
        {
            // Unset CZ75 for the appropriate team
            const char *columnName = (classId == CLASS_CT) ? "default_cz75_ct" : "default_cz75_t";

            char unsetQuery[512];
            snprintf(unsetQuery, sizeof(unsetQuery),
                     "UPDATE csgo_defaultequips SET %s = 0 WHERE owner_id = %llu AND %s = 1",
                     columnName, steamId, columnName);

            if (mysql_query(inventory_db, unsetQuery) != 0)
            {
                logger::error("EquipItem: Failed to unset CZ75 default equip: %s", mysql_error(inventory_db));
            }
            else
            {
                int affected = mysql_affected_rows(inventory_db);
                if (affected > 0)
                {
                    logger::info("EquipItem: Unset CZ75 default equip for player %llu", steamId);
                }
            }
        }
        else if (slotId == 2) // Glock/P2000/USP-S slot
        {
            // Unset USP-S default equip
            char unsetQuery[512];
            snprintf(unsetQuery, sizeof(unsetQuery),
                     "UPDATE csgo_defaultequips SET default_usp_ct = 0 WHERE owner_id = %llu AND default_usp_ct = 1",
                     steamId);

            if (mysql_query(inventory_db, unsetQuery) != 0)
            {
                logger::error("EquipItem: Failed to unset USP-S default equip: %s", mysql_error(inventory_db));
            }
            else
            {
                int affected = mysql_affected_rows(inventory_db);
                if (affected > 0)
                {
                    logger::info("EquipItem: Unset USP-S default equip for player %llu", steamId);
                }
            }
        }

        // Now equip the new item
        const char *column = (classId == CLASS_CT) ? "equipped_ct" : "equipped_t";

        char query[512];
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET %s = 1 WHERE id = %llu AND owner_steamid2 = '%s'",
                 column, itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("EquipItem: MySQL update query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Check if the item was found and updated
        if (mysql_affected_rows(inventory_db) == 0)
        {
            logger::error("EquipItem: Item %llu not found or already equipped for player %llu",
                          itemId, steamId);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Commit the transaction
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("EquipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Send the update to the client
        success = SendEquipUpdate(p2psocket, steamId, itemId, classId, slotId, inventory_db);

        logger::info("EquipItem: Successfully equipped item %llu to slot %u for class %u (player %llu)",
                     itemId, slotId, classId, steamId);
    }
    catch (const std::exception &e)
    {
        logger::error("EquipItem: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
    catch (...)
    {
        logger::error("EquipItem: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }

    return success;
}

/**
 * Unequips an item from all classes (CT/T)
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item to unequip
 * @param inventory_db Database connection to update
 * @return True if the item was successfully unequipped
 */
bool GCNetwork_Inventory::UnequipItem(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("UnequipItem: Database connection is null");
        return false;
    }

    // Check if this is a default item ID
    uint32_t defIndex = 0, paintKitIndex = 0;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        logger::info("UnequipItem: Handling default item with defIndex %u for player %llu", defIndex, steamId);

        // Begin transaction
        if (mysql_query(inventory_db, "START TRANSACTION") != 0)
        {
            logger::error("UnequipItem: Failed to start transaction: %s", mysql_error(inventory_db));
            return false;
        }

        try
        {
            // Determine which column to update based on the defIndex
            const char *columnName = nullptr;
            if (defIndex == 61) // USP-S
                columnName = "default_usp_ct";
            else if (defIndex == 60) // M4A1-S
                columnName = "default_m4a1s_ct";
            else if (defIndex == 64)
            { // R8 Revolver
                // Need to disable both CT and T equips
                char query[512];
                snprintf(query, sizeof(query),
                         "UPDATE csgo_defaultequips SET default_r8_ct = 0, default_r8_t = 0 WHERE owner_id = %llu",
                         steamId);

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("UnequipItem: Failed to update R8 default equip state: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                // Create and send default equip update to client
                CMsgSOMultipleObjects updateMsg;
                InitMultipleObjectsMessage(updateMsg, steamId);

                // Send the update
                bool success = SendSOMultipleObjects(p2psocket, updateMsg);

                if (mysql_query(inventory_db, "COMMIT") != 0)
                {
                    logger::error("UnequipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                logger::info("UnequipItem: Successfully unequipped default R8 for player %llu", steamId);
                return success;
            }
            else if (defIndex == 63)
            { // CZ75-Auto
                // Need to disable both CT and T equips
                char query[512];
                snprintf(query, sizeof(query),
                         "UPDATE csgo_defaultequips SET default_cz75_ct = 0, default_cz75_t = 0 WHERE owner_id = %llu",
                         steamId);

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("UnequipItem: Failed to update CZ75 default equip state: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                // Create and send default equip update to client
                CMsgSOMultipleObjects updateMsg;
                InitMultipleObjectsMessage(updateMsg, steamId);

                // Send the update
                bool success = SendSOMultipleObjects(p2psocket, updateMsg);

                if (mysql_query(inventory_db, "COMMIT") != 0)
                {
                    logger::error("UnequipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                logger::info("UnequipItem: Successfully unequipped default CZ75 for player %llu", steamId);
                return success;
            }

            if (columnName)
            {
                char query[512];
                snprintf(query, sizeof(query),
                         "UPDATE csgo_defaultequips SET %s = 0 WHERE owner_id = %llu",
                         columnName, steamId);

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("UnequipItem: Failed to update default equip state: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                // Create and send default equip update to client
                CMsgSOMultipleObjects updateMsg;
                InitMultipleObjectsMessage(updateMsg, steamId);

                // Send the update
                bool success = SendSOMultipleObjects(p2psocket, updateMsg);

                if (mysql_query(inventory_db, "COMMIT") != 0)
                {
                    logger::error("UnequipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                logger::info("UnequipItem: Successfully unequipped default item with defIndex %u for player %llu",
                             defIndex, steamId);
                return success;
            }

            // If we reached here, we couldn't figure out which default item this is
            logger::error("UnequipItem: Unrecognized default item with defIndex %u for player %llu",
                          defIndex, steamId);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }
        catch (const std::exception &e)
        {
            logger::error("UnequipItem: Exception caught: %s", e.what());
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }
        catch (...)
        {
            logger::error("UnequipItem: Unknown exception caught");
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }
    }

    // Begin transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("UnequipItem: Failed to start transaction: %s", mysql_error(inventory_db));
        return false;
    }

    bool success = false;

    try
    {
        // Get current item state and class/slot information
        char query[512];
        snprintf(query, sizeof(query),
                 "SELECT equipped_ct, equipped_t, id, item_id FROM csgo_items "
                 "WHERE id = %llu AND owner_steamid2 = '%s'",
                 itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("UnequipItem: MySQL select query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (!result || mysql_num_rows(result) == 0)
        {
            logger::error("UnequipItem: Item %llu not found for player %llu", itemId, steamId);
            if (result)
                mysql_free_result(result);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        bool was_equipped_ct = row[0] && atoi(row[0]) == 1;
        bool was_equipped_t = row[1] && atoi(row[1]) == 1;

        // Parse the item to get def_index for determining slot
        uint32_t def_index = 0, paint_index = 0;
        if (row[3] && !ParseItemId(row[3], def_index, paint_index))
        {
            logger::error("UnequipItem: Failed to parse item_id: %s", row[3] ? row[3] : "null");
            mysql_free_result(result);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        mysql_free_result(result);

        // Update the database to unequip the item from all classes
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET equipped_ct = 0, equipped_t = 0 "
                 "WHERE id = %llu AND owner_steamid2 = '%s'",
                 itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("UnequipItem: MySQL update query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Commit the transaction
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("UnequipItem: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Send the update to the client
        success = SendUnequipUpdate(p2psocket, steamId, itemId, inventory_db, was_equipped_ct, was_equipped_t, def_index);

        logger::info("UnequipItem: Successfully unequipped item %llu for player %llu", itemId, steamId);
    }
    catch (const std::exception &e)
    {
        logger::error("UnequipItem: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
    catch (...)
    {
        logger::error("UnequipItem: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }

    return success;
}

/**
 * Unequips all items in a specific slot for a class
 *
 * @param steamId The steam ID of the player
 * @param classId The class ID (CLASS_CT or CLASS_T)
 * @param slotId The slot ID to unequip
 * @param inventory_db Database connection to update
 * @return True if successful
 */
bool GCNetwork_Inventory::UnequipItemsInSlot(
    uint64_t steamId,
    uint32_t classId,
    uint32_t slotId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("UnequipItemsInSlot: Database connection is null");
        return false;
    }

    // Update the appropriate column based on classId
    const char *column = (classId == CLASS_CT) ? "equipped_ct" : "equipped_t";
    char query[1024];

    // Check if this is for collectibles (slot 55) or music kits (slot 54)
    if (slotId == 55)
    {
        // Special query for collectibles using "collectible-" prefix
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET %s = 0 "
                 "WHERE owner_steamid2 = '%s' AND %s = 1 AND "
                 "item_id LIKE 'collectible-%%'",
                 column, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str(), column);
    }
    else if (slotId == 54)
    {
        // Special query for music kits using "music_kit-" prefix
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET %s = 0 "
                 "WHERE owner_steamid2 = '%s' AND %s = 1 AND "
                 "item_id LIKE 'music_kit-%%'",
                 column, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str(), column);
    }
    else
    {
        // For other slots, get all defindexes for this slot
        std::vector<uint32_t> defindexes = GetDefindexFromItemSlot(slotId);
        if (defindexes.empty())
        {
            logger::warning("UnequipItemsInSlot: No items defined for slot %u", slotId);
            return true; // Not really an error, just nothing to do
        }

        // Build a query to find items in this slot
        std::string defIndexList = "(";
        for (size_t i = 0; i < defindexes.size(); i++)
        {
            if (i > 0)
                defIndexList += ",";
            defIndexList += "'" + std::to_string(defindexes[i]) + "'";
        }
        defIndexList += ")";

        // Original query for other item types
        snprintf(query, sizeof(query),
                 "UPDATE csgo_items SET %s = 0 "
                 "WHERE owner_steamid2 = '%s' AND %s = 1 AND "
                 "SUBSTRING_INDEX(SUBSTRING_INDEX(item_id, '_', 1), '-', -1) IN %s",
                 column, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str(),
                 column, defIndexList.c_str());
    }

    if (mysql_query(inventory_db, query) != 0)
    {
        logger::error("UnequipItemsInSlot: MySQL update query failed: %s", mysql_error(inventory_db));
        return false;
    }

    int affected = mysql_affected_rows(inventory_db);
    logger::info("UnequipItemsInSlot: Unequipped %d items from slot %u for class %u (player %llu)",
                 affected, slotId, classId, steamId);
    return true;
}

/**
 * Sends an equipment update to the client
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item being equipped
 * @param classId The class ID where the item is equipped
 * @param slotId The slot ID where the item is equipped
 * @param inventory_db Database connection
 * @return True if update was sent successfully
 */
bool GCNetwork_Inventory::SendEquipUpdate(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    uint32_t classId,
    uint32_t slotId,
    MYSQL *inventory_db)
{
    // Fetch the updated item
    CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);
    if (!item)
    {
        logger::error("SendEquipUpdate: Failed to fetch item %llu for update", itemId);
        return false;
    }

    // stupid hack
    if (slotId == 55 || slotId == 54)
    {
        std::string itemIdStr;
        char query[256];
        snprintf(query, sizeof(query),
                 "SELECT item_id FROM csgo_items WHERE id = %llu", itemId);

        if (mysql_query(inventory_db, query) == 0)
        {
            MYSQL_RES *result = mysql_store_result(inventory_db);
            if (result && mysql_num_rows(result) > 0)
            {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row && row[0])
                {
                    itemIdStr = row[0];
                }
            }
            if (result)
                mysql_free_result(result);
        }

        bool isSpecialItem = (itemIdStr.substr(0, 12) == "collectible-") ||
                             (itemIdStr.substr(0, 10) == "music_kit-");

        if (isSpecialItem)
        {
            item->clear_equipped_state();
            auto equipped_state = item->add_equipped_state();
            equipped_state->set_new_class(0);
            equipped_state->set_new_slot(slotId);

            logger::info("SendEquipUpdate: Special handling for collectible/music kit with class 0");
        }
    }

    // Creating multiple objects message
    CMsgSOMultipleObjects updateMsg;
    InitMultipleObjectsMessage(updateMsg, steamId);

    // Add the item to the modified objects list
    AddToMultipleObjectsMessage(updateMsg, SOTypeItem, *item);

    // Send the message
    bool success = SendSOMultipleObjects(p2psocket, updateMsg);

    delete item;
    return success;
}

/**
 * Sends an unequip update to the client
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item being unequipped
 * @param inventory_db Database connection
 * @param was_equipped_ct Whether the item was equipped for CT
 * @param was_equipped_t Whether the item was equipped for T
 * @param def_index The definition index of the item
 * @return True if update was sent successfully
 */
bool GCNetwork_Inventory::SendUnequipUpdate(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    MYSQL *inventory_db,
    bool was_equipped_ct,
    bool was_equipped_t,
    uint32_t def_index)
{
    // Fetch the updated item
    CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);
    if (!item)
    {
        logger::error("SendUnequipUpdate: Failed to fetch item %llu for update", itemId);
        return false;
    }

    // Creating multiple objects message
    CMsgSOMultipleObjects updateMsg;
    InitMultipleObjectsMessage(updateMsg, steamId);

    // Add the item to the modified objects list
    AddToMultipleObjectsMessage(updateMsg, SOTypeItem, *item);

    // Send the message
    bool success = SendSOMultipleObjects(p2psocket, updateMsg);

    delete item;
    return success;
}

// RENAMING ITEMS

/**
 * Handles naming an existing item
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item being named
 * @param name The new name to apply
 * @param inventory_db Database connection
 * @return True if the item was successfully named
 */
bool GCNetwork_Inventory::HandleNameItem(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    const std::string &name,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("HandleNameItem: Database connection is null");
        return false;
    }

    // Verify that the player owns the item
    char ownershipQuery[256];
    snprintf(ownershipQuery, sizeof(ownershipQuery),
             "SELECT id FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
             itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

    if (mysql_query(inventory_db, ownershipQuery) != 0)
    {
        logger::error("HandleNameItem: MySQL query failed: %s", mysql_error(inventory_db));
        return false;
    }

    MYSQL_RES *result = mysql_store_result(inventory_db);
    if (!result || mysql_num_rows(result) == 0)
    {
        logger::error("HandleNameItem: Item %llu not found or not owned by player %llu",
                      itemId, steamId);
        if (result)
            mysql_free_result(result);
        return false;
    }
    mysql_free_result(result);

    std::string escapedName(name.length() * 2 + 1, '\0');
    mysql_real_escape_string(inventory_db, &escapedName[0], name.c_str(), name.length());
    escapedName.resize(strlen(escapedName.c_str()));

    // Update the item's name
    char updateQuery[1024];
    snprintf(updateQuery, sizeof(updateQuery),
             "UPDATE csgo_items SET nametag = '%s' WHERE id = %llu",
             escapedName.c_str(), itemId);

    if (mysql_query(inventory_db, updateQuery) != 0)
    {
        logger::error("HandleNameItem: MySQL update query failed: %s", mysql_error(inventory_db));
        return false;
    }

    // Send updates to the client
    CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);
    if (!item)
    {
        logger::error("HandleNameItem: Failed to fetch updated item");
        return false;
    }

    // Send the updated item
    bool updateSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item);

    delete item;

    logger::info("HandleNameItem: Successfully named item %llu for player %llu", itemId, steamId);
    return updateSent;
}

/**
 * Handles creating a new base item with a name
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param defIndex The definition index of the base item to create
 * @param name The name to apply
 * @param inventory_db Database connection
 * @return True if the base item was successfully created and named
 */
bool GCNetwork_Inventory::HandleNameBaseItem(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint32_t defIndex,
    const std::string &name,
    MYSQL *inventory_db)
{
    if (!inventory_db || !g_itemSchema)
    {
        logger::error("HandleNameBaseItem: Database connection or ItemSchema is null");
        return false;
    }

    // Create the new base item with the custom name
    CSOEconItem *newItem = CreateBaseItem(defIndex, steamId, inventory_db, false, name);
    if (!newItem)
    {
        logger::error("HandleNameBaseItem: Failed to create base item with defIndex %u", defIndex);
        return false;
    }

    // Save the new item to the database
    uint64_t newItemId = SaveNewItemToDatabase(*newItem, steamId, inventory_db, true);
    if (newItemId == 0)
    {
        logger::error("HandleNameBaseItem: Failed to save new base item to database");
        delete newItem;
        return false;
    }

    // Set the new item ID
    newItem->set_id(newItemId);

    // Send the new item to the client
    bool createSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *newItem);

    delete newItem;

    logger::info("HandleNameBaseItem: Successfully created named base item with defIndex %u (ID: %llu) for player %llu",
                 defIndex, newItemId, steamId);
    return createSent;
}

/**
 * Handles removing the name from an item
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param itemId The ID of the item to remove the name from
 * @param inventory_db Database connection
 * @return True if the name was successfully removed
 */
bool GCNetwork_Inventory::HandleRemoveItemName(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    uint64_t itemId,
    MYSQL *inventory_db)
{
    if (!inventory_db)
    {
        logger::error("HandleRemoveItemName: Database connection is null");
        return false;
    }

    // Begin transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("HandleRemoveItemName: Failed to start transaction: %s", mysql_error(inventory_db));
        return false;
    }

    try
    {
        // Verify that the player owns the item and get item info
        char query[512];
        snprintf(query, sizeof(query),
                 "SELECT nametag, item_id, acquired_by, "
                 "(CASE WHEN sticker_1 > 0 OR sticker_2 > 0 OR sticker_3 > 0 OR sticker_4 > 0 OR sticker_5 > 0 "
                 "THEN 1 ELSE 0 END) as has_stickers "
                 "FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
                 itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("HandleRemoveItemName: MySQL query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (!result || mysql_num_rows(result) == 0)
        {
            logger::error("HandleRemoveItemName: Item %llu not found or not owned by player %llu",
                          itemId, steamId);
            if (result)
                mysql_free_result(result);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        bool hasNameTag = (row[0] && strlen(row[0]) > 0);
        std::string itemIdStr = row[1] ? row[1] : "";
        std::string acquiredBy = row[2] ? row[2] : "";
        bool hasStickers = (row[3] && atoi(row[3]) > 0);

        mysql_free_result(result);

        if (!hasNameTag)
        {
            logger::warning("HandleRemoveItemName: Item %llu doesn't have a name tag", itemId);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Check if this is a base item (acquired_by is "default" or item_id ends with "_0_0")
        bool isDefaultAcquired = (acquiredBy == "default");
        bool endsWithZeroZero = false;
        if (itemIdStr.length() > 4)
        {
            endsWithZeroZero = (itemIdStr.substr(itemIdStr.length() - 4) == "_0_0");
        }

        bool isBaseItem = (isDefaultAcquired || endsWithZeroZero);

        // If it's a base item with no stickers, delete it after name removal
        if (isBaseItem && !hasStickers)
        {
            logger::info("HandleRemoveItemName: Item %llu is a base item with no stickers, deleting it", itemId);

            // Fetch the item for sending delete notification
            CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);
            if (!item)
            {
                logger::error("HandleRemoveItemName: Failed to fetch item");
                mysql_query(inventory_db, "ROLLBACK");
                return false;
            }

            // Delete the item from the database
            snprintf(query, sizeof(query),
                     "DELETE FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
                     itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

            if (mysql_query(inventory_db, query) != 0)
            {
                logger::error("HandleRemoveItemName: Failed to delete base item: %s", mysql_error(inventory_db));
                mysql_query(inventory_db, "ROLLBACK");
                delete item;
                return false;
            }

            // Commit transaction
            if (mysql_query(inventory_db, "COMMIT") != 0)
            {
                logger::error("HandleRemoveItemName: Failed to commit transaction: %s", mysql_error(inventory_db));
                mysql_query(inventory_db, "ROLLBACK");
                delete item;
                return false;
            }

            // Send delete notification
            bool deleteSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item, k_EMsgGC_CC_DeleteItem);

            delete item;

            logger::info("HandleRemoveItemName: Successfully deleted base item %llu after name removal for player %llu",
                         itemId, steamId);

            return deleteSent;
        }
        else
        {
            // For non-base items or base items with stickers, just remove the name tag
            snprintf(query, sizeof(query),
                     "UPDATE csgo_items SET nametag = NULL WHERE id = %llu",
                     itemId);

            if (mysql_query(inventory_db, query) != 0)
            {
                logger::error("HandleRemoveItemName: MySQL update query failed: %s", mysql_error(inventory_db));
                mysql_query(inventory_db, "ROLLBACK");
                return false;
            }

            // Commit the transaction
            if (mysql_query(inventory_db, "COMMIT") != 0)
            {
                logger::error("HandleRemoveItemName: Failed to commit transaction: %s", mysql_error(inventory_db));
                mysql_query(inventory_db, "ROLLBACK");
                return false;
            }

            // Send update to client
            CSOEconItem *updatedItem = FetchItemFromDatabase(itemId, steamId, inventory_db);
            if (!updatedItem)
            {
                logger::error("HandleRemoveItemName: Failed to fetch updated item");
                return false;
            }

            bool updateSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *updatedItem);
            delete updatedItem;

            logger::info("HandleRemoveItemName: Successfully removed name from item %llu for player %llu",
                         itemId, steamId);
            return updateSent;
        }
    }
    catch (const std::exception &e)
    {
        logger::error("HandleRemoveItemName: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
    catch (...)
    {
        logger::error("HandleRemoveItemName: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
}

// STICKERS

/**
 * Handles applying a sticker to an item
 * Creates a new base weapon if requested and applies the sticker
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param message The sticker application message
 * @param inventory_db Database connection
 * @return True if sticker was successfully applied
 */
bool GCNetwork_Inventory::HandleApplySticker(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_ApplySticker &message,
    MYSQL *inventory_db)
{
    if (!inventory_db || !g_itemSchema)
    {
        logger::error("HandleApplySticker: Database connection or ItemSchema is null");
        return false;
    }

    logger::info("HandleApplySticker: Processing sticker application from player %llu", steamId);

    // Get sticker item info
    uint64_t stickerItemId = message.sticker_item_id();

    // Verify player owns the sticker
    CSOEconItem *stickerItem = FetchItemFromDatabase(stickerItemId, steamId, inventory_db);
    if (!stickerItem)
    {
        logger::error("HandleApplySticker: Player %llu doesn't own sticker item %llu",
                      steamId, stickerItemId);
        return false;
    }

    // Get the sticker kit id
    uint32_t stickerKitId = 0;
    for (int i = 0; i < stickerItem->attribute_size(); i++)
    {
        const CSOEconItemAttribute &attr = stickerItem->attribute(i);
        if (attr.def_index() == ATTR_ITEM_STICKER_ID)
        {
            stickerKitId = g_itemSchema->AttributeUint32(&attr);
            break;
        }
    }

    if (!stickerKitId)
    {
        logger::error("HandleApplySticker: Sticker item %llu doesn't have valid sticker kit ID",
                      stickerItemId);
        delete stickerItem;
        return false;
    }

    // Target item - either existing or create a new base item
    CSOEconItem *targetItem = nullptr;
    bool createdBaseItem = false;

    // Apply to an existing item
    if (message.has_item_item_id() && message.item_item_id() > 0)
    {
        uint64_t targetItemId = message.item_item_id();
        targetItem = FetchItemFromDatabase(targetItemId, steamId, inventory_db);

        if (!targetItem)
        {
            logger::error("HandleApplySticker: Player %llu doesn't own target item %llu",
                          steamId, targetItemId);
            delete stickerItem;
            return false;
        }
    }
    // Create a new base item and apply sticker to it
    else if (message.has_baseitem_defidx() && message.baseitem_defidx() > 0)
    {
        uint32_t baseItemDefIndex = message.baseitem_defidx();
        // Pass empty string as the custom name parameter
        targetItem = CreateBaseItem(baseItemDefIndex, steamId, inventory_db, true, "");

        if (!targetItem)
        {
            logger::error("HandleApplySticker: Failed to create base item with defIndex %u",
                          baseItemDefIndex);
            delete stickerItem;
            return false;
        }

        createdBaseItem = true;
    }
    else
    {
        logger::error("HandleApplySticker: Invalid request - missing target item ID or base item def_index");
        delete stickerItem;
        return false;
    }

    // Begin database transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("HandleApplySticker: Failed to start transaction: %s", mysql_error(inventory_db));
        delete stickerItem;
        delete targetItem;
        return false;
    }

    try
    {
        // Get sticker slot
        uint32_t stickerSlot = message.has_sticker_slot() ? message.sticker_slot() : 0;

        // Calculate attribute indexes for the sticker slot
        uint32_t stickerIdAttr = 113 + (stickerSlot * 4);
        uint32_t stickerWearAttr = stickerIdAttr + 1;

        // Apply the sticker to the item (add attributes)
        char updateQuery[1024];

        // If base item was just created, we just need to add a record in the sticker column
        if (createdBaseItem)
        {
            // The item was just created, we already have the ID from CreateBaseItem
            snprintf(updateQuery, sizeof(updateQuery),
                     "UPDATE csgo_items SET sticker_%u = %u, sticker_%u_wear = 0.0 "
                     "WHERE id = %llu AND owner_steamid2 = '%s'",
                     stickerSlot + 1, stickerKitId, stickerSlot + 1,
                     targetItem->id(), GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());
        }
        else
        {
            // Existing item - update the sticker in the slot
            snprintf(updateQuery, sizeof(updateQuery),
                     "UPDATE csgo_items SET sticker_%u = %u, sticker_%u_wear = 0.0 "
                     "WHERE id = %llu AND owner_steamid2 = '%s'",
                     stickerSlot + 1, stickerKitId, stickerSlot + 1,
                     targetItem->id(), GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());
        }

        if (mysql_query(inventory_db, updateQuery) != 0)
        {
            logger::error("HandleApplySticker: Failed to update sticker attributes: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            delete stickerItem;
            delete targetItem;
            return false;
        }

        // Delete the sticker item
        char deleteQuery[256];
        snprintf(deleteQuery, sizeof(deleteQuery),
                 "DELETE FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
                 stickerItemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, deleteQuery) != 0)
        {
            logger::error("HandleApplySticker: Failed to delete sticker item: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            delete stickerItem;
            delete targetItem;
            return false;
        }

        // Commit the transaction
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("HandleApplySticker: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            delete stickerItem;
            delete targetItem;
            return false;
        }

        // Get fresh target item after modifications
        CSOEconItem *updatedItem = FetchItemFromDatabase(targetItem->id(), steamId, inventory_db);
        if (!updatedItem)
        {
            logger::error("HandleApplySticker: Failed to fetch updated item");
            delete stickerItem;
            delete targetItem;
            return false;
        }

        // Send update for modified item
        bool updateSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *updatedItem);

        // Send deletion for sticker item
        bool deleteSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *stickerItem, k_EMsgGC_CC_DeleteItem);

        logger::info("HandleApplySticker: Successfully applied sticker %llu to item %llu for player %llu",
                     stickerItemId, targetItem->id(), steamId);

        // Clean up
        delete stickerItem;
        delete targetItem;
        delete updatedItem;

        return updateSent && deleteSent;
    }
    catch (const std::exception &e)
    {
        logger::error("HandleApplySticker: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        delete stickerItem;
        delete targetItem;
        return false;
    }
    catch (...)
    {
        logger::error("HandleApplySticker: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        delete stickerItem;
        delete targetItem;
        return false;
    }
}

/**
 * Handles scraping a sticker from an item
 * If wear reaches max, removes the sticker entirely
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param message The sticker scrape message
 * @param inventory_db Database connection
 * @return True if sticker was successfully scraped
 */
bool GCNetwork_Inventory::HandleScrapeSticker(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_ApplySticker &message,
    MYSQL *inventory_db)
{
    if (!inventory_db || !g_itemSchema)
    {
        logger::error("HandleScrapeSticker: Database connection or ItemSchema is null");
        return false;
    }

    // Must have a target item
    if (!message.has_item_item_id() || message.item_item_id() == 0)
    {
        logger::error("HandleScrapeSticker: Missing target item ID");
        return false;
    }

    uint64_t itemId = message.item_item_id();
    uint32_t stickerSlot = message.has_sticker_slot() ? message.sticker_slot() : 0;

    logger::info("HandleScrapeSticker: Processing sticker scrape for item %llu, slot %u from player %llu",
                 itemId, stickerSlot, steamId);

    // Begin transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("HandleScrapeSticker: Failed to start transaction: %s", mysql_error(inventory_db));
        return false;
    }

    try
    {
        // First, check if the item exists and get current sticker info
        char query[512];
        snprintf(query, sizeof(query),
                 "SELECT id, sticker_%u, sticker_%u_wear, item_id FROM csgo_items "
                 "WHERE id = %llu AND owner_steamid2 = '%s'",
                 stickerSlot + 1, stickerSlot + 1,
                 itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("HandleScrapeSticker: MySQL query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (!result || mysql_num_rows(result) == 0)
        {
            logger::error("HandleScrapeSticker: Item %llu not found for player %llu",
                          itemId, steamId);
            if (result)
                mysql_free_result(result);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);

        // Check if there's a sticker in this slot
        bool hasStickerInSlot = (row[1] && atoi(row[1]) > 0);
        if (!hasStickerInSlot)
        {
            logger::error("HandleScrapeSticker: No sticker in slot %u for item %llu",
                          stickerSlot, itemId);
            mysql_free_result(result);
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Get current wear and calculate new wear
        float currentWear = row[2] ? std::stof(row[2]) : 0.0f;
        // Increment wear by 1/9 (as in client code)
        float wearIncrement = 1.0f / 9.0f;
        float newWear = currentWear + wearIncrement;

        // Get item_id and parse it to extract def_index
        std::string itemIdStr = row[3] ? row[3] : "";
        uint32_t defIndex = 0;
        uint32_t paintIndex = 0;
        if (!itemIdStr.empty())
        {
            if (!ParseItemId(itemIdStr, defIndex, paintIndex))
            {
                logger::warning("HandleScrapeSticker: Failed to parse item_id: %s", itemIdStr.c_str());
            }
        }

        // Check if this is a base weapon with a sticker
        bool isBaseItem = false;
        if (itemIdStr.length() >= 5)
        {
            size_t first_underscore = itemIdStr.find('_');
            size_t second_underscore = itemIdStr.find('_', first_underscore + 1);
            if (first_underscore != std::string::npos && second_underscore != std::string::npos)
            {
                std::string paintIndexStr = itemIdStr.substr(first_underscore + 1, second_underscore - first_underscore - 1);
                int paintIndex = std::stoi(paintIndexStr);
                if (paintIndex == 0)
                {
                    isBaseItem = true;
                }
            }
        }

        mysql_free_result(result);

        // If wear exceeds 1.0, remove the sticker entirely
        if (newWear > 1.0f)
        {
            // If this is a base weapon with only this sticker/no nametag, delete the entire item
            if (isBaseItem)
            {
                snprintf(query, sizeof(query),
                         "SELECT nametag, acquired_by, "
                         "(CASE WHEN (sticker_1 > 0 AND sticker_1 != sticker_%u) OR "
                         "(sticker_2 > 0 AND sticker_2 != sticker_%u) OR "
                         "(sticker_3 > 0 AND sticker_3 != sticker_%u) OR "
                         "(sticker_4 > 0 AND sticker_4 != sticker_%u) OR "
                         "(sticker_5 > 0 AND sticker_5 != sticker_%u) THEN 1 ELSE 0 END) as has_other_stickers "
                         "FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
                         stickerSlot + 1, stickerSlot + 1, stickerSlot + 1, stickerSlot + 1, stickerSlot + 1,
                         itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

                if (mysql_query(inventory_db, query) != 0)
                {
                    logger::error("HandleScrapeSticker: MySQL query failed: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                result = mysql_store_result(inventory_db);
                row = mysql_fetch_row(result);

                bool hasNameTag = (row[0] != NULL && strlen(row[0]) > 0);
                bool isDefaultAcquired = (row[1] != NULL && strcmp(row[1], "default") == 0);
                bool hasOtherStickers = (row[2] != NULL && atoi(row[2]) > 0);

                mysql_free_result(result);

                // Is it a base item?
                bool endsWithZeroZero = false;
                if (itemIdStr.length() > 4)
                {
                    endsWithZeroZero = (itemIdStr.substr(itemIdStr.length() - 4) == "_0_0");
                }

                if ((isDefaultAcquired || endsWithZeroZero) && !hasNameTag && !hasOtherStickers)
                {
                    // If this is a base item with no name tag and this is the only sticker, delete it
                    CSOEconItem *item = FetchItemFromDatabase(itemId, steamId, inventory_db);

                    // Delete the item from the database
                    snprintf(query, sizeof(query),
                             "DELETE FROM csgo_items WHERE id = %llu AND owner_steamid2 = '%s'",
                             itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());

                    if (mysql_query(inventory_db, query) != 0)
                    {
                        logger::error("HandleScrapeSticker: Failed to delete base item: %s", mysql_error(inventory_db));
                        mysql_query(inventory_db, "ROLLBACK");
                        delete item;
                        return false;
                    }

                    // Commit transaction
                    if (mysql_query(inventory_db, "COMMIT") != 0)
                    {
                        logger::error("HandleScrapeSticker: Failed to commit transaction: %s", mysql_error(inventory_db));
                        mysql_query(inventory_db, "ROLLBACK");
                        delete item;
                        return false;
                    }

                    // Send delete notification
                    bool deleteSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *item, k_EMsgGC_CC_DeleteItem);

                    delete item;

                    logger::info("HandleScrapeSticker: Removed last sticker and deleted base item %llu for player %llu",
                                 itemId, steamId);

                    return deleteSent;
                }
                else
                {
                    logger::info("HandleScrapeSticker: Not deleting base item %llu - hasNameTag: %d, hasOtherStickers: %d",
                                 itemId, hasNameTag, hasOtherStickers);
                }
            }

            // Just remove the sticker from this slot
            snprintf(query, sizeof(query),
                     "UPDATE csgo_items SET sticker_%u = NULL, sticker_%u_wear = NULL "
                     "WHERE id = %llu AND owner_steamid2 = '%s'",
                     stickerSlot + 1, stickerSlot + 1,
                     itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());
        }
        else
        {
            // Just increase the wear value
            snprintf(query, sizeof(query),
                     "UPDATE csgo_items SET sticker_%u_wear = %f "
                     "WHERE id = %llu AND owner_steamid2 = '%s'",
                     stickerSlot + 1, newWear,
                     itemId, GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str());
        }

        if (mysql_query(inventory_db, query) != 0)
        {
            logger::error("HandleScrapeSticker: MySQL update query failed: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Commit transaction
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("HandleScrapeSticker: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        // Get updated item
        CSOEconItem *updatedItem = FetchItemFromDatabase(itemId, steamId, inventory_db);
        if (!updatedItem)
        {
            logger::error("HandleScrapeSticker: Failed to fetch updated item");
            return false;
        }

        // Send update notification
        bool updateSent = SendSOSingleObject(p2psocket, steamId, SOTypeItem, *updatedItem);

        delete updatedItem;

        logger::info("HandleScrapeSticker: Successfully %s sticker in slot %u for item %llu (player %llu)",
                     (newWear > 1.0f) ? "removed" : "scraped",
                     stickerSlot, itemId, steamId);

        return updateSent;
    }
    catch (const std::exception &e)
    {
        logger::error("HandleScrapeSticker: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
    catch (...)
    {
        logger::error("HandleScrapeSticker: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
}

/**
 * Main entry point for handling sticker application/scraping requests
 *
 * @param p2psocket The socket to send updates to
 * @param steamId The steam ID of the player
 * @param message The sticker message from the client
 * @param inventory_db Database connection
 * @return True if the operation was successful
 */
bool GCNetwork_Inventory::ProcessStickerAction(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_ApplySticker &message,
    MYSQL *inventory_db)
{
    // scrape
    if (!message.has_sticker_item_id() || message.sticker_item_id() == 0)
    {
        return HandleScrapeSticker(p2psocket, steamId, message, inventory_db);
    }
    else
    {
        return HandleApplySticker(p2psocket, steamId, message, inventory_db);
    }
}

// PURCHASES

/**
 * Handles store purchase initialization requests from clients
 *
 * @param p2psocket The socket to send responses to
 * @param steamId The steam ID of the player making the purchase
 * @param message The store purchase initialization message
 * @param inventory_db Database connection to create items
 * @return True if the purchase was processed successfully
 */
bool GCNetwork_Inventory::HandleStorePurchaseInit(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_StorePurchaseInit &message,
    MYSQL *inventory_db)
{
    if (!inventory_db || !g_itemSchema)
    {
        logger::error("HandleStorePurchaseInit: Database connection or ItemSchema is null");
        return false;
    }

    logger::info("HandleStorePurchaseInit: Processing store purchase from player %llu - Currency: %d, Line items: %d",
                 steamId, message.currency(), message.line_items_size());

    if (message.line_items_size() == 0)
    {
        logger::error("HandleStorePurchaseInit: No items in purchase request");
        return false;
    }

    // Process the purchase and create items
    uint64_t txnId = 0;
    std::vector<uint64_t> itemIds;

    bool success = ProcessStorePurchase(p2psocket, steamId, message, inventory_db, txnId, itemIds);

    if (!success)
    {
        logger::error("HandleStorePurchaseInit: Failed to process purchase");

        // Send failure response
        CMsgGC_CC_GC2CL_StorePurchaseInitResponse responseMsg;
        responseMsg.set_result(2); // Failure

        NetworkMessage netMsg = NetworkMessage::FromProto(responseMsg, k_EMsgGC_CC_GC2CL_StorePurchaseInitResponse);
        netMsg.WriteToSocket(p2psocket, true);
        return false;
    }

    // Build success response
    CMsgGC_CC_GC2CL_StorePurchaseInitResponse responseMsg;
    responseMsg.set_result(0); // Success
    responseMsg.set_txn_id(txnId);

    // Add all created item IDs to the response
    for (const auto &itemId : itemIds)
    {
        responseMsg.add_item_ids(itemId);
    }

    // Send response to the client
    logger::info("HandleStorePurchaseInit: Sending StorePurchaseInitResponse with txnId %llu and %zu items",
                 txnId, itemIds.size());

    // Create and send the response
    NetworkMessage netMsg = NetworkMessage::FromProto(responseMsg, k_EMsgGC_CC_GC2CL_StorePurchaseInitResponse);
    bool responseSent = netMsg.WriteToSocket(p2psocket, true);

    // Log after sending
    logger::info("HandleStorePurchaseInit: Response sent: %s", responseSent ? "Success" : "Failed");

    logger::info("HandleStorePurchaseInit: Purchase complete - TxnId: %llu, Items: %zu",
                 txnId, itemIds.size());

    return responseSent;
}

/**
 * Processes a store purchase, creates the items, and returns transaction info
 *
 * @param p2psocket The socket to send responses to
 * @param steamId The steam ID of the player making the purchase
 * @param message The store purchase initialization message
 * @param inventory_db Database connection to create items
 * @param txnId Output parameter for the transaction ID
 * @param itemIds Output parameter for the created item IDs
 * @return True if the purchase was processed successfully
 */
bool GCNetwork_Inventory::ProcessStorePurchase(
    SNetSocket_t p2psocket,
    uint64_t steamId,
    const CMsgGC_CC_CL2GC_StorePurchaseInit &message,
    MYSQL *inventory_db,
    uint64_t &txnId,
    std::vector<uint64_t> &itemIds)
{
    // Generate a transaction ID (20 digits)
    // Combining current timestamp with a random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(100000, 999999);

    txnId = (static_cast<uint64_t>(time(nullptr)) * 1000) + dist(gen);

    // Begin transaction
    if (mysql_query(inventory_db, "START TRANSACTION") != 0)
    {
        logger::error("ProcessStorePurchase: Failed to start transaction: %s", mysql_error(inventory_db));
        return false;
    }

    try
    {
        // Process each line item
        for (int i = 0; i < message.line_items_size(); i++)
        {
            const auto &lineItem = message.line_items(i);
            uint32_t defIndex = lineItem.item_def_id();
            uint32_t quantity = lineItem.quantity();

            logger::info("ProcessStorePurchase: Processing item %d/%d - DefIndex: %u, Quantity: %u",
                         i + 1, message.line_items_size(), defIndex, quantity);

            // Create items based on quantity
            for (uint32_t q = 0; q < quantity; q++)
            {
                // Create a new item for each quantity
                std::string itemName = "Key";
                std::string itemIdStr = "key-" + std::to_string(defIndex);

                // Directly insert the item into the database
                char insertQuery[1024];
                snprintf(insertQuery, sizeof(insertQuery),
                         "INSERT INTO csgo_items "
                         "(owner_steamid2, item_id, name, weapon_type, weapon_id, "
                         "rarity, quality, tradable, acquired_by, acknowledged) "
                         "VALUES ('%s', '%s', '%s', 'Key', 'key', 3, 4, 1, 'purchased', 0)",
                         GCNetwork_Users::SteamID64ToSteamID2(steamId).c_str(),
                         itemIdStr.c_str(),
                         itemName.c_str());

                if (mysql_query(inventory_db, insertQuery) != 0)
                {
                    logger::error("ProcessStorePurchase: Failed to insert item: %s", mysql_error(inventory_db));
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                uint64_t newItemId = mysql_insert_id(inventory_db);
                if (newItemId == 0)
                {
                    logger::error("ProcessStorePurchase: Failed to get new item ID");
                    mysql_query(inventory_db, "ROLLBACK");
                    return false;
                }

                // Add to the list of created item IDs
                itemIds.push_back(newItemId);
                logger::info("ProcessStorePurchase: Created item with ID %llu", newItemId);
            }
        }

        // Commit transaction
        if (mysql_query(inventory_db, "COMMIT") != 0)
        {
            logger::error("ProcessStorePurchase: Failed to commit transaction: %s", mysql_error(inventory_db));
            mysql_query(inventory_db, "ROLLBACK");
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        logger::error("ProcessStorePurchase: Exception caught: %s", e.what());
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
    catch (...)
    {
        logger::error("ProcessStorePurchase: Unknown exception caught");
        mysql_query(inventory_db, "ROLLBACK");
        return false;
    }
}
