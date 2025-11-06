#include "discord_notifier.hpp"
#include "logger.hpp"
#include <sstream>
#include <iomanip>
#include <map>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#else
    #include <curl/curl.h>
#endif

// Static member initialization
std::string DiscordNotifier::m_webhook_url = "";
std::string DiscordNotifier::m_moderator_role_id = "";
bool DiscordNotifier::m_enabled = false;

// Report type definitions
static const std::map<int, std::pair<std::string, std::string>> REPORT_TYPES = {
    {1, {"🎯", "Aimbot"}},
    {2, {"👻", "Wallhack"}},
    {3, {"⚡", "Speedhack/Other Hack"}},
    {4, {"🔥", "Griefing/Team Harm"}},
    {5, {"💬", "Abusive Text Chat"}},
    {6, {"🔊", "Abusive Voice Chat"}}
};

void DiscordNotifier::Initialize(const std::string& webhook_url, const std::string& moderator_role_id) {
    m_webhook_url = webhook_url;
    m_moderator_role_id = moderator_role_id;
    m_enabled = !webhook_url.empty();
    
    if (m_enabled) {
        logger::info("Discord notifier enabled with webhook");
        if (!moderator_role_id.empty()) {
            logger::info("Will ping moderator role: %s", moderator_role_id.c_str());
        }
    } else {
        logger::info("Discord notifier disabled (no webhook URL)");
    }
}

bool DiscordNotifier::IsEnabled() {
    return m_enabled;
}

std::string DiscordNotifier::GetReportTypeEmoji(int type) {
    auto it = REPORT_TYPES.find(type);
    return (it != REPORT_TYPES.end()) ? it->second.first : "❓";
}

std::string DiscordNotifier::GetReportTypeName(int type) {
    auto it = REPORT_TYPES.find(type);
    return (it != REPORT_TYPES.end()) ? it->second.second : "Unknown";
}

std::string DiscordNotifier::SteamID64ToSteamID3(uint64_t steamid64) {
    uint32_t accountID = (uint32_t)(steamid64 & 0xFFFFFFFF);
    std::stringstream ss;
    ss << "[U:1:" << accountID << "]";
    return ss.str();
}

std::string DiscordNotifier::BuildEmbedJSON(const std::vector<ReportData>& reports) {
    if (reports.empty()) return "{}";
    
    // Get reported player from first report (all should be same player in batch)
    uint64_t receiver = reports[0].receiver_steamid;
    std::string receiver_name = reports[0].receiver_name;
    std::string receiver_id3 = SteamID64ToSteamID3(receiver);
    
    // Count report types
    std::map<int, int> typeCounts;
    for (const auto& report : reports) {
        typeCounts[report.report_type]++;
    }
    
    // Build report type list
    std::stringstream reportTypes;
    for (const auto& pair : typeCounts) {
        reportTypes << GetReportTypeEmoji(pair.first) << " " 
                   << GetReportTypeName(pair.first) << " × " << pair.second << "\\n";
    }
    
    // Count unique reporters
    std::set<uint64_t> uniqueReporters;
    for (const auto& report : reports) {
        uniqueReporters.insert(report.sender_steamid);
    }
    
    // Get current timestamp in ISO 8601 format
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    // Build recent reports list (up to 5)
    std::stringstream recentReports;
    size_t displayCount = std::min(size_t(5), reports.size());
    for (size_t i = 0; i < displayCount; i++) {
        const auto& report = reports[i];
        std::string senderID3 = SteamID64ToSteamID3(report.sender_steamid);
        recentReports << GetReportTypeEmoji(report.report_type) << " " 
                     << GetReportTypeName(report.report_type) << " by " 
                     << senderID3;
        if (!report.sender_name.empty()) {
            recentReports << " (" << report.sender_name << ")";
        }
        recentReports << "\\n";
    }
    
    if (reports.size() > displayCount) {
        recentReports << "... and " << (reports.size() - displayCount) << " more report(s)";
    }
    
    // Build profile URL
    std::stringstream profileURL;
    profileURL << "https://steamcommunity.com/profiles/" << receiver;
    
    // Build JSON
    std::stringstream json;
    json << "{"
         << "\"embeds\": [{"
         << "\"title\": \"🚨 New Player Report(s)\","
         << "\"color\": 16728132," // Red color
         << "\"fields\": ["
         << "{"
         << "\"name\": \"👤 Reported Player\","
         << "\"value\": \"" << receiver_id3;
    
    if (!receiver_name.empty()) {
        json << " (" << receiver_name << ")";
    }
    
    json << "\\n[Profile](" << profileURL.str() << ")\","
         << "\"inline\": false"
         << "},"
         << "{"
         << "\"name\": \"📊 Report Summary\","
         << "\"value\": \"" << reportTypes.str() << "\","
         << "\"inline\": true"
         << "},"
         << "{"
         << "\"name\": \"📈 Statistics\","
         << "\"value\": \"**Total Reports:** " << reports.size() 
         << "\\n**Unique Reporters:** " << uniqueReporters.size() << "\","
         << "\"inline\": true"
         << "},"
         << "{"
         << "\"name\": \"📝 Recent Reports\","
         << "\"value\": \"" << recentReports.str() << "\","
         << "\"inline\": false"
         << "}"
         << "],"
         << "\"footer\": {\"text\": \"FragMount Report System\"},"
         << "\"timestamp\": \"" << timestamp << "\""
         << "}]";
    
    // Add mention if role ID is set
    if (!m_moderator_role_id.empty()) {
        json << ",\"content\": \"<@&" << m_moderator_role_id << "> New player report(s) received!\"";
    }
    
    json << "}";
    
    return json.str();
}

