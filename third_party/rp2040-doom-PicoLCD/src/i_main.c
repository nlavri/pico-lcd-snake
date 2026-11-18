//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Main program, simply calls D_DoomMain high level loop.
//

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#if !LIB_PICO_STDLIB
#include "SDL.h"
#else
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#if DOOM_EP0172_EARLY_SCREEN
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#endif
#include "pico/sem.h"
#include "pico/multicore.h"
#if PICO_ON_DEVICE
#include "hardware/vreg.h"
#endif
#endif
#if USE_PICO_NET
#include "piconet.h"
#endif
#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"

#if DOOM_EP0172_EARLY_SCREEN
#include "pico-screens/screen.h"
#endif

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
//

void D_DoomMain (void);

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
#if !DOOM_NO_AUDIO
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCK"));
#endif
#endif

int main(int argc, char **argv)
{
    // save arguments
#if !NO_USE_ARGS
    myargc = argc;
    myargv = argv;
#endif
#if PICO_ON_DEVICE
#if DOOM_EP0172_SAFE_CLOCK
    // Start the EP-0172 port at the RP2040's normal clock while validating
    // its display transport.  Other DOOM targets retain the 270 MHz setup.
    set_sys_clock_khz(125000, true);
#else
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    // todo pause? is this the cause of the cold start isue?
#if DOOM_EP0172_PICO2
    set_sys_clock_khz(300000, true);
#else
    set_sys_clock_khz(270000, true);
#endif
#endif
#ifdef PICO_SMPS_MODE_PIN
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, 1);
#endif
#endif
#if LIB_PICO_STDIO
    stdio_init_all();
#endif
#if DOOM_EP0172_EARLY_SCREEN
    // Initialize the EP-0172 before input and WAD setup so probe-free boot
    // diagnostics remain observable even if later initialization stalls.
    I_initScreen();
#if DOOM_EP0172_AUTOSTART
    // A frame watchdog records the last synchronization point in scratch[0].
    // After a watchdog reset, show the persistent visual code briefly, then
    // consume it and reboot into a clean run.  BOOTSEL flashing does not clear
    // watchdog scratch registers, so freezing here would otherwise prevent a
    // newly flashed diagnostic build from ever starting.
    if (watchdog_caused_reboot()
        && watchdog_hw->scratch[1] == 0x45503172u) {
        uint32_t failed_stage = watchdog_hw->scratch[0];
        if (failed_stage > 16) failed_stage = 16;
        for (uint32_t stage = 1; stage <= failed_stage; ++stage) {
            I_showBootStage((uint8_t)stage);
        }
        sleep_ms(1500);
        watchdog_hw->scratch[1] = 0;
        watchdog_reboot(0, 0, 0);
        while (true) tight_loop_contents();
    }
#endif
    I_showBootStage(1);
#endif
#if PICO_BUILD
    I_Init();
#if DOOM_EP0172_EARLY_SCREEN
    I_showBootStage(2);
#endif
#endif
#if USE_PICO_NET
    // do init early to set pulls
    piconet_init();
#endif
//!
    // Print the program version and exit.
    //
    if (M_ParmExists("-version") || M_ParmExists("--version")) {
        puts(PACKAGE_STRING);
        exit(0);
    }

#if !NO_USE_ARGS
    M_FindResponseFile();
#endif

    #ifdef SDL_HINT_NO_SIGNAL_HANDLERS
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    #endif

    // start doom

#if DOOM_EP0172_EARLY_SCREEN
    I_showBootStage(3);
#endif
    D_DoomMain ();

    return 0;
}

