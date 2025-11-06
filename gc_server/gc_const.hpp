#pragma once

// game independent gc constants
constexpr uint32_t ProtobufMask = 0x80000000;
constexpr uint32_t CCProtoMask = 0x90000000;

// Inventory Version
constexpr uint64_t InventoryVersion = 2000336; // is this correct? taken from ClVe=SkinPlyr in pak_dat.vpk

// Client Version
constexpr uint64_t ClientVersion = 102;

// CMsgSOIDOwner type
enum SoIdType
{
    SoIdTypeSteamId = 1
};
