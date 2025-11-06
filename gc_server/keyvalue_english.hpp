#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
class KeyValue;
class LocalizationSystem
{
public:
    static LocalizationSystem& GetInstance();
    
    LocalizationSystem(const LocalizationSystem&) = delete;
    LocalizationSystem& operator=(const LocalizationSystem&) = delete;
    
    std::string_view GetLocalizedString(std::string_view token, std::string_view fallback = {}) const;
    
    bool LoadLocalizationFile(const char* path);
    
private:
    LocalizationSystem();
    
    std::unordered_map<std::string, std::string> m_localizationStrings;
};

std::string_view LocalizeToken(std::string_view token, std::string_view fallback = {});