#ifndef DSP_PSK31_H
#define DSP_PSK31_H
#include "SDT.h"

/*
 * PSK31 primitives (data tables + clean DBPSK demod) -- ported from
 * T41_SDR/psk31.cpp + psk31.h.
 *
 * Scope of this port (intentional): the legitimately reusable parts of T41's
 * PSK31 code -- the standard PSK31 varicode lookup tables, the varicode
 * encoder/decoder, and the differential-BPSK symbol decoder -- ported into
 * Phoenix's DataBlock layout and stripped of Serial.print debug spam.
 *
 * What this port deliberately does NOT include:
 *   - T41's five experimental Psk31PhaseShiftDetector* implementations.
 *     They use ad-hoc thresholds with hand-tuned magic constants and no
 *     symbol-clock synchronization. A proper Phoenix implementation should
 *     be written from scratch using Phoenix's FFT/Goertzel infrastructure.
 *   - Symbol-clock recovery. T41's timing_recovery_cc() is commented out
 *     in the upstream source; without it neither codebase can reliably
 *     decode real-world PSK31 signals.
 *   - WAV-file test path (commented out in T41's source).
 *   - UI / display surface for decoded text. Phoenix has no decoded-text
 *     pane today; that's a separate UI port.
 *
 * Phoenix integration: nothing currently calls into this module. It builds
 * the primitives (varicode tables, DBPSK demod, decoder state) so a future
 * Phoenix-native PSK31 implementation has them ready. To make PSK31
 * end-to-end, future work will need:
 *   (a) A new modulation enum value (PSK31 = 6) in SDT.h
 *   (b) A dispatcher case in Demodulate() in DSP.cpp
 *   (c) A narrow CW-style audio bandpass filter at the PSK31 audio center
 *   (d) Symbol-clock synchronization (Gardner / early-late / Mueller-Muller)
 *   (e) A decoded-text display pane
 */

/**
 * @brief Push one demodulated PSK31 symbol bit into the varicode decoder.
 * @param symbol  The next demodulated bit (treated as nonzero = 1, zero = 0).
 * @return ASCII character if a complete varicode codeword was matched on
 *         this push, otherwise 0.
 * @note Maintains internal shift-register state across calls. Call
 *       ResetPSK31Decoder() between independent decode sessions.
 *       Ported from T41_SDR/psk31.cpp::psk31_varicode_decoder_push (Serial
 *       debug removed). Standard PSK31 varicode (G3PLX / 1998).
 */
char PSK31VaricodeDecoderPush(uint8_t symbol);

/**
 * @brief Encode an ASCII string as a sequence of PSK31 varicode bits.
 * @param[in]  input            Pointer to ASCII input bytes.
 * @param[out] output           Buffer to receive encoded bits (one bit per byte, 0 or 1).
 * @param[in]  input_size       Number of input bytes available.
 * @param[in]  output_max_size  Maximum number of output bits the caller will accept.
 * @param[out] input_processed  Number of input bytes actually consumed.
 * @param[out] output_size      Number of output bits actually produced.
 * @note Each input character expands to bitcount + 2 bits (codeword + 2 zero
 *       inter-character bits). Stops early if output_max_size is exhausted.
 *       Ported from T41_SDR/psk31.cpp::psk31_varicode_encoder_u8_u8 unchanged.
 */
void PSK31VaricodeEncode(const uint8_t *input, uint8_t *output,
                         int input_size, int output_max_size,
                         int *input_processed, int *output_size);

/**
 * @brief Differential-BPSK symbol decoder.
 * @param[in]  data        Pointer to a DataBlock containing complex baseband
 *                         samples. data->I[i] is the real component, data->Q[i]
 *                         the imaginary component, for i in [0, data->N).
 * @param[out] output      Buffer to receive decoded bits (one bit per sample,
 *                         0 if differential phase change > +/-PI/2 else 1).
 *                         Must be at least data->N bytes.
 * @note Maintains last-phase state across calls (each block is differentially
 *       decoded relative to the previous block's last sample). Call
 *       ResetPSK31Decoder() between independent decode sessions.
 *       Adapted from T41_SDR/psk31.cpp::dbpsk_decoder_c_u8 (which used
 *       interleaved I/Q) to Phoenix's separate I[]/Q[] DataBlock layout.
 *       This is *symbol* decoding only -- there is no symbol-clock recovery,
 *       so callers must downsample data->N to one sample per PSK31 symbol
 *       (~31.25 baud) before invoking this. See header comment.
 */
void PSK31DBPSKDecode(const DataBlock *data, uint8_t *output);

/**
 * @brief Reset the varicode decoder shift register and DBPSK last-phase state.
 * @note Call between independent decode sessions to avoid spurious matches
 *       at the start of a new burst.
 */
void ResetPSK31Decoder(void);

#endif /* DSP_PSK31_H */
