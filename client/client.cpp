#include "client.h"
#include <iostream>

GameClient::GameClient(asio::io_context& io)
    : io_(io), socket_(io), latency_timer_(io), connected_(false), my_player_id_(0)
{
}

void GameClient::connect(const std::string& host, uint16_t port)
{
        tcp::resolver resolver(io_);
        auto endpoints = resolver.resolve(host, std::to_string(port));

        asio::connect(socket_, endpoints);
        connected_ = true;

        std::cout << "Connected to server\n";

        // Send connection message
        protocol::MessageBuffer msg;
        msg.write_header(protocol::MessageType::CLIENT_CONNECT);
        msg.finalize();

        asio::write(socket_, asio::buffer(msg.data));

        read_header();
}

void GameClient::send_input(float dx, float dy)
{
        if (!connected_)
                return;

        protocol::MessageBuffer msg;
        msg.write_header(protocol::MessageType::CLIENT_INPUT);
        msg.write_float(dx);
        msg.write_float(dy);

        auto now           = std::chrono::steady_clock::now();
        uint32_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        msg.write_uint32(timestamp);
        msg.finalize();

        // Simulate 200ms send latency
        latency_timer_.expires_after(std::chrono::milliseconds(200));
        latency_timer_.async_wait(
            [this, data = msg.data](auto ec)
            {
                    if (!ec && connected_)
                    {
                            asio::async_write(socket_, asio::buffer(data), [](asio::error_code, std::size_t) {});
                    }
            });
}

void GameClient::update_interpolation(float dt)
{
        auto now = std::chrono::steady_clock::now();

        for (auto& [id, player] : players_)
        {
                float elapsed = std::chrono::duration<float>(now - player.last_update).count();
                float t       = std::min(elapsed / INTERPOLATION_TIME, 1.0f);

                // Linear interpolation
                player.render_pos.x = player.current_pos.x + (player.target_pos.x - player.current_pos.x) * t;
                player.render_pos.y = player.current_pos.y + (player.target_pos.y - player.current_pos.y) * t;
        }
}

void GameClient::read_header()
{
        asio::async_read(socket_,
                         asio::buffer(header_buffer_),
                         [this](asio::error_code ec, std::size_t)
                         {
                                 if (!ec)
                                 {
                                         protocol::MessageHeader header;
                                         std::memcpy(&header, header_buffer_.data(), sizeof(header));

                                         if (header.length > sizeof(protocol::MessageHeader) && header.length < 65536)
                                         {
                                                 read_body(header.length - sizeof(protocol::MessageHeader));
                                         }
                                         else
                                         {
                                                 read_header();
                                         }
                                 }
                                 else
                                 {
                                         std::cout << "Connection lost: " << ec.message() << "\n";
                                         connected_ = false;
                                 }
                         });
}

void GameClient::read_body(uint32_t length)
{
        body_buffer_.resize(length);

        asio::async_read(socket_,
                         asio::buffer(body_buffer_),
                         [this](asio::error_code ec, std::size_t)
                         {
                                 if (!ec)
                                 {
                                         std::vector<uint8_t> full_msg;
                                         full_msg.insert(full_msg.end(), header_buffer_.begin(), header_buffer_.end());
                                         full_msg.insert(full_msg.end(), body_buffer_.begin(), body_buffer_.end());

                                         // Simulate 200ms receive latency
                                         latency_timer_.expires_after(std::chrono::milliseconds(200));
                                         latency_timer_.async_wait(
                                             [this, msg = std::move(full_msg)](auto ec)
                                             {
                                                     if (!ec)
                                                             process_message(msg);
                                             });

                                         read_header();
                                 }
                                 else
                                 {
                                         connected_ = false;
                                 }
                         });
}

void GameClient::process_message(const std::vector<uint8_t>& data)
{
        protocol::MessageReader reader(data.data(), data.size());
        protocol::MessageHeader header;

        if (!reader.read_header(header))
                return;

        switch (header.type)
        {
        case protocol::MessageType::SERVER_GAME_STATE:
                handle_game_state(reader);
                break;

        default:
                break;
        }
}

void GameClient::handle_game_state(protocol::MessageReader& reader)
{
        uint32_t timestamp;
        if (!reader.read_uint32(timestamp))
                return;

        uint8_t player_count = 0, coin_count = 0;
        if (reader.offset + 2 > reader.size)
                return;
        player_count = reader.data[reader.offset++];
        coin_count   = reader.data[reader.offset++];

        auto now     = std::chrono::steady_clock::now();

        // Update players
        std::map<uint32_t, InterpolatedPlayer> new_players;
        for (int i = 0; i < player_count; i++)
        {
                protocol::PlayerState ps;
                if (!reader.read_player_state(ps))
                        break;

                auto it = players_.find(ps.id);
                if (it != players_.end())
                {
                        // Existing player - set up interpolation
                        new_players[ps.id]             = it->second;
                        new_players[ps.id].current_pos = it->second.render_pos;
                        new_players[ps.id].target_pos  = ps.position;
                        new_players[ps.id].score       = ps.score;
                        new_players[ps.id].last_update = now;
                }
                else
                {
                        // New player - no interpolation needed
                        InterpolatedPlayer ip;
                        ip.id              = ps.id;
                        ip.current_pos     = ps.position;
                        ip.target_pos      = ps.position;
                        ip.render_pos      = ps.position;
                        ip.score           = ps.score;
                        ip.last_update     = now;
                        new_players[ps.id] = ip;

                        if (my_player_id_ == 0)
                        {
                                my_player_id_ = ps.id;
                                std::cout << "My player ID: " << my_player_id_ << "\n";
                        }
                }
        }
        players_ = std::move(new_players);

        // Update coins
        coins_.clear();
        for (int i = 0; i < coin_count; i++)
        {
                protocol::CoinState cs;
                if (!reader.read_coin_state(cs))
                        break;
                coins_[cs.id] = cs;
        }
}