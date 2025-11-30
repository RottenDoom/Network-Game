#include "client.h"
#include <iostream>
#include <cmath>

GameClient::GameClient(asio::io_context& io)
    : io_(io), socket_(io), latency_timer_(io), connected_(false), my_player_id_(0)
{
        next_input_seq_ = 1;
        ping_ms_        = 0.0f;
}

void GameClient::apply_local_input(float dx, float dy, float dt)
{
        std::lock_guard<std::mutex> lock(mutex_);
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
        {
                std::lock_guard<std::mutex> lock(mutex_);
                connected_ = true;
        }

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

        uint32_t seq;
        {
                std::lock_guard<std::mutex> lock(mutex_);
                seq = next_input_seq_++;
                // Store pending input for reconciliation
                pending_inputs_.push_back(PendingInput{seq, dx, dy, timestamp});
        }

        msg.write_uint32(timestamp);
        msg.write_uint32(seq);
        msg.finalize();

        asio::async_write(socket_, asio::buffer(msg.data), [](asio::error_code, std::size_t) {});
}

void GameClient::update_interpolation(float dt)
{
        std::lock_guard<std::mutex> lock(mutex_);
        // If we have server snapshots, use them to interpolate remote players
        if (!snapshot_buffer.empty())
        {
                // Use the latest server timestamp as reference
                uint64_t latest_ts = snapshot_buffer.back().server_ts_ms;
                uint64_t target_ts = (latest_ts > static_cast<uint64_t>(INTERP_DELAY.count()))
                                         ? (latest_ts - static_cast<uint64_t>(INTERP_DELAY.count()))
                                         : latest_ts;

                // For each remote player, interpolate/extrapolate based on snapshots
                for (auto& [id, player] : players_)
                {
                        // Local player handled by prediction/reconciliation; skip snapshot interpolation
                        if (id == my_player_id_)
                                continue;

                        // Find two snapshots s0, s1 that bracket target_ts
                        Snapshot* s0 = nullptr;
                        Snapshot* s1 = nullptr;
                        for (size_t i = 0; i + 1 < snapshot_buffer.size(); ++i)
                        {
                                if (snapshot_buffer[i].server_ts_ms <= target_ts &&
                                    snapshot_buffer[i + 1].server_ts_ms >= target_ts)
                                {
                                        s0 = &snapshot_buffer[i];
                                        s1 = &snapshot_buffer[i + 1];
                                        break;
                                }
                        }

                        if (s0 && s1)
                        {
                                // Both snapshots contain positions for this player?
                                auto it0 = s0->player_positions.find(id);
                                auto it1 = s1->player_positions.find(id);
                                if (it0 != s0->player_positions.end() && it1 != s1->player_positions.end())
                                {
                                        double span = double(s1->server_ts_ms - s0->server_ts_ms);
                                        double t    = (span > 0.0) ? double(target_ts - s0->server_ts_ms) / span : 0.0;
                                        // Lerp positions
                                        player.render_pos.x = static_cast<float>(it0->second.x +
                                                                                 (it1->second.x - it0->second.x) * t);
                                        player.render_pos.y = static_cast<float>(it0->second.y +
                                                                                 (it1->second.y - it0->second.y) * t);
                                        continue;
                                }
                        }

                        // If we couldn't bracket, try extrapolation from last two snapshots
                        if (snapshot_buffer.size() >= 2)
                        {
                                const Snapshot& last = snapshot_buffer.back();
                                const Snapshot& prev = snapshot_buffer[snapshot_buffer.size() - 2];
                                auto itLast          = last.player_positions.find(id);
                                auto itPrev          = prev.player_positions.find(id);
                                if (itLast != last.player_positions.end() && itPrev != prev.player_positions.end())
                                {
                                        double dt_sec = double(last.server_ts_ms - prev.server_ts_ms) / 1000.0;
                                        if (dt_sec > 0.0)
                                        {
                                                float vx = (itLast->second.x - itPrev->second.x) /
                                                           static_cast<float>(dt_sec);
                                                float vy = (itLast->second.y - itPrev->second.y) /
                                                           static_cast<float>(dt_sec);
                                                double extra_ms     = double(target_ts > last.server_ts_ms
                                                                                 ? (target_ts - last.server_ts_ms)
                                                                                 : 0);
                                                double extra_s      = extra_ms / 1000.0;
                                                player.render_pos.x = itLast->second.x +
                                                                      vx * static_cast<float>(extra_s);
                                                player.render_pos.y = itLast->second.y +
                                                                      vy * static_cast<float>(extra_s);
                                                continue;
                                        }
                                }
                        }

                        // Fallback: use target_pos with light smoothing to avoid snapping
                        const float SMOOTH_FALLBACK  = 8.0f;
                        float alpha                  = 1.0f - std::exp(-SMOOTH_FALLBACK * dt);
                        player.render_pos.x         += (player.target_pos.x - player.render_pos.x) * alpha;
                        player.render_pos.y         += (player.target_pos.y - player.render_pos.y) * alpha;
                }

                // Trim old snapshots (keep ~1s of history)
                uint64_t keep_ms = 1000;
                while (snapshot_buffer.size() > 1 &&
                       (snapshot_buffer.back().server_ts_ms - snapshot_buffer.front().server_ts_ms) > keep_ms)
                {
                        snapshot_buffer.pop_front();
                }
                return;
        }

        // If no snapshots are available, fallback to previous smoothing behaviour
        {
                // Exponential smoothing parameters
                const float SMOOTHING_K_REMOTE = 6.0f;   // higher => faster catch-up for remote players
                const float SMOOTHING_K_LOCAL  = 10.0f;  // local reconciliation smoothing

                for (auto& [id, player] : players_)
                {
                        if (dt <= 0.0f)
                                continue;
                        float k              = (id == my_player_id_) ? SMOOTHING_K_LOCAL : SMOOTHING_K_REMOTE;
                        float alpha          = 1.0f - std::exp(-k * dt);
                        float dx             = player.target_pos.x - player.render_pos.x;
                        float dy             = player.target_pos.y - player.render_pos.y;
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
                                         {
                                                 std::lock_guard<std::mutex> lock(mutex_);
                                                 connected_ = false;
                                         }
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

                                         // process immediately — server simulates latency
                                         process_message(full_msg);
                                         read_header();
                                 }
                                 else
                                 {
                                         std::lock_guard<std::mutex> lock(mutex_);
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
                        {
                                std::lock_guard<std::mutex> lock(mutex_);
                                my_player_id_ = assigned_id;
                        }
                        std::cout << "Assigned player ID: " << assigned_id << "\n";
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

        std::lock_guard<std::mutex> lock(mutex_);

        // Update players
        std::map<uint32_t, InterpolatedPlayer> new_players;
        auto dist = [](const protocol::Vec2& a, const protocol::Vec2& b)
        {
                float dx = a.x - b.x;
                float dy = a.y - b.y;
                return std::sqrt(dx * dx + dy * dy);
        };

        uint32_t server_last_seq_for_me = 0;
        uint32_t server_last_ts_for_me  = 0;

        Snapshot snap;
        snap.server_ts_ms = timestamp;

        for (int i = 0; i < player_count; i++)
        {
                protocol::PlayerState ps;
                if (!reader.read_player_state(ps))
                        break;

                if (ps.id == my_player_id_)
                {
                        server_last_seq_for_me = ps.last_processed_input_seq;
                        server_last_ts_for_me  = ps.last_processed_input_ts;
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
                }

                // fill snapshot
                snap.player_positions[ps.id] = ps.position;
                snap.player_scores[ps.id]    = ps.score;
                snap.player_last_seq[ps.id]  = ps.last_processed_input_seq;
        }
        players_ = std::move(new_players);

        // push snapshot into buffer
        snapshot_buffer.push_back(std::move(snap));

        // Reconciliation for local player: measure ping, drop acknowledged inputs and reapply pending ones
        if (my_player_id_ != 0 && server_last_seq_for_me != 0)
        {
                // Measure RTT. Prefer server-echoed input timestamp if available.
                uint32_t now_ms = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                            std::chrono::steady_clock::now().time_since_epoch())
                                                            .count());

                if (server_last_ts_for_me != 0)
                {
                        uint32_t rtt = (now_ms >= server_last_ts_for_me) ? (now_ms - server_last_ts_for_me) : 0;
                        if (ping_ms_ <= 0.0f)
                                ping_ms_ = static_cast<float>(rtt);
                        else
                                ping_ms_ = ping_ms_ * 0.8f + static_cast<float>(rtt) * 0.2f;
                }
                else
                {
                        // Fallback: compute RTT using pending input timestamps and acknowledged seq
                        uint32_t ack_seq = server_last_seq_for_me;
                        for (const auto& pi : pending_inputs_)
                        {
                                if (pi.seq == ack_seq)
                                {
                                        uint32_t rtt = (now_ms >= pi.timestamp) ? (now_ms - pi.timestamp) : 0;
                                        if (ping_ms_ <= 0.0f)
                                                ping_ms_ = static_cast<float>(rtt);
                                        else
                                                ping_ms_ = ping_ms_ * 0.8f + static_cast<float>(rtt) * 0.2f;
                                        break;
                                }
                        }
                }
                // ping_ms_ is already protected by mutex_ lock from handle_game_state()

                // Drop acknowledged inputs from the front of the queue
                while (!pending_inputs_.empty() && pending_inputs_.front().seq <= server_last_seq_for_me)
                        pending_inputs_.pop_front();

                auto it = players_.find(my_player_id_);
                if (it != players_.end())
                {
                        const float INPUT_DT     = 0.016f;  // assume ~60fps
                        const float PLAYER_SPEED = 150.0f;

                        protocol::Vec2 recon_pos = it->second.current_pos;
                        for (size_t i = 0; i < pending_inputs_.size(); ++i)
                        {
                                const auto& pi = pending_inputs_[i];
                                float dt       = 0.016f;
                                if (i + 1 < pending_inputs_.size())
                                        dt = (pending_inputs_[i + 1].timestamp - pi.timestamp) / 1000.0f;
                                else
                                        dt = 1.0f / 60.0f;  // fallback

                                float len = std::sqrt(pi.dx * pi.dx + pi.dy * pi.dy);
                                if (len > 0.001f)
                                {
                                        float nx     = pi.dx / len;
                                        float ny     = pi.dy / len;
                                        recon_pos.x += nx * PLAYER_SPEED * dt;
                                        recon_pos.y += ny * PLAYER_SPEED * dt;
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

std::map<uint32_t, InterpolatedPlayer> GameClient::get_players() const
{
        std::lock_guard<std::mutex> lock(mutex_);
        return players_;
}

std::map<uint32_t, protocol::CoinState> GameClient::get_coins() const
{
        std::lock_guard<std::mutex> lock(mutex_);
        return coins_;
}

bool GameClient::is_connected() const
{
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_;
}

uint32_t GameClient::get_my_id() const
{
        std::lock_guard<std::mutex> lock(mutex_);
        return my_player_id_;
}

float GameClient::get_ping_ms() const
{
        std::lock_guard<std::mutex> lock(mutex_);
        return ping_ms_;
}