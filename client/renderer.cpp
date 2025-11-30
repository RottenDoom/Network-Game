#include "renderer.h"
#include <iostream>
#include <cmath>
#include <sstream>

#ifdef USE_SDL_TTF
#include <SDL2/SDL_ttf.h>
#endif

Renderer::Renderer(int width, int height)
    : window_(nullptr), renderer_(nullptr), width_(width), height_(height), running_(true)
{
}

Renderer::~Renderer()
{
        cleanup();
}

bool Renderer::init()
{
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
                std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
                return false;
        }

#ifdef USE_SDL_TTF
        font_        = nullptr;
        font_loaded_ = false;
        if (TTF_Init() < 0)
        {
                std::cerr << "TTF init failed: " << TTF_GetError() << "\n";
        }
        else
        {
                // Try to load a font from assets; if missing, font_loaded_ remains false
                const char* font_path = "assets/Roboto-Regular.ttf";
                font_                 = TTF_OpenFont(font_path, 16);
                if (font_)
                        font_loaded_ = true;
                else
                        std::cerr << "Failed to open font '" << font_path << "': " << TTF_GetError() << "\n";
        }
#endif

        window_ = SDL_CreateWindow(
            "Coin Collector", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_, height_, SDL_WINDOW_SHOWN);

        if (!window_)
        {
                std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
                return false;
        }

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer_)
        {
                std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
                return false;
        }

        return true;
}

void Renderer::cleanup()
{
        if (renderer_)
                SDL_DestroyRenderer(renderer_);
        if (window_)
                SDL_DestroyWindow(window_);
#ifdef USE_SDL_TTF
        if (font_)
                TTF_CloseFont(font_);
        TTF_Quit();
#endif
        SDL_Quit();
}

void Renderer::draw_circle(int cx, int cy, int radius, SDL_Color color)
{
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);

        for (int w = 0; w < radius * 2; w++)
        {
                for (int h = 0; h < radius * 2; h++)
                {
                        int dx = radius - w;
                        int dy = radius - h;
                        if ((dx * dx + dy * dy) <= (radius * radius))
                        {
                                SDL_RenderDrawPoint(renderer_, cx + dx, cy + dy);
                        }
                }
        }
}

void Renderer::render(const GameClient& client)
{
        // Clear screen
        SDL_SetRenderDrawColor(renderer_, 20, 20, 30, 255);
        SDL_RenderClear(renderer_);

        if (!client.is_connected())
        {
                // draw_text("Connecting...", 300, 250, {255, 255, 255, 255});
                SDL_RenderPresent(renderer_);
                return;
        }

        // Draw coins
        for (const auto& [id, coin] : client.get_coins())
        {
                draw_circle(static_cast<int>(coin.position.x), static_cast<int>(coin.position.y), 15, {255, 215, 0, 255}
                            // Gold
                );
        }

        // Draw players
        uint32_t my_id = client.get_my_id();
        for (const auto& [id, player] : client.get_players())
        {
                SDL_Color color = (id == my_id) ? SDL_Color{0, 255, 0, 255}       // Green for local player
                                                : SDL_Color{100, 150, 255, 255};  // Blue for remote players

                draw_circle(static_cast<int>(player.render_pos.x), static_cast<int>(player.render_pos.y), 25, color);

                // Draw score above player
                std::string score_text = "P" + std::to_string(id) + ": " + std::to_string(player.score);
                draw_text(score_text,
                          static_cast<int>(player.render_pos.x) - 30,
                          static_cast<int>(player.render_pos.y) - 50,
                          {255, 255, 255, 255});
        }

        // Draw instructions
        draw_text("Use WASD or Arrow Keys to move", 10, 10, {200, 200, 200, 255});

        // // Draw ping and general UI
        // std::ostringstream ss;
        // ss << "Ping: " << static_cast<int>(client.get_ping_ms()) << " ms";
        // draw_text(ss.str(), width_ - 150, 10, {200, 200, 200, 255});

        // Draw total scores in top-left
        int y = 40;
        for (const auto& [id, player] : client.get_players())
        {
                std::string s = "P" + std::to_string(id) + ": " + std::to_string(player.score);
                draw_text(s, 10, y, {240, 240, 240, 255});
                y += 20;
        }

        SDL_RenderPresent(renderer_);
}

void Renderer::draw_text(const std::string& text, int x, int y, SDL_Color color)
{
#ifdef USE_SDL_TTF
        if (!font_loaded_ || !font_)
                return;

        SDL_Color col     = color;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), col);
        if (!surf)
                return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        if (!tex)
        {
                SDL_FreeSurface(surf);
                return;
        }
        SDL_Rect dst{x, y, surf->w, surf->h};
        SDL_FreeSurface(surf);
        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
#else
        // TTF not available: nothing to draw
        (void)text;
        (void)x;
        (void)y;
        (void)color;
#endif
}