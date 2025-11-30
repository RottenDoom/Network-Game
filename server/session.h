/**
 * @file session.h
 * @brief Game session logic and state management
 * @author NetworkGame Project
 * @date 2024
 */

#pragma once
#include "protocol.h"
#include <asio.hpp>
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>

/**
 * @class GameSession
 * @brief Manages game logic including player movement, coin spawning, and collision detection
 */
class GameSession : public std::enable_shared_from_this<GameSession>
{
public:
        /**
         * @brief Construct game session
         * @param io ASIO I/O context for timers
         */
        GameSession(asio::io_context& io);

        /**
         * @brief Add a player to the game
         * @param player_id Unique player ID
         */
        void add_player(uint32_t player_id);

        /**
         * @brief Remove a player from the game
         * @param player_id Player ID to remove
         */
        void remove_player(uint32_t player_id);

        /**
         * @brief Process player input and update game state
         * @param player_id Player ID
         * @param input Input data from client
         */
        void process_input(uint32_t player_id, const protocol::ClientInput& input);

        /**
         * @brief Start the game session
         */
        void start();

        /**
         * @brief Create game state message for broadcasting
         * @return Serialized game state message
         */
        protocol::MessageBuffer create_state_message();

private:
        void update_game_logic();
        void spawn_coin();
        void schedule_coin_spawn();
        bool check_coin_collision(uint32_t player_id, uint32_t coin_id);

        asio::io_context& io_;
        asio::steady_timer update_timer_;
        asio::steady_timer coin_spawn_timer_;

        std::unordered_map<uint32_t, protocol::PlayerState> players_;
        std::unordered_map<uint32_t, protocol::CoinState> coins_;

        uint32_t next_coin_id_;
        std::mt19937 rng_;
        std::chrono::steady_clock::time_point last_update_;

        bool game_running_;                  ///< Whether the game is currently running
        const float MAP_WIDTH     = 800.0f;  ///< Game world width
        const float MAP_HEIGHT    = 600.0f;  ///< Game world height
        const float PLAYER_SPEED  = 200.0f;  ///< Player movement speed (pixels/second)
        const float COIN_RADIUS   = 20.0f;   ///< Coin collision radius
        const float PLAYER_RADIUS = 25.0f;   ///< Player collision radius
};