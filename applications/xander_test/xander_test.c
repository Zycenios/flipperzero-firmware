#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

typedef struct {
	uint8_t xpos;
	uint8_t ypos;
	uint8_t pressed;
} State;

static void render_callback(Canvas* canvas, void* ctx) {
	State* state = (State*)ctx;
	
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
	
	if (state->pressed)
	{
		canvas_draw_str(canvas, 0 + state->xpos, 12 + state->ypos, "Xander Test");
	}
	else
	{
		canvas_draw_str(canvas, 0 + state->xpos, 12 + state->ypos, "XanderTest");
	}
}

static void input_callback(InputEvent* input_event, void* ctx) {
    osMessageQueueId_t event_queue = ctx;
	
    osMessageQueuePut(event_queue, input_event, 0, 0);
}

int32_t xander_test_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(InputEvent), NULL);
	State state;
	state.xpos = 0;
	state.ypos = 0;
	state.pressed = 0;

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);
		
        if(event_status == osOK) {
            // handle events
			if(event.key == InputKeyUp) {
                if(state.ypos > 0) state.ypos--;
            }
			
			if(event.key == InputKeyDown) {
                if(state.ypos < 63) state.ypos++;
            }
			
			if(event.key == InputKeyLeft) {
                if(state.xpos > 0) state.xpos--;
            }
			
			if(event.key == InputKeyRight) {
                if(state.xpos < 127) state.xpos++;
            }
			
			if (event.type == InputTypeShort &&
            event.key == InputKeyBack) {
                break;
            }
			
			if (event.key == InputKeyOk)
			{
				state.pressed = !state.pressed;
			}
        } else {
            // event timeout
        }

        view_port_update(view_port);
    }
	
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);

    return 0;
}
