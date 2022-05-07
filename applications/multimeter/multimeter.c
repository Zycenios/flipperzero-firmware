#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>

#include <gui/gui.h>
#include <input/input.h>

#include <stdio.h>
#include <math.h>

typedef struct {
	char message[16];
} State;

static void render_callback(Canvas* canvas, void* ctx) {
	State* state = (State*)ctx;
	
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
	
	canvas_set_font(canvas, FontPrimary);
	canvas_draw_str(canvas, 40, 10, "Voltage:");
	
    canvas_set_font(canvas, FontBigNumbers);
	canvas_draw_str(canvas, 36, 30, state->message);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    osMessageQueueId_t event_queue = ctx;
	
    osMessageQueuePut(event_queue, input_event, 0, 0);
}

// Uses SPI to get a sample from the MCP3202.
//  Allocate tx_buffer and rx_buffer before calling this.
static uint16_t sampleADC(uint8_t* tx_buffer, uint8_t* rx_buffer)
{
	uint16_t out = 0x00;
	furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
	if (furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, tx_buffer, rx_buffer, sizeof(uint8_t)*3, 100))
	{
		out = (((uint16_t)rx_buffer[1]) << 8) + rx_buffer[2]; // rx_buffer[1] has the MSB of the 12-bit output.
	}
	furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
	return out; // Should be from 0 to 4097
}

int32_t multimeter_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(InputEvent), NULL);
	State state;
	sprintf(state.message, "0.000V");
	
	// Turn on the 5V pin
	furi_hal_power_enable_otg();
	
	// Multimeter settings
	int numSamples = 256;
	float adcResolution = 4097.0f; // Float since we divide by this for RMS
	float supplyVoltage = 5.0f;
	int measurementDelay = 250;
	int lastMeasurementTime = 0;
	
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
			
			if (event.type == InputTypeShort &&
            event.key == InputKeyBack) {
                break;
            }
        } else {
            // event timeout
        }
		
		if (millis() - lastMeasurementTime > measurementDelay)
		{
			// Take a bunch of samples and compute the RMS voltage
			float squareSum = 0.0f;
			float sample;
			for (int i = 0; i < numSamples; i++)
			{
				sample = ((float)sampleADC(ADCtx_buffer, ADCrx_buffer)) / adcResolution * supplyVoltage;
				squareSum += sample*sample;
			}
			float rmsVoltage = sqrtf(squareSum/((float)numSamples));
			sprintf(state.message, "%5.3f V", rmsVoltage);
			
			lastMeasurementTime = millis();
		}

        view_port_update(view_port);
    }
	
	// Turn off the 5V pin
	furi_hal_power_disable_otg();
	
	// Deinitialize external SPI handle
	furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
	
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);

    return 0;
}
