#include "session.h"
#include <iostream>
#include <cmath>

GameSession::GameSession(asio::io_context& io)
    : io_(io), update_timer_(io), coin_spawn_timer_(io), next_coin_id_(1), game_running_(false)
{
        std::random_device rd;
        rng_.seed(rd());
}

void GameSession::add_player(uint32_t player_id)
{
        protocol::PlayerState ps;
        ps.id               = player_id;
        ps.position         = protocol::Vec2(400.0f, 300.0f);
        ps.score            = 0;
        players_[player_id] = ps;
        std::cout << "Player " << player_id << " joined. Total: " << players_.size() << "\n";
}

void GameSession::remove_player(uint32_t player_id)
{
        players_.erase(player_id);
        std::cout << "Player " << player_id << " left. Total: " << players_.size() << "\n";
}

void GameSession::process_input(uint32_t player_id, const protocol::ClientInput& input)
{
        auto it = players_.find(player_id);
        if (it == players_.end() || !game_running_)
                return;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last_update_).count();
        if (dt > 0.1f)
                dt = 0.016f;  // Cap at 16ms for first frame

        protocol::PlayerState& player = it->second;

        // Normalize input
        float len = std::sqrt(input.dx * input.dx + input.dy * input.dy);
        if (len > 0.01f)
        {
                float nx           = input.dx / len;
                float ny           = input.dy / len;

                player.position.x += nx * PLAYER_SPEED * dt;
                player.position.y += ny * PLAYER_SPEED * dt;

                // Clamp to map bounds
                player.position.x = std::max(PLAYER_RADIUS, std::min(MAP_WIDTH - PLAYER_RADIUS, player.position.x));
                player.position.y = std::max(PLAYER_RADIUS, std::min(MAP_HEIGHT - PLAYER_RADIUS, player.position.y));
        }

        // Check coin collisions
        std::vector<uint32_t> collected;
        for (auto& [coin_id, coin] : coins_)
        {
                if (check_coin_collision(player_id, coin_id))
                {
                        collected.push_back(coin_id);
                        player.score++;
                }
        }

        for (uint32_t coin_id : collected)
        {
                coins_.erase(coin_id);
                std::cout << "Player " << player_id << " collected coin. Score: " << player.score << "\n";
        }
}

void GameSession::start()
{
        if (game_running_)
                return;
        game_running_ = true;
        last_update_  = std::chrono::steady_clock::now();

        std::cout << "Game starting!\n";

        // Spawn initial coins
        for (int i = 0; i < 3; i++)
        {
                spawn_coin();
        }

        // Start game loop
        update_timer_.expires_after(std::chrono::milliseconds(16));
        update_timer_.async_wait(
            [self = shared_from_this()](auto ec)
            {
                    if (!ec)
                            self->update_game_logic();
            });

        // Start coin spawner
        coin_spawn_timer_.expires_after(std::chrono::seconds(3));
        coin_spawn_timer_.async_wait(
            [self = shared_from_this()](auto ec)
            {
                    if (!ec && self->game_running_)
                    {
                            self->spawn_coin();
                            self->coin_spawn_timer_.expires_after(std::chrono::seconds(3));
                            self->coin_spawn_timer_.async_wait(
                                [self](auto ec)
                                {
                                        if (!ec && self->game_running_)
                                                self->spawn_coin();
                                });
                    }
            });
}

void GameSession::update_game_logic()
{
        if (!game_running_)
                return;

        last_update_ = std::chrono::steady_clock::now();

        // Continue update loop
        update_timer_.expires_after(std::chrono::milliseconds(16));
        update_timer_.async_wait(
            [self = shared_from_this()](auto ec)
            {
                    if (!ec)
                            self->update_game_logic();
            });
}

void GameSession::spawn_coin()
{
        std::uniform_real_distribution<float> dist_x(COIN_RADIUS, MAP_WIDTH - COIN_RADIUS);
        std::uniform_real_distribution<float> dist_y(COIN_RADIUS, MAP_HEIGHT - COIN_RADIUS);

        protocol::CoinState coin;
        coin.id         = next_coin_id_++;
        coin.position.x = dist_x(rng_);
        coin.position.y = dist_y(rng_);

        coins_[coin.id] = coin;
        std::cout << "Spawned coin " << coin.id << " at (" << coin.position.x << ", " << coin.position.y << ")\n";
}

bool GameSession::check_coin_collision(uint32_t player_id, uint32_t coin_id)
{
        auto pit = players_.find(player_id);
        auto cit = coins_.find(coin_id);
        if (pit == players_.end() || cit == coins_.end())
                return false;

        const auto& ppos = pit->second.position;
        const auto& cpos = cit->second.position;

        float dx         = ppos.x - cpos.x;
        float dy         = ppos.y - cpos.y;
        float dist_sq    = dx * dx + dy * dy;
        float threshold  = (PLAYER_RADIUS + COIN_RADIUS);

        return dist_sq < (threshold * threshold);
}

protocol::MessageBuffer GameSession::create_state_message()
{
        protocol::MessageBuffer buf;
        buf.write_header(protocol::MessageType::SERVER_GAME_STATE);

        auto now           = std::chrono::steady_clock::now();
        uint32_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        buf.write_uint32(timestamp);

        buf.data.push_back(static_cast<uint8_t>(players_.size()));
        buf.data.push_back(static_cast<uint8_t>(coins_.size()));

        for (const auto& [id, player] : players_)
        {
                buf.write_player_state(player);
        }

        for (const auto& [id, coin] : coins_)
        {
                buf.write_coin_state(coin);
        }

        buf.finalize();
        return buf;
}