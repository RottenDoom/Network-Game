#include "client.h"
#include <iostream>
#include <cmath>

GameClient::GameClient(asio::io_context& io)
    : io_(io), socket_(io), latency_timer_(io), connected_(false), my_player_id_(0)
{
        next_input_seq_ = 1;
}

void GameClient::apply_local_input(float dx, float dy, float dt)
{
        if (my_player_id_ == 0)
                return;

        auto it = players_.find(my_player_id_);
        if (it == players_.end())
                return;

        // Simple client-side prediction: move the local player's render and
        // current positions immediately using the same speed used on server.
        const float PLAYER_SPEED = 150.0f;

        float len                = std::sqrt(dx * dx + dy * dy);
        if (len > 0.001f)
        {
                float nx                  = dx / len;
                float ny                  = dy / len;

                it->second.current_pos.x += nx * PLAYER_SPEED * dt;
                it->second.current_pos.y += ny * PLAYER_SPEED * dt;

                it->second.render_pos.x  += nx * PLAYER_SPEED * dt;
                it->second.render_pos.y  += ny * PLAYER_SPEED * dt;
        }
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

        // Build message with sequence number and timestamp
        protocol::MessageBuffer msg;
        msg.write_header(protocol::MessageType::CLIENT_INPUT);
        msg.write_float(dx);
        msg.write_float(dy);

        auto now           = std::chrono::steady_clock::now();
        uint32_t timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

        uint32_t seq = next_input_seq_++;
        msg.write_uint32(timestamp);
        msg.write_uint32(seq);
        msg.finalize();

        // Store pending input for reconciliation
        pending_inputs_.push_back(PendingInput{seq, dx, dy, timestamp});

        // Simulate 200ms send latency using a per-message timer so multiple
        // pending sends don't cancel each other.
        auto timer = std::make_shared<asio::steady_timer>(io_);
        timer->expires_after(std::chrono::milliseconds(200));
        timer->async_wait(
            [this, data = msg.data, timer](auto ec)
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

        // Exponential smoothing parameters
        const float SMOOTHING_K_REMOTE = 6.0f;   // higher => faster catch-up for remote players
        const float SMOOTHING_K_LOCAL  = 10.0f;  // local reconciliation smoothing

        for (auto& [id, player] : players_)
        {
                // If no time passed, skip
                if (dt <= 0.0f)
                        continue;

                // Compute per-frame smoothing alpha from a time constant k
                float k     = (id == my_player_id_) ? SMOOTHING_K_LOCAL : SMOOTHING_K_REMOTE;
                float alpha = 1.0f - std::exp(-k * dt);

                // Distance to server target
                float dx      = player.target_pos.x - player.render_pos.x;
                float dy      = player.target_pos.y - player.render_pos.y;
                float dist_sq = dx * dx + dy * dy;

                // If we're extremely far from the target (e.g., teleport), snap to avoid slow crawl
                const float SNAP_DISTANCE_SQ = 200.0f * 200.0f;
                if (dist_sq > SNAP_DISTANCE_SQ)
                {
                        player.render_pos = player.target_pos;
                }
                else
                {
                        // Smoothly move render_pos toward target_pos
                        player.render_pos.x += dx * alpha;
                        player.render_pos.y += dy * alpha;
                }
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

                                         // Simulate 200ms receive latency using a per-message timer
                                         auto timer = std::make_shared<asio::steady_timer>(io_);
                                         timer->expires_after(std::chrono::milliseconds(200));
                                         timer->async_wait(
                                             [this, msg = std::move(full_msg), timer](auto ec)
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

        case protocol::MessageType::SERVER_START_GAME:
        {
                uint32_t assigned_id = 0;
                if (reader.read_uint32(assigned_id))
                {
                        my_player_id_ = assigned_id;
                        std::cout << "Assigned player ID: " << my_player_id_ << "\n";
                }
                break;
        }

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
        auto dist = [](const protocol::Vec2& a, const protocol::Vec2& b)
        {
                float dx = a.x - b.x;
                float dy = a.y - b.y;
                return std::sqrt(dx * dx + dy * dy);
        };

        uint32_t server_last_seq_for_me = 0;

        for (int i = 0; i < player_count; i++)
        {
                protocol::PlayerState ps;
                if (!reader.read_player_state(ps))
                        break;

                if (ps.id == my_player_id_)
                {
                        server_last_seq_for_me = ps.last_processed_input_seq;
                }

                auto it = players_.find(ps.id);
                if (it != players_.end())
                {
                        // Existing player - decide how to interpolate / reconcile
                        const InterpolatedPlayer& prev = it->second;

                        // Distance from server position to what we are currently rendering
                        float server_to_render      = dist(ps.position, prev.render_pos);
                        float server_to_prev_target = dist(ps.position, prev.target_pos);

                        // Small deadzone to ignore micro-corrections that cause jitter
                        const float DEADZONE         = 1.0f;    // pixels
                        const float RECONCILE_SNAP   = 100.0f;  // if off by this much, snap
                        const float RECONCILE_SMOOTH = 5.0f;    // if off by this much, smooth-correct

                        if (ps.id == my_player_id_)
                        {
                                // Local player: reconcile predicted position with authoritative server
                                float pred_diff = dist(ps.position, prev.current_pos);

                                if (pred_diff > RECONCILE_SNAP)
                                {
                                        // Way out of sync: snap to server position
                                        InterpolatedPlayer ip;
                                        ip.id              = ps.id;
                                        ip.current_pos     = ps.position;
                                        ip.target_pos      = ps.position;
                                        ip.render_pos      = ps.position;
                                        ip.score           = ps.score;
                                        ip.last_update     = now;
                                        new_players[ps.id] = ip;
                                }
                                else if (pred_diff > RECONCILE_SMOOTH)
                                {
                                        // Moderate desync: smoothly correct by interpolating towards server
                                        new_players[ps.id]             = prev;
                                        new_players[ps.id].current_pos = prev.render_pos;
                                        new_players[ps.id].target_pos  = ps.position;
                                        new_players[ps.id].score       = ps.score;
                                        new_players[ps.id].last_update = now;
                                }
                                else
                                {
                                        // Small or no desync: keep predicted position but nudge target
                                        new_players[ps.id]             = prev;
                                        new_players[ps.id].target_pos  = ps.position;
                                        new_players[ps.id].score       = ps.score;
                                        new_players[ps.id].last_update = now;
                                }
                        }
                        else
                        {
                                // Remote player: ignore tiny corrections to avoid buzzing
                                if (server_to_render < DEADZONE)
                                {
                                        new_players[ps.id]       = prev;
                                        new_players[ps.id].score = ps.score;
                                }
                                else
                                {
                                        new_players[ps.id]             = prev;
                                        new_players[ps.id].current_pos = prev.render_pos;
                                        new_players[ps.id].target_pos  = ps.position;
                                        new_players[ps.id].score       = ps.score;
                                        new_players[ps.id].last_update = now;
                                }
                        }
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

                        // `my_player_id_` is set from the server via SERVER_START_GAME.
                        // Do not guess it here.
                }
        }
        players_ = std::move(new_players);

        // Reconciliation for local player: drop acknowledged inputs and reapply pending ones
        if (my_player_id_ != 0 && server_last_seq_for_me != 0)
        {
                // Drop acknowledged inputs from the front of the queue
                while (!pending_inputs_.empty() && pending_inputs_.front().seq <= server_last_seq_for_me)
                        pending_inputs_.pop_front();

                auto it = players_.find(my_player_id_);
                if (it != players_.end())
                {
                        // Start from the authoritative position reported by server
                        protocol::Vec2 recon_pos = it->second.current_pos;

                        const float INPUT_DT     = 0.016f;  // assume ~60Hz
                        const float PLAYER_SPEED = 150.0f;

                        for (const auto& pi : pending_inputs_)
                        {
                                float len = std::sqrt(pi.dx * pi.dx + pi.dy * pi.dy);
                                if (len > 0.001f)
                                {
                                        float nx     = pi.dx / len;
                                        float ny     = pi.dy / len;
                                        recon_pos.x += nx * PLAYER_SPEED * INPUT_DT;
                                        recon_pos.y += ny * PLAYER_SPEED * INPUT_DT;
                                }
                        }

                        it->second.current_pos = recon_pos;
                        it->second.render_pos  = recon_pos;
                        it->second.target_pos  = recon_pos;
                }
        }

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