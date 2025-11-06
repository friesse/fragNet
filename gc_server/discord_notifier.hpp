#ifndef DISCORD_NOTIFIER_HPP
#define DISCORD_NOTIFIER_HPP

#include <string>
#include <cstdint>
#include <vector>

/**
 * Discord Webhook Notifier
 * Sends report notifications to Discord in real-time
 * 
 * Usage:
 *   DiscordNotifier::Initialize("https://discord.com/api/webhooks/...", "ROLE_ID");
 *   DiscordNotifier::SendReportNotification(reportData);
 */

struct ReportData {
    uint64_t sender_steamid;
    uint64_t receiver_steamid;
    int report_type;
    uint64_t match_id;
    std::string sender_name;
    std::string receiver_name;
};

class DiscordNotifier {
public:
    /**
     * Initialize the Discord notifier
     * @param webhook_url Discord webhook URL
     * @param moderator_role_id Discord role ID to ping (optional, can be empty)
     */
    static void Initialize(const std::string& webhook_url, const std::string& moderator_role_id = "");
    
    /**
     * Send a report notification to Discord
     * @param report Report data
     * @return true if sent successfully
     */
    static bool SendReportNotification(const ReportData& report);
    
    /**
     * Send a batch report notification (multiple reports for same player)
     * @param reports Vector of reports
     * @return true if sent successfully
     */
    static bool SendBatchReportNotification(const std::vector<ReportData>& reports);
    
    /**
     * Check if Discord notifier is enabled
     */
    static bool IsEnabled();
    
private:
    static std::string m_webhook_url;
    static std::string m_moderator_role_id;
    static bool m_enabled;
    
    // Helper functions
    static std::string GetReportTypeEmoji(int type);
    static std::string GetReportTypeName(int type);
    static std::string SteamID64ToSteamID3(uint64_t steamid64);
    static std::string BuildEmbedJSON(const std::vector<ReportData>& reports);
    static bool SendWebhook(const std::string& json_payload);
};

#endif // DISCORD_NOTIFIER_HPP
