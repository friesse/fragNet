// Safe MySQL database implementation using prepared statements
#include "matchmaking_manager.hpp"
#ifdef _WIN32
    #include <mysql.h>
#else
    #include <mysql/mysql.h>
#endif
#include <memory>
#include <mutex>
#include "logger.hpp"

class MySQLDatabase : public IDatabase {
private:
    MYSQL* m_connection;
    mutable std::mutex m_dbMutex;
    
    // Prepared statements for common queries
    MYSQL_STMT* m_getPlayerStmt;
    MYSQL_STMT* m_updatePlayerStmt;
    MYSQL_STMT* m_logMatchStmt;
    
    bool PrepareStatements() {
        // Prepare SELECT statement for player rating
        const char* getPlayerQuery = 
            "SELECT mmr, rank_id, wins, level FROM player_rankings WHERE steamid64 = ?";
        
        m_getPlayerStmt = mysql_stmt_init(m_connection);
        if (!m_getPlayerStmt) {
            logger::error("Failed to init get player statement");
            return false;
        }
        
        if (mysql_stmt_prepare(m_getPlayerStmt, getPlayerQuery, strlen(getPlayerQuery)) != 0) {
            logger::error("Failed to prepare get player statement: %s", mysql_stmt_error(m_getPlayerStmt));
            return false;
        }
        
        // Prepare UPDATE/INSERT statement for player rating
        const char* updatePlayerQuery = 
            "INSERT INTO player_rankings (steamid64, mmr, rank_id, wins, level) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON DUPLICATE KEY UPDATE "
            "mmr = VALUES(mmr), rank_id = VALUES(rank_id), "
            "wins = VALUES(wins), level = VALUES(level)";
        
        m_updatePlayerStmt = mysql_stmt_init(m_connection);
        if (!m_updatePlayerStmt) {
            logger::error("Failed to init update player statement");
            return false;
        }
        
        if (mysql_stmt_prepare(m_updatePlayerStmt, updatePlayerQuery, strlen(updatePlayerQuery)) != 0) {
            logger::error("Failed to prepare update player statement: %s", mysql_stmt_error(m_updatePlayerStmt));
            return false;
        }
        
        // Prepare INSERT statement for match logging
        const char* logMatchQuery = 
            "INSERT INTO match_history (match_id, match_token, map_name, avg_mmr, "
            "team_a_players, team_b_players, server_address, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, NOW())";
        
        m_logMatchStmt = mysql_stmt_init(m_connection);
        if (!m_logMatchStmt) {
            logger::error("Failed to init log match statement");
            return false;
        }
        
        if (mysql_stmt_prepare(m_logMatchStmt, logMatchQuery, strlen(logMatchQuery)) != 0) {
            logger::error("Failed to prepare log match statement: %s", mysql_stmt_error(m_logMatchStmt));
            return false;
        }
        
        return true;
    }
    
public:
    MySQLDatabase(const char* host, const char* user, const char* password, const char* database, unsigned int port = 3306) 
        : m_connection(nullptr), m_getPlayerStmt(nullptr), m_updatePlayerStmt(nullptr), m_logMatchStmt(nullptr) {
        
        m_connection = mysql_init(nullptr);
        if (!m_connection) {
            throw std::runtime_error("Failed to initialize MySQL connection");
        }
        
        // Enable automatic reconnection
        my_bool reconnect = 1;
        mysql_options(m_connection, MYSQL_OPT_RECONNECT, &reconnect);
        
        // Set connection timeout
        unsigned int timeout = 10;
        mysql_options(m_connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        
        // Connect to database
        if (!mysql_real_connect(m_connection, host, user, password, database, port, nullptr, 0)) {
            std::string error = mysql_error(m_connection);
            mysql_close(m_connection);
            throw std::runtime_error("Failed to connect to database: " + error);
        }
        
        // Set UTF8 character set
        mysql_set_character_set(m_connection, "utf8mb4");
        
        // Prepare statements
        if (!PrepareStatements()) {
            mysql_close(m_connection);
            throw std::runtime_error("Failed to prepare database statements");
        }
        
        logger::info("Connected to MySQL database successfully");
    }
    
    ~MySQLDatabase() override {
        if (m_getPlayerStmt) mysql_stmt_close(m_getPlayerStmt);
        if (m_updatePlayerStmt) mysql_stmt_close(m_updatePlayerStmt);
        if (m_logMatchStmt) mysql_stmt_close(m_logMatchStmt);
        if (m_connection) mysql_close(m_connection);
    }
    
    std::optional<PlayerSkillRating> GetPlayerRating(uint64_t steamId) const override {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        
        PlayerSkillRating rating;
        rating.mmr = 1000;  // Default values
        rating.rank = 6;    // Gold Nova 1
        rating.wins = 0;
        rating.level = 1;
        
        // Bind parameters
        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        
        bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
        bind[0].buffer = (char*)&steamId;
        bind[0].is_null = 0;
        bind[0].length = 0;
        
        if (mysql_stmt_bind_param(m_getPlayerStmt, bind) != 0) {
            logger::error("Failed to bind get player parameters: %s", mysql_stmt_error(m_getPlayerStmt));
            return rating;  // Return default rating on error
        }
        
        // Execute query
        if (mysql_stmt_execute(m_getPlayerStmt) != 0) {
            logger::error("Failed to execute get player query: %s", mysql_stmt_error(m_getPlayerStmt));
            return rating;
        }
        
        // Bind result
        MYSQL_BIND result[4];
        memset(result, 0, sizeof(result));
        
        unsigned int mmr, rank, wins, level;
        
        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = (char*)&mmr;
        result[1].buffer_type = MYSQL_TYPE_LONG;
        result[1].buffer = (char*)&rank;
        result[2].buffer_type = MYSQL_TYPE_LONG;
        result[2].buffer = (char*)&wins;
        result[3].buffer_type = MYSQL_TYPE_LONG;
        result[3].buffer = (char*)&level;
        
        if (mysql_stmt_bind_result(m_getPlayerStmt, result) != 0) {
            logger::error("Failed to bind get player result: %s", mysql_stmt_error(m_getPlayerStmt));
            return rating;
        }
        
        // Fetch result
        if (mysql_stmt_fetch(m_getPlayerStmt) == 0) {
            rating.mmr = mmr;
            rating.rank = rank;
            rating.wins = wins;
            rating.level = level;
        }
        
        // Clean up
        mysql_stmt_free_result(m_getPlayerStmt);
        
        return rating;
    }
    
    bool UpdatePlayerRating(uint64_t steamId, const PlayerSkillRating& rating) override {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        
        // Bind parameters
        MYSQL_BIND bind[5];
        memset(bind, 0, sizeof(bind));
        
        bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
        bind[0].buffer = (char*)&steamId;
        
        bind[1].buffer_type = MYSQL_TYPE_LONG;
        bind[1].buffer = (char*)&rating.mmr;
        
        bind[2].buffer_type = MYSQL_TYPE_LONG;
        bind[2].buffer = (char*)&rating.rank;
        
        bind[3].buffer_type = MYSQL_TYPE_LONG;
        bind[3].buffer = (char*)&rating.wins;
        
        bind[4].buffer_type = MYSQL_TYPE_LONG;
        bind[4].buffer = (char*)&rating.level;
        
        if (mysql_stmt_bind_param(m_updatePlayerStmt, bind) != 0) {
            logger::error("Failed to bind update player parameters: %s", mysql_stmt_error(m_updatePlayerStmt));
            return false;
        }
        
        // Execute query
        if (mysql_stmt_execute(m_updatePlayerStmt) != 0) {
            logger::error("Failed to execute update player query: %s", mysql_stmt_error(m_updatePlayerStmt));
            return false;
        }
        
        return true;
    }
    
    bool LogMatch(const Match& match) override {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        
        // Build player lists as JSON strings
        std::string teamAPlayers = "[";
        for (size_t i = 0; i < match.teamA.size(); ++i) {
            if (i > 0) teamAPlayers += ",";
            teamAPlayers += std::to_string(match.teamA[i]->steamId);
        }
        teamAPlayers += "]";
        
        std::string teamBPlayers = "[";
        for (size_t i = 0; i < match.teamB.size(); ++i) {
            if (i > 0) teamBPlayers += ",";
            teamBPlayers += std::to_string(match.teamB[i]->steamId);
        }
        teamBPlayers += "]";
        
        std::string serverAddr = match.serverAddress + ":" + std::to_string(match.serverPort);
        
        // Bind parameters
        MYSQL_BIND bind[7];
        memset(bind, 0, sizeof(bind));
        
        bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
        bind[0].buffer = (char*)&match.matchId;
        
        unsigned long tokenLen = match.matchToken.length();
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char*)match.matchToken.c_str();
        bind[1].length = &tokenLen;
        
        unsigned long mapLen = match.mapName.length();
        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = (char*)match.mapName.c_str();
        bind[2].length = &mapLen;
        
        bind[3].buffer_type = MYSQL_TYPE_LONG;
        bind[3].buffer = (char*)&match.avgMMR;
        
        unsigned long teamALen = teamAPlayers.length();
        bind[4].buffer_type = MYSQL_TYPE_STRING;
        bind[4].buffer = (char*)teamAPlayers.c_str();
        bind[4].length = &teamALen;
        
        unsigned long teamBLen = teamBPlayers.length();
        bind[5].buffer_type = MYSQL_TYPE_STRING;
        bind[5].buffer = (char*)teamBPlayers.c_str();
        bind[5].length = &teamBLen;
        
        unsigned long serverLen = serverAddr.length();
        bind[6].buffer_type = MYSQL_TYPE_STRING;
        bind[6].buffer = (char*)serverAddr.c_str();
        bind[6].length = &serverLen;
        
        if (mysql_stmt_bind_param(m_logMatchStmt, bind) != 0) {
            logger::error("Failed to bind log match parameters: %s", mysql_stmt_error(m_logMatchStmt));
            return false;
        }
        
        // Execute query
        if (mysql_stmt_execute(m_logMatchStmt) != 0) {
            logger::error("Failed to execute log match query: %s", mysql_stmt_error(m_logMatchStmt));
            return false;
        }
        
        logger::info("Logged match %llu to database", match.matchId);
        return true;
    }
};

// Factory function for creating database connection
std::shared_ptr<IDatabase> CreateMySQLDatabase(const char* connectionString) {
    // Parse connection string (simplified example)
    // Format: "mysql://user:password@host:port/database"
    
    try {
        return std::make_shared<MySQLDatabase>("localhost", "root", "password", "csgo_matchmaking");
    } catch (const std::exception& e) {
        logger::error("Failed to create MySQL database: %s", e.what());
        return nullptr;
    }
}
