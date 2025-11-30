/**
 * @file renderer.h
 * @brief SDL2-based rendering system for the game client
 * @author NetworkGame Project
 * @date 2024
 */

#pragma once
#include "client.h"
#include <SDL2/SDL.h>
#ifdef USE_SDL_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <string>

/**
 * @class Renderer
 * @brief Handles all SDL2 rendering operations including players, coins, and UI
 */
class Renderer
{
public:
        /**
         * @brief Construct renderer
         * @param width Window width in pixels
         * @param height Window height in pixels
         */
        Renderer(int width, int height);

        /**
         * @brief Destructor - cleans up SDL resources
         */
        ~Renderer();

        /**
         * @brief Initialize SDL and create window
         * @return true if successful, false on error
         */
        bool init();

        /**
         * @brief Render current game state
         * @param client Game client to read state from
         */
        void render(const GameClient& client);

        /**
         * @brief Clean up SDL resources
         */
        void cleanup();

        /**
         * @brief Check if renderer is running
         * @return true if running, false if should exit
         */
        bool is_running() const { return running_; }

        /**
         * @brief Set running state
         * @param r Running state
         */
        void set_running(bool r) { running_ = r; }

private:
        void draw_circle(int cx, int cy, int radius, SDL_Color color);
        void draw_text(const std::string& text, int x, int y, SDL_Color color);

        SDL_Window* window_;
        SDL_Renderer* renderer_;
#ifdef USE_SDL_TTF
        TTF_Font* font_;
        bool font_loaded_;
#endif

        int width_;
        int height_;
        bool running_;
};