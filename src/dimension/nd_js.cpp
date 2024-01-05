#include "nd_sdl.hpp"

NDSDL::NDSDL(int slot, uint32_t* vram) : slot(slot), vram(vram), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL) {}

void NDSDL::repaint(void) {
}

void NDSDL::init(void) {
}

void NDSDL::uninit(void) {
}

void NDSDL::destroy(void) {
}

void NDSDL::pause(bool pause) {
}

void NDSDL::resize(float scale) {
}

void nd_sdl_repaint(void) {
}

void nd_sdl_resize(float scale) {
}

void nd_sdl_show(void) {
}

void nd_sdl_hide(void) {
}

void nd_sdl_destroy(void) {
}
