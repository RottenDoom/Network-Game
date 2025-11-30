#pragma once
#include "protocol.h"
#include <asio.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <map>

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
        void update_interpolation(float dt);

        const std::map<uint32_t, InterpolatedPlayer>& get_players() const { return players_; }
        const std::map<uint32_t, protocol::CoinState>& get_coins() const { return coins_; }
        bool is_connected() const { return connected_; }
        uint32_t get_my_id() const { return my_player_id_; }

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

        bool connected_;
        uint32_t my_player_id_;

        const float INTERPOLATION_TIME = 0.1f;  // 100ms interpolation buffer
};