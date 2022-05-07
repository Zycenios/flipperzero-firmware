#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>

#include <gui/gui.h>
#include <input/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "function_waveforms.h"

#define NUM_PARAMS 4

enum WaveformParam
{
	TypeParam=0,
	FrequencyParam=1,
	MagnitudeParam=2,
	OffsetParam=3,
};

typedef struct {
	enum WaveformType curWaveform;
	enum WaveformParam curParam;
	bool outputOn;
	float waveformFreq;
	float waveformMag;
	float waveformOffset;
} State;

static void render_callback(Canvas* canvas, void* ctx) {
	State* state = (State*)ctx;
	
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
	
	int labelOffset = 8;
	int valueOffset = 20;
	int paramSpacing = 32;
	int paramOffset = 2;
	int boxWidth = paramSpacing;
	int boxHeight = 11;
	
	// Draw the parameter labels
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str(canvas, paramOffset, labelOffset, "Type");
	canvas_draw_str(canvas, paramSpacing + paramOffset, labelOffset, "Freq");
	canvas_draw_str(canvas, 2*paramSpacing + paramOffset, labelOffset, "Mag");
	canvas_draw_str(canvas, 3*paramSpacing + paramOffset, labelOffset, "Offset");
	
	// Draw parameter values
	char tempValue[16];
	canvas_draw_str(canvas, paramOffset, valueOffset, waveformNames[state->curWaveform]);
	
	sprintf(tempValue, "%4.1f", state->waveformFreq);
	canvas_draw_str(canvas, paramSpacing + paramOffset, valueOffset, tempValue);
	
	sprintf(tempValue, "%3.1f", state->waveformMag);
	canvas_draw_str(canvas, 2*paramSpacing + paramOffset, valueOffset, tempValue);
	
	sprintf(tempValue, "%3.1f", state->waveformOffset);
	canvas_draw_str(canvas, 3*paramSpacing + paramOffset, valueOffset, tempValue);
	
	// Draw a box around the selected parameter
	canvas_draw_frame(canvas, state->curParam * paramSpacing, 0, boxWidth, boxHeight);
	
	
	canvas_set_font(canvas, FontPrimary);
	if (state->outputOn)
	{
		canvas_draw_str(canvas, 40, 40, "Output On");
	}
	else
	{
		canvas_draw_str(canvas, 40, 40, "Output Off");
	}
}

static void input_callback(InputEvent* input_event, void* ctx) {
    osMessageQueueId_t event_queue = ctx;
	
    osMessageQueuePut(event_queue, input_event, 0, 0);
}

uint8_t txBuffer[2];

// Set the output voltage and settings of the MCP4901 using SPI.
static bool writeDAC(uint16_t dacValue, bool buff, bool gain, bool shdn)
{
	txBuffer[0] = (buff << 6) | (gain << 5) | (shdn << 4) | (dacValue >> 4);
	txBuffer[1] = (dacValue << 4);
	
	furi_hal_spi_acquire(&furi_hal_spi_bus_handle_DAC);
	bool ret = furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_DAC, txBuffer, sizeof(uint8_t)*2, 100);
	furi_hal_spi_release(&furi_hal_spi_bus_handle_DAC);
	return ret;
}


static bool writeDACVoltage(float voltage)
{
	uint8_t dacValue = MAX(MIN((int)(voltage*256.0f/5.0f), 255), 0);
	return writeDAC(dacValue, 0, 1, 1);
}



static bool shutdownDAC()
{
	return writeDAC(0, 0, 1, 0);
}


