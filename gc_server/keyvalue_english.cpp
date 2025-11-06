#include "stdafx.h"
#include "keyvalue_english.hpp"
#include "keyvalue.hpp"

LocalizationSystem::LocalizationSystem()
{
    LoadLocalizationFile("items/csgo_english.txt");
}

LocalizationSystem& LocalizationSystem::GetInstance() {
    static LocalizationSystem instance;
    return instance;
}

std::string_view LocalizationSystem::GetLocalizedString(std::string_view token, std::string_view fallback) const
{
    // Remove the # prefix if present
    std::string tokenStr;
    if (!token.empty() && token[0] == '#')
    {
        tokenStr = std::string(token.substr(1));
    }
    else
    {
        tokenStr = std::string(token);
    }

    auto it = m_localizationStrings.find(tokenStr);
    if (it != m_localizationStrings.end())
    {
        return it->second;
    }
    return fallback;
}

bool LocalizationSystem::LoadLocalizationFile(const char *path)
{
    logger::info("Starting to load localization file with direct parsing: %s", path);

    // Open file
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        logger::info("File %s does not exist or cannot be opened", path);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read entire file content
    std::vector<unsigned char> buffer(fileSize);
    fread(buffer.data(), 1, fileSize, f);
    fclose(f);

    // Check for UTF-16 BOM
    bool isUTF16 = false;
    if (fileSize >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE)
    {
        isUTF16 = true;
        logger::info("UTF-16 LE encoding detected");
    }

    // Convert to string (handle UTF-16 if needed)
    std::string fileContent;
    if (isUTF16)
    {
        // Skip BOM
        for (size_t i = 2; i < buffer.size(); i += 2)
        {
            if (i + 1 >= buffer.size())
                break;

            // Get UTF-16 character (little endian)
            uint16_t utf16Char = buffer[i] | (buffer[i + 1] << 8);

            // Convert to UTF-8
            if (utf16Char < 128)
            {
                fileContent.push_back(static_cast<char>(utf16Char));
            }
            else if (utf16Char < 2048)
            {
                fileContent.push_back(static_cast<char>(192 | (utf16Char >> 6)));
                fileContent.push_back(static_cast<char>(128 | (utf16Char & 63)));
            }
            else
            {
                fileContent.push_back(static_cast<char>(224 | (utf16Char >> 12)));
                fileContent.push_back(static_cast<char>(128 | ((utf16Char >> 6) & 63)));
                fileContent.push_back(static_cast<char>(128 | (utf16Char & 63)));
            }
        }
    }
    else
    {
        fileContent.assign(reinterpret_cast<char *>(buffer.data()), buffer.size());
    }

    // More aggressive direct parsing - looking for any "key" "value" pattern in the file
    size_t pos = 0;
    size_t tokenCount = 0;

    // Log some debugging info
    //logger::info("Converted file size: %zu bytes", fileContent.size());

    // Directly scan through the entire file for "key" "value" patterns
    while (pos < fileContent.size())
    {
        // Skip whitespace and comments
        while (pos < fileContent.size())
        {
            // Skip whitespace
            while (pos < fileContent.size() &&
                   (fileContent[pos] == ' ' || fileContent[pos] == '\t' ||
                    fileContent[pos] == '\r' || fileContent[pos] == '\n'))
            {
                pos++;
            }

            // Skip comments
            if (pos + 1 < fileContent.size() && fileContent[pos] == '/' && fileContent[pos + 1] == '/')
            {
                pos = fileContent.find('\n', pos);
                if (pos == std::string::npos)
                    break;
                pos++;
                continue;
            }

            break;
        }

        if (pos >= fileContent.size())
            break;

        // Looking for key pattern "key"
        if (fileContent[pos] != '"')
        {
            // Not a key - skip until next quote or newline
            size_t nextQuote = fileContent.find('"', pos);
            size_t nextNewline = fileContent.find('\n', pos);

            if (nextQuote == std::string::npos && nextNewline == std::string::npos)
                break;

            pos = std::min(
                nextQuote != std::string::npos ? nextQuote : fileContent.size(),
                nextNewline != std::string::npos ? nextNewline : fileContent.size());
            continue;
        }

        // Extract key
        size_t keyStart = pos + 1;
        size_t keyEnd = fileContent.find('"', keyStart);
        if (keyEnd == std::string::npos)
            break;

        std::string key = fileContent.substr(keyStart, keyEnd - keyStart);
        pos = keyEnd + 1;

        // Skip whitespace to value
        while (pos < fileContent.size() &&
               (fileContent[pos] == ' ' || fileContent[pos] == '\t'))
        {
            pos++;
        }

        // Extract value - must start with a quote
        if (pos >= fileContent.size() || fileContent[pos] != '"')
        {
            continue; // No value, skip this key
        }

        size_t valueStart = pos + 1;
        size_t valueEnd = fileContent.find('"', valueStart);
        if (valueEnd == std::string::npos)
            break;

        std::string value = fileContent.substr(valueStart, valueEnd - valueStart);
        pos = valueEnd + 1;

        // Store in map - only if key seems like a token (no spaces, reasonable length)
        if (key.find(' ') == std::string::npos && key.size() < 100 &&
            key != "lang" && key != "Language" && key != "Tokens")
        {
            m_localizationStrings.emplace(key, value);
            tokenCount++;

            // Debug: log some example tokens
            /*if (tokenCount <= 5 || (tokenCount % 1000 == 0))
            {
                logger::info("Token %zu: \"%s\" = \"%s\"",
                            tokenCount, key.c_str(),
                            value.size() > 30 ? (value.substr(0, 30) + "...").c_str() : value.c_str());
            }*/
        }
    }

    logger::info("Loaded %zu localized strings from %s", tokenCount, path);
    return tokenCount > 0;
}

// helper
std::string_view LocalizeToken(std::string_view token, std::string_view fallback)
{
    return LocalizationSystem::GetInstance().GetLocalizedString(token, fallback);
}