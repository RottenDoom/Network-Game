#pragma once
#include "client.h"
#include <SDL2/SDL.h>
// #include <SDL2/SDL_ttf.h>
#include <string>

class Renderer
{
public:
        Renderer(int width, int height);
        ~Renderer();

        bool init();
        void render(const GameClient& client);
        void cleanup();

        bool is_running() const { return running_; }
        void set_running(bool r) { running_ = r; }

private:
        void draw_circle(int cx, int cy, int radius, SDL_Color color);
        // void draw_text(const std::string& text, int x, int y, SDL_Color color);

        SDL_Window* window_;
        SDL_Renderer* renderer_;
        // TTF_Font* font_;

        int width_;
        int height_;
        bool running_;
};