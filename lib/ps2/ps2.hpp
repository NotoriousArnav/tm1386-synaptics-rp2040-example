// SPDX-License-Identifier: GPL-3.0-or-later
//
// ps2.hpp — Reusable PS/2 host driver for RP2040 (PIO-based)
//
// Usage:
//   #include "ps2.hpp"
//   PS2 trackpad(pio0, 0, /*data_pin=*/2, /*clk_pin=*/3);
//   trackpad.init();
//   trackpad.reset(id);
//   trackpad.enable_reporting();
//   PS2::Packet pkt;
//   if (trackpad.read_packet(pkt)) { /* use pkt.dx, pkt.dy, pkt.left, ... */ }
//
// Pin requirements:
//   - data_pin and clk_pin must be adjacent (clk_pin == data_pin + 1)
//   - External 4.7k-10k pull-up resistors to 3.3V recommended on both lines
//
// Copyright (C) 2026 Arnav
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <cstdint>
#include "hardware/pio.h"

class PS2 {
public:
    /// Decoded 3-byte PS/2 mouse movement packet.
    struct Packet {
        int  dx;          ///< X movement (-256 to +255)
        int  dy;          ///< Y movement (-256 to +255)
        bool left;        ///< Left button pressed
        bool right;       ///< Right button pressed
        bool middle;      ///< Middle button pressed
        bool x_overflow;  ///< X counter overflowed
        bool y_overflow;  ///< Y counter overflowed
    };

    /// Construct a PS/2 host driver. Does NOT touch hardware until init().
    ///
    /// @param pio       PIO instance (pio0 or pio1)
    /// @param sm        State machine index (0-3)
    /// @param data_pin  GPIO number for DATA line
    /// @param clk_pin   GPIO number for CLK line (must be data_pin + 1)
    PS2(PIO pio, uint sm, uint data_pin, uint clk_pin);

    /// Load PIO program, configure pins with pull-ups, start state machine.
    void init();

    /// Send a single byte to the PS/2 device (host-to-device protocol).
    /// Temporarily disables the PIO SM, bit-bangs the inhibit + data sequence,
    /// then re-enables the SM for receiving.
    ///
    /// @param data  Byte to send
    /// @return true if the device acknowledged with a line pull-low after stop bit
    bool send_byte(uint8_t data);

    /// Receive a single byte from the PS/2 device (reads PIO RX FIFO).
    ///
    /// @param[out] out         Received byte
    /// @param      timeout_ms  Timeout in milliseconds (0 = non-blocking check)
    /// @return true if a byte was received within the timeout
    bool recv_byte(uint8_t &out, uint32_t timeout_ms = 100);

    /// Send a command byte and wait for ACK (0xFA).
    ///
    /// @param cmd  Command byte to send
    /// @return true if 0xFA was received
    bool command(uint8_t cmd);

    /// Send a command byte + argument byte, waiting for ACK after each.
    ///
    /// @param cmd  Command byte
    /// @param arg  Argument byte
    /// @return true if both were acknowledged
    bool command(uint8_t cmd, uint8_t arg);

    /// Send 0xFF (Reset), wait for ACK + self-test result (0xAA) + device ID.
    ///
    /// @param[out] device_id  The device ID byte returned after reset
    /// @return true if reset sequence completed successfully
    bool reset(uint8_t &device_id);

    /// Send 0xF4 (Enable Data Reporting).
    ///
    /// @return true if acknowledged
    bool enable_reporting();

    /// Send 0xF5 (Disable Data Reporting).
    ///
    /// @return true if acknowledged
    bool disable_reporting();

    /// Read and parse one 3-byte standard PS/2 mouse movement packet.
    ///
    /// @param[out] pkt         Decoded packet
    /// @param      timeout_ms  Timeout for each byte
    /// @return true if a complete valid packet was received
    bool read_packet(Packet &pkt, uint32_t timeout_ms = 100);

private:
    PIO  pio_;
    uint sm_;
    uint data_pin_;
    uint clk_pin_;
    uint pio_offset_;

    /// Drain any stale bytes from the PIO RX FIFO.
    void flush_rx();
};
