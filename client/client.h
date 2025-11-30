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

struct InterpolatedPlayer
{
        uint32_t id;
        protocol::Vec2 current_pos;
        protocol::Vec2 target_pos;
        protocol::Vec2 render_pos;
        uint32_t score;
        std::chrono::steady_clock::time_point last_update;
};

class GameClient
{
public:
        GameClient(asio::io_context& io);

        void connect(const std::string& host, uint16_t port);
        void send_input(float dx, float dy);
        // Apply local prediction immediately (so movement feels responsive)
        void apply_local_input(float dx, float dy, float dt);
        void update_interpolation(float dt);

        // Return copies to avoid iterator invalidation when networking thread updates state
        std::map<uint32_t, InterpolatedPlayer> get_players() const;
        std::map<uint32_t, protocol::CoinState> get_coins() const;
        bool is_connected() const;
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

        // Snapshot buffer for interpolation of remote players
        struct Snapshot
        {
                uint64_t server_ts_ms;
                std::unordered_map<uint32_t, protocol::Vec2> player_positions;
                std::unordered_map<uint32_t, uint32_t> player_scores;
                std::unordered_map<uint32_t, uint32_t> player_last_seq;
        };

        std::deque<Snapshot> snapshot_buffer;
        const std::chrono::milliseconds INTERP_DELAY = std::chrono::milliseconds(200);

        bool connected_;
        uint32_t my_player_id_;

        float ping_ms_;

        // Mutex to protect shared state between network thread and main/render thread
        mutable std::mutex mutex_;

public:
        float get_ping_ms() const;

        const float INTERPOLATION_TIME = 0.2f;  // 200ms interpolation buffer (matches simulated latency)
};