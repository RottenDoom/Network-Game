#pragma once
#include "protocol.h"
#include <asio.hpp>
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>

class GameSession : public std::enable_shared_from_this<GameSession>
{
public:
        GameSession(asio::io_context& io);

        void add_player(uint32_t player_id);
        void remove_player(uint32_t player_id);
        void process_input(uint32_t player_id, const protocol::ClientInput& input);
        void start();

        protocol::MessageBuffer create_state_message();

private:
        void update_game_logic();
        void spawn_coin();
        bool check_coin_collision(uint32_t player_id, uint32_t coin_id);

        asio::io_context& io_;
        asio::steady_timer update_timer_;
        asio::steady_timer coin_spawn_timer_;

        std::unordered_map<uint32_t, protocol::PlayerState> players_;
        std::unordered_map<uint32_t, protocol::CoinState> coins_;

        uint32_t next_coin_id_;
        std::mt19937 rng_;
        std::chrono::steady_clock::time_point last_update_;

        bool game_running_;
        const float MAP_WIDTH     = 800.0f;
        const float MAP_HEIGHT    = 600.0f;
        const float PLAYER_SPEED  = 200.0f;
        const float COIN_RADIUS   = 20.0f;
        const float PLAYER_RADIUS = 25.0f;
};