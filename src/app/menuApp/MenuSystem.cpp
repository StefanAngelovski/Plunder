#include "include/MenuSystem.h"

MenuSystem::MenuSystem(SDL_Renderer* r, TTF_Font* f) : renderer(r), font(f) {}
void MenuSystem::pushScreen(std::shared_ptr<Screen> screen) { screenStack.push_back(screen); }
void MenuSystem::popScreen() { if (screenStack.size() > 1) screenStack.pop_back(); }
void MenuSystem::replaceScreen(std::shared_ptr<Screen> screen) { if (!screenStack.empty()) screenStack.pop_back(); screenStack.push_back(screen); }
void MenuSystem::render() { if (!screenStack.empty()) screenStack.back()->render(renderer, font); SDL_RenderPresent(renderer); }
void MenuSystem::handleInput(const SDL_Event& e) { if (!screenStack.empty()) screenStack.back()->handleInput(e, *this); }
bool MenuSystem::isRunning() const { return !screenStack.empty(); }
SDL_Renderer* MenuSystem::getRenderer() const { return renderer; }