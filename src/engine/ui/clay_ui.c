// Clay UI backend (included once into the unity build). This is the single home of
// Clay's implementation, plus the text-measure hook and the render-command -> r2d
// translation. See clay_ui.h for the hot-reload notes.

// Clay's implementation lives here. It's third-party single-header code, so silence
// the warnings it trips under -Wall -Wextra rather than let them bury ours.
#define CLAY_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#include "../../../third_party/clay/clay.h"
#pragma clang diagnostic pop

#include "clay_ui.h"
#include "../debug.h"
#include <stdlib.h>

static Color clay_color(Clay_Color c) {
    return (Color){ c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
}

// Clay measures text through this; our bitmap font is a fixed 8x8 glyph advanced by
// font->advance, scaled uniformly. fontSize is treated as the target pixel height.
static Clay_Dimensions clay_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Renderer *r = (Renderer *)userData;
    f32 scale = (f32)config->fontSize / (f32)r->font->glyph_h;
    return (Clay_Dimensions){
        .width  = (f32)text.length * (f32)r->font->advance * scale,
        .height = (f32)r->font->glyph_h * scale,
    };
}

static void clay_on_error(Clay_ErrorData e) {
    LOG_ERROR("clay: %.*s", (int)e.errorText.length, e.errorText.chars);
}

b32 clay_ui_init(ClayUI *ui, Renderer *r) {
    if (!ui || !r) return MACH_FALSE;
    uint32_t need = Clay_MinMemorySize();
    ui->memory = malloc(need);
    if (!ui->memory) {
        LOG_ERROR("clay_ui_init: failed to allocate %u bytes", need);
        return MACH_FALSE;
    }
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(need, ui->memory);
    Clay_Dimensions dims = { (f32)r->width, (f32)r->height };
    ui->ctx = Clay_Initialize(arena, dims, (Clay_ErrorHandler){ clay_on_error, NULL });
    Clay_SetMeasureTextFunction(clay_measure_text, r);
    ui->ready = MACH_TRUE;
    LOG_INFO("clay ui initialized (%u bytes)", need);
    return MACH_TRUE;
}

void clay_ui_shutdown(ClayUI *ui) {
    if (!ui) return;
    free(ui->memory);
    ui->memory = NULL;
    ui->ctx = NULL;
    ui->ready = MACH_FALSE;
}

void clay_ui_begin(ClayUI *ui, Renderer *r, Clay_Vector2 mouse, b32 mouse_down) {
    if (!ui || !ui->ready) return;
    // Re-point Clay's globals every frame: after a hot reload the new library's copy
    // of Clay starts with an empty context and a dangling measure-fn pointer.
    Clay_SetCurrentContext(ui->ctx);
    Clay_SetMeasureTextFunction(clay_measure_text, r);
    Clay_SetLayoutDimensions((Clay_Dimensions){ (f32)r->width, (f32)r->height });
    Clay_SetPointerState(mouse, mouse_down);
    Clay_BeginLayout();
}

void clay_ui_render(ClayUI *ui, Renderer *r) {
    if (!ui || !ui->ready) return;
    // deltaTime is only used by Clay's animation/transition features, which we don't
    // use yet, so 0 is fine.
    Clay_RenderCommandArray cmds = Clay_EndLayout(0.0f);
    for (i32 i = 0; i < cmds.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        Clay_BoundingBox b = cmd->boundingBox;
        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            r2d_fill_rect(r, b.x, b.y, b.width, b.height,
                          clay_color(cmd->renderData.rectangle.backgroundColor));
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *t = &cmd->renderData.text;
            char buf[256];
            i32 n = t->stringContents.length;
            if (n > (i32)sizeof(buf) - 1) n = (i32)sizeof(buf) - 1;
            memcpy(buf, t->stringContents.chars, (size_t)n);
            buf[n] = '\0';
            f32 scale = (f32)t->fontSize / (f32)r->font->glyph_h;
            r2d_text(r, b.x, b.y, scale, buf, clay_color(t->textColor));
        } break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *bd = &cmd->renderData.border;
            Vec4 col = clay_color(bd->color);
            if (bd->width.top)    r2d_fill_rect(r, b.x, b.y, b.width, (f32)bd->width.top, col);
            if (bd->width.bottom) r2d_fill_rect(r, b.x, b.y + b.height - (f32)bd->width.bottom, b.width, (f32)bd->width.bottom, col);
            if (bd->width.left)   r2d_fill_rect(r, b.x, b.y, (f32)bd->width.left, b.height, col);
            if (bd->width.right)  r2d_fill_rect(r, b.x + b.width - (f32)bd->width.right, b.y, (f32)bd->width.right, b.height, col);
        } break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            r2d_clip_begin(r, b.x, b.y, b.width, b.height);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            r2d_clip_end(r);
            break;
        default:
            break;  // images, custom, overlays: unused by our HUD for now
        }
    }
}
