// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — PS/2 Trackpad (TM1386) reader for Raspberry Pi Pico
//
// Reads movement data from a PS/2 trackpad and prints it over USB serial.
//
// Wiring:
//   Trackpad DATA  → GP2  (+ 10k pull-up to 3.3V)
//   Trackpad CLK   → GP3  (+ 10k pull-up to 3.3V)
//   Trackpad VCC   → 3V3 OUT
//   Trackpad GND   → GND
//
// Copyright (C) 2026 Arnav
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <cstdio>
#include "pico/stdlib.h"
#include "ps2.hpp"

// ---------------------------------------------------------------------------
// Pin configuration — change these to match your wiring
// ---------------------------------------------------------------------------
static constexpr uint DATA_PIN = 2;
static constexpr uint CLK_PIN  = 3;

// Onboard LED for status indication
static constexpr uint LED_PIN = 25;

// ---------------------------------------------------------------------------
// Helper: blink the LED a given number of times
// ---------------------------------------------------------------------------
static void led_blink(uint count, uint on_ms = 100, uint off_ms = 100) {
    for (uint i = 0; i < count; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(on_ms);
        gpio_put(LED_PIN, 0);
        sleep_ms(off_ms);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // Initialize USB serial (CDC) — printf goes over USB
    stdio_init_all();

    // Setup onboard LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // Wait for USB serial connection (with timeout)
    // Blink LED while waiting so you know the Pico is alive
    for (int i = 0; i < 20; i++) {  // ~4 seconds max
        if (stdio_usb_connected()) break;
        led_blink(1, 100, 100);
    }

    printf("\n");
    printf("===========================================\n");
    printf("  PS/2 Trackpad TM1386 — Pico USB Reader\n");
    printf("===========================================\n");
    printf("DATA pin: GP%d\n", DATA_PIN);
    printf("CLK  pin: GP%d\n", CLK_PIN);
    printf("\n");

    // Create PS/2 driver instance
    PS2 trackpad(pio0, 0, DATA_PIN, CLK_PIN);

    printf("[INIT] Initializing PS/2 PIO driver...\n");
    trackpad.init();
    printf("[INIT] PIO driver ready.\n");

    // Reset the trackpad
    printf("[INIT] Sending reset (0xFF)...\n");
    uint8_t device_id = 0;
    bool reset_ok = false;

    for (int attempt = 0; attempt < 3; attempt++) {
        if (trackpad.reset(device_id)) {
            reset_ok = true;
            break;
        }
        printf("[INIT] Reset attempt %d failed, retrying...\n", attempt + 1);
        sleep_ms(500);
    }

    if (reset_ok) {
        printf("[INIT] Reset OK. Device ID: 0x%02X\n", device_id);
        led_blink(2, 200, 100);  // 2 blinks = reset success
    } else {
        printf("[INIT] WARNING: Reset failed after 3 attempts.\n");
        printf("[INIT] Continuing anyway — trackpad may still work.\n");
        led_blink(5, 50, 50);  // 5 fast blinks = reset failed
    }

    // Attempt to identify and switch to Synaptics Absolute Mode
    uint8_t syn_minor, syn_model, syn_major;
    bool is_synaptics = trackpad.synaptics_identify(syn_minor, syn_model, syn_major);
    
    if (is_synaptics) {
        printf("[INIT] Synaptics Touchpad detected! (v%d.%d, model 0x%X)\n", syn_major, syn_minor, syn_model);
        printf("[INIT] Enabling Synaptics Absolute + W Mode...\n");
        // 0x81 = Absolute Mode (bit 7) | W Mode (bit 0)
        if (trackpad.synaptics_set_mode(0x81)) {
            printf("[INIT] Synaptics Absolute Mode enabled.\n");
        } else {
            printf("[INIT] WARNING: Failed to set Synaptics mode.\n");
        }
    } else {
        printf("[INIT] Standard PS/2 Mouse detected (no Synaptics signature).\n");
    }

    // Enable data reporting (stream mode)
    printf("[INIT] Enabling data reporting (0xF4)...\n");
    if (trackpad.enable_reporting()) {
        printf("[INIT] Data reporting enabled.\n");
    } else {
        printf("[INIT] WARNING: Enable reporting not ACK'd.\n");
    }

    printf("\n");
    printf("[READY] Listening for trackpad packets...\n");
    if (trackpad.is_synaptics_absolute()) {
        printf("[READY] Format: X:nnnn Y:nnnn Z:nnn FINGERS:n BTN:n\n");
    } else {
        printf("[READY] Format: X:±nnn Y:±nnn BTN:n\n");
        printf("[NOTE]  TM1386 single button reports as PS/2 middle-click — remapped to BTN.\n");
    }
    printf("\n");

    // LED solid ON = running
    gpio_put(LED_PIN, 1);

    // Main loop: continuously read and print packets
    PS2::Packet rel_pkt;
    PS2::SynapticsData abs_pkt;
    uint32_t packet_count = 0;

    while (true) {
        if (trackpad.is_synaptics_absolute()) {
            if (trackpad.read_synaptics_packet(abs_pkt, 500)) {
                packet_count++;
                
                int fingers = 0;
                if (abs_pkt.z > 0) { // Only count fingers if touching
                    if (abs_pkt.w == 0) fingers = 2;
                    else if (abs_pkt.w == 1) fingers = 3;
                    else fingers = 1; // W=2 is pen, W>=4 is normal width
                }

                // Any hardware button counts as BTN
                bool btn = abs_pkt.left || abs_pkt.right || abs_pkt.up || abs_pkt.down;

                printf("X:%04d Y:%04d Z:%03d FINGERS:%d BTN:%d\n",
                       abs_pkt.x, abs_pkt.y, abs_pkt.z, fingers, btn);

                gpio_put(LED_PIN, packet_count & 1);
            }
        } else {
            if (trackpad.read_packet(rel_pkt, 500)) {
                packet_count++;

                bool btn = rel_pkt.left || rel_pkt.right || rel_pkt.middle;
                printf("X:%+4d Y:%+4d BTN:%d", rel_pkt.dx, rel_pkt.dy, btn);
                if (rel_pkt.x_overflow || rel_pkt.y_overflow) printf(" [OVF]");
                printf("\n");

                gpio_put(LED_PIN, packet_count & 1);
            }
        }
    }

    return 0;
}
