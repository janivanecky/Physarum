#pragma once

#include <stdint.h>
  
// API definition
namespace midi {
    int  init();
    void release();

    float get_controller_state(uint8_t controller_id);
    bool  get_button_state(uint8_t button_id);
}

// AKAI MIDIMIX bindings of individual control elements to their IDs
#define AKAI_MIDIMIX_KNOB_0_0 0x10
#define AKAI_MIDIMIX_KNOB_1_0 0x11
#define AKAI_MIDIMIX_KNOB_2_0 0x12
#define AKAI_MIDIMIX_KNOB_0_1 0x14
#define AKAI_MIDIMIX_KNOB_1_1 0x15
#define AKAI_MIDIMIX_KNOB_2_1 0x16
#define AKAI_MIDIMIX_KNOB_0_2 0x18
#define AKAI_MIDIMIX_KNOB_1_2 0x19
#define AKAI_MIDIMIX_KNOB_2_2 0x1A
#define AKAI_MIDIMIX_KNOB_0_3 0x1C
#define AKAI_MIDIMIX_KNOB_1_3 0x1D
#define AKAI_MIDIMIX_KNOB_2_3 0x1E
#define AKAI_MIDIMIX_KNOB_0_4 0x2E
#define AKAI_MIDIMIX_KNOB_1_4 0x2F
#define AKAI_MIDIMIX_KNOB_2_4 0x30
#define AKAI_MIDIMIX_KNOB_0_5 0x32
#define AKAI_MIDIMIX_KNOB_1_5 0x33
#define AKAI_MIDIMIX_KNOB_2_5 0x34
#define AKAI_MIDIMIX_KNOB_0_6 0x36
#define AKAI_MIDIMIX_KNOB_1_6 0x37
#define AKAI_MIDIMIX_KNOB_2_6 0x38
#define AKAI_MIDIMIX_KNOB_0_7 0x3A
#define AKAI_MIDIMIX_KNOB_1_7 0x3B
#define AKAI_MIDIMIX_KNOB_2_7 0x3C

#define AKAI_MIDIMIX_SLIDER_0 0x13
#define AKAI_MIDIMIX_SLIDER_1 0x17
#define AKAI_MIDIMIX_SLIDER_2 0x1B
#define AKAI_MIDIMIX_SLIDER_3 0x1F
#define AKAI_MIDIMIX_SLIDER_4 0x31
#define AKAI_MIDIMIX_SLIDER_5 0x35
#define AKAI_MIDIMIX_SLIDER_6 0x39
#define AKAI_MIDIMIX_SLIDER_7 0x3D
#define AKAI_MIDIMIX_SLIDER_MASTER 0x3E

#define AKAI_MIDIMIX_BUTTON_MUTE_0 0x01
#define AKAI_MIDIMIX_BUTTON_REC_0  0x03
#define AKAI_MIDIMIX_BUTTON_MUTE_1 0x04
#define AKAI_MIDIMIX_BUTTON_REC_1  0x06
#define AKAI_MIDIMIX_BUTTON_MUTE_2 0x07
#define AKAI_MIDIMIX_BUTTON_REC_2  0x09
#define AKAI_MIDIMIX_BUTTON_MUTE_3 0x0A
#define AKAI_MIDIMIX_BUTTON_REC_3  0x0C
#define AKAI_MIDIMIX_BUTTON_MUTE_4 0x0D
#define AKAI_MIDIMIX_BUTTON_REC_4  0x0F
#define AKAI_MIDIMIX_BUTTON_MUTE_5 0x10
#define AKAI_MIDIMIX_BUTTON_REC_5  0x12
#define AKAI_MIDIMIX_BUTTON_MUTE_6 0x13
#define AKAI_MIDIMIX_BUTTON_REC_6  0x15
#define AKAI_MIDIMIX_BUTTON_MUTE_7 0x16
#define AKAI_MIDIMIX_BUTTON_REC_7  0x18

#define AKAI_MIDIMIX_BUTTON_SOLO       0x1B
#define AKAI_MIDIMIX_BUTTON_BANK_LEFT  0x19
#define AKAI_MIDIMIX_BUTTON_BANK_RIGHT 0x1A

// Implementation
#ifdef MIDI_DEFINE
#include <mmsystem.h>

// Error codes
#define MIDI_OK                    0
#define MIDI_NO_DEVICE_FOUND       1
#define MIDI_CANNOT_OPEN_HANDLE    2
#define MIDI_CANNOT_START_TRANSFER 3

// Buffer size used for communicating in bytes
#define MIDI_BUFFER_SIZE 1000

// Globally stored handle to a device
static HMIDIIN device_handle;

// Storing button states
static uint8_t controller_states[256];
static bool    button_states[256];

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    uint8_t status_byte = (uint8_t)(dwParam1 & 0x000000FF);
    uint8_t first_byte = (uint8_t)((dwParam1 & 0x0000FF00) >> 8);
    uint8_t second_byte = (uint8_t)((dwParam1 & 0x00FF0000) >> 16);
    if (wMsg == MIM_DATA) {
        uint8_t controller_id = first_byte;
        uint8_t value = second_byte;
        switch (status_byte) {
            case 0xB0:
            {
                // Controller value changed
                controller_states[controller_id] = value;
            }
            break;
            case 0x90:
            {
                // Button pressed
                button_states[controller_id] = true;
            }
            break;
            case 0x80:
            {
                // Button released
                button_states[controller_id] = false;
            }
            break;
        };
    }
    return;
}

int midi::init() {
    UINT num = midiInGetNumDevs();
    if (num == 0) {
        return MIDI_NO_DEVICE_FOUND;
    }

    MMRESULT result = midiInOpen(&device_handle, 0, (DWORD_PTR)&MidiInProc, 0, CALLBACK_FUNCTION | MIDI_IO_STATUS);
    if (result != MMSYSERR_NOERROR) {
        return MIDI_CANNOT_OPEN_HANDLE;
    }

    MIDIHDR buffer_info = {};
    buffer_info.lpData = (LPSTR)memory::alloc_heap<uint8_t>(MIDI_BUFFER_SIZE);
    buffer_info.dwBufferLength = sizeof(uint8_t) * MIDI_BUFFER_SIZE;

    result = midiInPrepareHeader(device_handle, &buffer_info, sizeof(buffer_info));
    if (result != MMSYSERR_NOERROR) {
        return MIDI_CANNOT_START_TRANSFER;
    }

    result = midiInAddBuffer(device_handle, &buffer_info, sizeof(buffer_info));
    if (result != MMSYSERR_NOERROR) {
        return MIDI_CANNOT_START_TRANSFER;
    }

    result = midiInStart(device_handle);
    if (result != MMSYSERR_NOERROR) {
        return MIDI_CANNOT_START_TRANSFER;
    }

    return MIDI_OK;
}

void midi::release() {
    midiInClose(device_handle);
}

float midi::get_controller_state(uint8_t controller_id) {
    return float(controller_states[controller_id] / 127.0f);
}

bool midi::get_button_state(uint8_t button_id) {
    return button_states[button_id];
}

#endif
