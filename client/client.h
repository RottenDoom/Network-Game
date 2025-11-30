/**
 * @file client.h
 * @brief Client-side game networking and state management
 * @author NetworkGame Project
 * @date 2024
 */

#pragma once
#include "protocol.h"
#include <asio.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <map>
#include <mutex>

using asio::ip::tcp;

/**
 * @struct InterpolatedPlayer
 * @brief Player state with interpolation support for smooth rendering
 */
struct InterpolatedPlayer
{
        uint32_t id;
        protocol::Vec2 current_pos;
        protocol::Vec2 target_pos;
        protocol::Vec2 render_pos;
        uint32_t score;
        std::chrono::steady_clock::time_point last_update;  ///< Last update timestamp
};

/**
 * @class GameClient
 * @brief Manages client-side networking, state interpolation, and server communication
 *
 * This class handles:
 * - TCP connection to game server
 * - Client-side prediction for responsive movement
 * - Entity interpolation for smooth visuals
 * - Input reconciliation with server state
 * - Thread-safe state access
 */
class GameClient
{
public:
        /**
         * @brief Construct game client
         * @param io ASIO I/O context for networking
         */
        GameClient(asio::io_context& io);

        /**
         * @brief Connect to game server
         * @param host Server hostname or IP address
         * @param port Server port number
         */
        void connect(const std::string& host, uint16_t port);

        /**
         * @brief Send player input to server
         * @param dx X direction (-1 to 1)
         * @param dy Y direction (-1 to 1)
         */
        void send_input(float dx, float dy);

        /**
         * @brief Apply local prediction immediately for responsive movement
         * @param dx X direction (-1 to 1)
         * @param dy Y direction (-1 to 1)
         * @param dt Delta time since last frame
         */
        void apply_local_input(float dx, float dy, float dt);

        /**
         * @brief Update entity interpolation for smooth rendering
         * @param dt Delta time since last frame
         */
        void update_interpolation(float dt);

        /**
         * @brief Get all players (thread-safe copy)
         * @return Map of player ID to interpolated player state
         */
        std::map<uint32_t, InterpolatedPlayer> get_players() const;

        /**
         * @brief Get all coins (thread-safe copy)
         * @return Map of coin ID to coin state
         */
        std::map<uint32_t, protocol::CoinState> get_coins() const;

        /**
         * @brief Check if connected to server
         * @return true if connected, false otherwise
         */
        bool is_connected() const;

        /**
         * @brief Get local player ID
         * @return Player ID assigned by server, 0 if not assigned
         */
        uint32_t get_my_id() const;

private:
        void read_header();
        void read_body(uint32_t length);
        void process_message(const std::vector<uint8_t>& data);
        void handle_game_state(protocol::MessageReader& reader);

        asio::io_context& io_;
        tcp::socket socket_;
        asio::steady_timer latency_timer_;

        std::array<uint8_t, sizeof(protocol::MessageHeader)> header_buffer_;
        std::vector<uint8_t> body_buffer_;

        std::map<uint32_t, InterpolatedPlayer> players_;
        std::map<uint32_t, protocol::CoinState> coins_;

        struct PendingInput
        {
                uint32_t seq;
                float dx, dy;
                uint32_t timestamp;
        };

        std::deque<PendingInput> pending_inputs_;
        uint32_t next_input_seq_;

        /**
         * @struct Snapshot
         * @brief Server state snapshot for interpolation
         */
        struct Snapshot
        {
                uint64_t server_ts_ms;                                          ///< Server timestamp in milliseconds
                std::unordered_map<uint32_t, protocol::Vec2> player_positions;  ///< Player positions
                std::unordered_map<uint32_t, uint32_t> player_scores;           ///< Player scores
                std::unordered_map<uint32_t, uint32_t> player_last_seq;         ///< Last processed input sequence
        };

        std::deque<Snapshot> snapshot_buffer;  ///< Buffer of server snapshots for interpolation
        const std::chrono::milliseconds INTERP_DELAY = std::chrono::milliseconds(200);  ///< Interpolation delay

        bool connected_;         ///< Connection status
        uint32_t my_player_id_;  ///< Local player ID assigned by server

        float ping_ms_;  ///< Current ping in milliseconds

        mutable std::mutex mutex_;  ///< Mutex protecting shared state between threads

public:
        /**
         * @brief Get current ping to server
         * @return Ping in milliseconds
         */
        float get_ping_ms() const;

        const float INTERPOLATION_TIME = 0.2f;  ///< 200ms interpolation buffer (matches simulated latency)
};