#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>

#include <gui/gui.h>
#include <input/input.h>

#include <stdio.h>

typedef struct {
	uint8_t xpos;
	uint8_t ypos;
	uint8_t pressed;
	char message[16];
} State;

static void render_callback(Canvas* canvas, void* ctx) {
	State* state = (State*)ctx;
	
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
	
	if (state->pressed)
	{
		canvas_draw_str(canvas, 0 + state->xpos, 12 + state->ypos, state->message);
	}
	else
	{
		canvas_draw_str(canvas, 0 + state->xpos, 12 + state->ypos, state->message);
	}
}

static void input_callback(InputEvent* input_event, void* ctx) {
    osMessageQueueId_t event_queue = ctx;
	
    osMessageQueuePut(event_queue, input_event, 0, 0);
}

// Uses SPI to get a sample from the MCP3202.
//  Allocate tx_buffer and rx_buffer before calling this.
uint16_t sampleADC(uint8_t* tx_buffer, uint8_t* rx_buffer)
{
	
	if (!furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, tx_buffer, rx_buffer, sizeof(tx_buffer), 100))
	{
		return 0x00;
	}
	uint16_t out = ((uint16_t)rx_buffer[1] << 8) + rx_buffer[2]; // rx_buffer[1] has the MSB of the 12-bit output.
	return out; // Should be from 0 to 4097
}

int32_t oscilloscope_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(InputEvent), NULL);
	State state;
	state.xpos = 0;
	state.ypos = 0;
	state.pressed = 0;
	sprintf(state.message, "Hi!");
	
	/* Initialize external SPI
	* Preset: `furi_hal_spi_preset_1edge_low_2m`
	* 
	* miso: pa6
	* mosi: pa7
	* sck: pb3
	* cs:  pa4 (software controlled)
	*/
	furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
	
	// Set up ADC for fast sampling
	uint8_t ADCtx_buffer[3] = {1, 144, 0}; // Differential mode, channel 0 is IN+, only MSB.
	uint8_t ADCrx_buffer[3] = {0, 0, 0};

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
				uint16_t sample = sampleADC(ADCtx_buffer, ADCrx_buffer);
				sprintf(state.message, "%d", sample);
			}
        } else {
            // event timeout
        }

        view_port_update(view_port);
    }
	
	// Deinitialize external SPI handle
	furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
	
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);

    return 0;
}
