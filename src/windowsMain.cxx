#ifdef _WIN32

#define SDL_MAIN_HANDLED 1
#include <SDL2/SDL_main.h>
#include <windows.h>

int main(int argc, char* argv[]);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return SDL_main(__argc, __argv);
}

#endif // ifdef _WIN32