#ifdef _WIN32
// Windows implementation using WinHTTP
bool DiscordNotifier::SendWebhook(const std::string& json_payload) {
    // Parse webhook URL
    std::wstring wurl(m_webhook_url.begin(), m_webhook_url.end());
    
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostname[256] = {0};
    wchar_t path[1024] = {0};
    
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = sizeof(hostname) / sizeof(wchar_t);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);
    
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
        logger::error("Failed to parse webhook URL");
        return false;
    }
    
    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"FragMount-GC-Server/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession) {
        logger::error("WinHttpOpen failed");
        return false;
    }
    
    // Connect
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        hostname,
        urlComp.nPort,
        0
    );
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        logger::error("WinHttpConnect failed");
        return false;
    }
    
    // Create request
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        logger::error("WinHttpOpenRequest failed");
        return false;
    }
    
    // Set headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    
    // Send request
    bool success = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        -1,
        (LPVOID)json_payload.c_str(),
        json_payload.length(),
        json_payload.length(),
        0
    );
    
    if (success) {
        success = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (success) {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            NULL,
            &statusCode,
            &statusCodeSize,
            NULL
        );
        
        success = (statusCode >= 200 && statusCode < 300);
        if (!success) {
            logger::error("Discord webhook returned status %d", statusCode);
        }
    } else {
        logger::error("WinHttpSendRequest failed: %d", GetLastError());
    }
    
    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return success;
}

#else
// Linux implementation using libcurl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    return size * nmemb; // Discard response
}

bool DiscordNotifier::SendWebhook(const std::string& json_payload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        logger::error("Failed to initialize curl");
        return false;
    }
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, m_webhook_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);
    
    if (!success) {
        logger::error("Discord webhook failed: %s", curl_easy_strerror(res));
    }
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code < 200 || response_code >= 300) {
        logger::error("Discord webhook returned status %ld", response_code);
        success = false;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return success;
}
#endif

bool DiscordNotifier::SendReportNotification(const ReportData& report) {
    if (!m_enabled) {
        return false;
    }
    
    std::vector<ReportData> reports = {report};
    return SendBatchReportNotification(reports);
}

bool DiscordNotifier::SendBatchReportNotification(const std::vector<ReportData>& reports) {
    if (!m_enabled || reports.empty()) {
        return false;
    }
    
    std::string json = BuildEmbedJSON(reports);
    bool success = SendWebhook(json);
    
    if (success) {
        logger::info("Sent Discord notification for %zu report(s)", reports.size());
    } else {
        logger::error("Failed to send Discord notification");
    }
    
    return success;
}