int32_t function_generator_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(InputEvent), NULL);
	State state;
	state.curWaveform = DC;
	state.curParam = TypeParam;
	state.outputOn = false;
	state.waveformFreq = 1; // Hz
	state.waveformMag = 1; // V
	state.waveformOffset = 0; // V	
	
	// Function generator settings
	int lastWriteTime = millis();
	float curSampleFloat = 0;
	int curSample = 0;
	int samplesPerPeriod = WAVEFORM_SAMPLES;
	
	float magnitudeMax = 5.0f;
	float magnitudeMin = 0.0f;
	float magnitudeStep = 0.1f;
	float offsetMax = 5.0f;
	float offsetMin = -5.0f;
	float offsetStep = 0.1f;
	float frequencyMax = 100.0f;
	float frequencyMin = 0.0f;
	float frequencyStep = 0.1f;
	
	// UI settings
	int screenUpdatePeriod = 200;
	int lastScreenTime = 0;
	
	// Turn on the 5V pin
	furi_hal_power_enable_otg();
	
	// Write the LDAC pin low to enable DAC updating
	hal_gpio_init_simple(&gpio_ext_pc3, GpioModeOutputPushPull);
	hal_gpio_write(&gpio_ext_pc3, 0);
	
	/* Initialize DAC SPI
	* Preset: `furi_hal_spi_preset_1edge_low_2m`
	* 
	* miso: pa6
	* mosi: pa7
	* sck: pb3
	* cs:  pb2 (software controlled)
	*/
	
	furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_DAC);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);
		
        if(event_status == osOK)
		{
            // handle events
			
			// Press back to exit
			if (event.type == InputTypeShort &&
            event.key == InputKeyBack) {
                break;
            }
			
			// Press ok to toggle waveform output
			if (event.type == InputTypeLong &&
			event.key == InputKeyOk) {
				if (state.outputOn)
				{
					state.outputOn = false;
					shutdownDAC();
				}
				else
				{
					state.outputOn = true;
					curSampleFloat = 0.0f;
					curSample = 0;
					lastWriteTime = millis();
				}
			}
			
			// Press right/left to switch the current parameter
			if ((event.type == InputTypeShort || event.type == InputTypeRepeat) &&
			event.key == InputKeyRight) {
				state.curParam++;
				if (state.curParam >= NUM_PARAMS)
				{
					state.curParam = 0;
				}
			}
			
			if ((event.type == InputTypeShort || event.type == InputTypeRepeat) &&
			event.key == InputKeyLeft) {
				if (state.curParam == 0)
				{
					state.curParam = NUM_PARAMS - 1;
				}
				else
				{
					state.curParam--;
				}
			}
			
			// Press up/down to change the parameter value
			if ((event.type == InputTypeShort || event.type == InputTypeRepeat) &&
			(event.key == InputKeyUp || event.key == InputKeyDown))
			{
				switch (state.curParam) {
					case TypeParam:
						if (event.key == InputKeyUp)
						{
							state.curWaveform++;
							if (state.curWaveform >= NUM_WAVEFORMS)
							{
								state.curWaveform = 0;
							}
						}
						else
						{
							if (state.curWaveform == 0)
							{
								state.curWaveform = NUM_WAVEFORMS - 1;
							}
							else
							{
								state.curWaveform--;
							}
						}
						break;
					
					case FrequencyParam:
						if (event.key == InputKeyUp)
						{
							state.waveformFreq += frequencyStep;
							if (state.waveformFreq > frequencyMax)
							{
								state.waveformFreq = frequencyMax;
							}
						}
						else
						{
							state.waveformFreq -= frequencyStep;
							if (state.waveformFreq < frequencyMin)
							{
								state.waveformFreq = frequencyMin;
							}
						}
						break;
					
					case MagnitudeParam:
						if (event.key == InputKeyUp)
						{
							state.waveformMag += magnitudeStep;
							if (state.waveformMag > magnitudeMax)
							{
								state.waveformMag = magnitudeMax;
							}
						}
						else
						{
							state.waveformMag -= magnitudeStep;
							if (state.waveformMag < magnitudeMin)
							{
								state.waveformMag = magnitudeMin;
							}
						}
						break;
					
					case OffsetParam:
						if (event.key == InputKeyUp)
						{
							state.waveformOffset += offsetStep;
							if (state.waveformOffset > offsetMax)
							{
								state.waveformOffset = offsetMax;
							}
						}
						else
						{
							state.waveformOffset -= offsetStep;
							if (state.waveformOffset < offsetMin)
							{
								state.waveformOffset = offsetMin;
							}
						}
						break;
				}
			}
        } else 
		{
            // event timeout
        }
		
		// Update the function generator every iteration
		if (state.outputOn)
		{
			int newTime = millis();
			curSampleFloat += state.waveformFreq * (newTime - lastWriteTime) / 1000.0f * samplesPerPeriod;
			if (curSampleFloat >= samplesPerPeriod)
			{
				curSampleFloat -= samplesPerPeriod;
			}
			curSample = (int)curSampleFloat;
			lastWriteTime = newTime;
			
			float outVoltage = state.waveformOffset + state.waveformMag * waveformVals[state.curWaveform][curSample];
			writeDACVoltage(outVoltage);
		}
		
		// Update the screen every once in a while
		if (millis() - lastScreenTime >= screenUpdatePeriod)
		{
			view_port_update(view_port);
		}
    }
	
	// Turn off the 5V pin
	furi_hal_power_disable_otg();
	
	// Deinitialize external SPI handle
	furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_DAC);
	
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);

    return 0;
}
