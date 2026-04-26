#include "SDT.h"
#include "DSP.h"          // for ApproxAtan2
#include "DSP_PSK31.h"

/*
 * PSK31 primitives (data tables + clean DBPSK demod).
 *
 * Ported from T41_SDR/psk31.cpp -- the legitimately reusable subset:
 *   - Standard PSK31 varicode lookup tables
 *   - Varicode encoder/decoder
 *   - DBPSK symbol decoder (adapted to Phoenix's separate I/Q DataBlock)
 *
 * Excluded: T41's experimental phase-shift detectors, commented-out timing
 * recovery, WAV-file test path, and Serial.print debug spam.
 *
 * Reference: G3PLX, "PSK31: A new radio-teletype mode" (1998).
 * Original source: libcsdr (https://github.com/ha7ilm/csdr).
 */

/* =================================================================
 * Varicode tables
 *
 * Each entry is { codeword, bitcount, ascii }. Codewords are read MSB-first
 * and are guaranteed to never contain two consecutive zero bits, so a
 * "00" sequence is the inter-character delimiter.
 *
 * Table is the standard G3PLX varicode (128 ASCII characters). Copied
 * verbatim from T41_SDR/psk31.cpp.
 * =================================================================
 */

typedef struct {
    unsigned long long code;
    int                bitcount;
    unsigned char      ascii;
} psk31_varicode_item_t;

static const psk31_varicode_item_t psk31_varicode_items[] = {
    { 0b1010101011, 10, 0x00 }, /* NUL */    { 0b1011011011, 10, 0x01 }, /* SOH */
    { 0b1011101101, 10, 0x02 }, /* STX */    { 0b1101110111, 10, 0x03 }, /* ETX */
    { 0b1011101011, 10, 0x04 }, /* EOT */    { 0b1101011111, 10, 0x05 }, /* ENQ */
    { 0b1011101111, 10, 0x06 }, /* ACK */    { 0b1011111101, 10, 0x07 }, /* BEL */
    { 0b1011111111, 10, 0x08 }, /* BS  */    { 0b11101111,    8, 0x09 }, /* TAB */
    { 0b11101,       5, 0x0a }, /* LF  */    { 0b1101101111, 10, 0x0b }, /* VT  */
    { 0b1011011101, 10, 0x0c }, /* FF  */    { 0b11111,       5, 0x0d }, /* CR  */
    { 0b1101110101, 10, 0x0e }, /* SO  */    { 0b1110101011, 10, 0x0f }, /* SI  */
    { 0b1011110111, 10, 0x10 }, /* DLE */    { 0b1011110101, 10, 0x11 }, /* DC1 */
    { 0b1110101101, 10, 0x12 }, /* DC2 */    { 0b1110101111, 10, 0x13 }, /* DC3 */
    { 0b1101011011, 10, 0x14 }, /* DC4 */    { 0b1101101011, 10, 0x15 }, /* NAK */
    { 0b1101101101, 10, 0x16 }, /* SYN */    { 0b1101010111, 10, 0x17 }, /* ETB */
    { 0b1101111011, 10, 0x18 }, /* CAN */    { 0b1101111101, 10, 0x19 }, /* EM  */
    { 0b1110110111, 10, 0x1a }, /* SUB */    { 0b1101010101, 10, 0x1b }, /* ESC */
    { 0b1101011101, 10, 0x1c }, /* FS  */    { 0b1110111011, 10, 0x1d }, /* GS  */
    { 0b1011111011, 10, 0x1e }, /* RS  */    { 0b1101111111, 10, 0x1f }, /* US  */
    { 0b1,           1, 0x20 }, /* sp  */    { 0b111111111,   9, 0x21 }, /* !   */
    { 0b101011111,   9, 0x22 }, /* "   */    { 0b111110101,   9, 0x23 }, /* #   */
    { 0b111011011,   9, 0x24 }, /* $   */    { 0b1011010101, 10, 0x25 }, /* %   */
    { 0b1010111011, 10, 0x26 }, /* &   */    { 0b101111111,   9, 0x27 }, /* '   */
    { 0b11111011,    8, 0x28 }, /* (   */    { 0b11110111,    8, 0x29 }, /* )   */
    { 0b101101111,   9, 0x2a }, /* *   */    { 0b111011111,   9, 0x2b }, /* +   */
    { 0b1110101,     7, 0x2c }, /* ,   */    { 0b110101,      6, 0x2d }, /* -   */
    { 0b1010111,     7, 0x2e }, /* .   */    { 0b110101111,   9, 0x2f }, /* /   */
    { 0b10110111,    8, 0x30 }, /* 0   */    { 0b10111101,    8, 0x31 }, /* 1   */
    { 0b11101101,    8, 0x32 }, /* 2   */    { 0b11111111,    8, 0x33 }, /* 3   */
    { 0b101110111,   9, 0x34 }, /* 4   */    { 0b101011011,   9, 0x35 }, /* 5   */
    { 0b101101011,   9, 0x36 }, /* 6   */    { 0b110101101,   9, 0x37 }, /* 7   */
    { 0b110101011,   9, 0x38 }, /* 8   */    { 0b110110111,   9, 0x39 }, /* 9   */
    { 0b11110101,    8, 0x3a }, /* :   */    { 0b110111101,   9, 0x3b }, /* ;   */
    { 0b111101101,   9, 0x3c }, /* <   */    { 0b1010101,     7, 0x3d }, /* =   */
    { 0b111010111,   9, 0x3e }, /* >   */    { 0b1010101111, 10, 0x3f }, /* ?   */
    { 0b1010111101, 10, 0x40 }, /* @   */    { 0b1111101,     7, 0x41 }, /* A   */
    { 0b11101011,    8, 0x42 }, /* B   */    { 0b10101101,    8, 0x43 }, /* C   */
    { 0b10110101,    8, 0x44 }, /* D   */    { 0b1110111,     7, 0x45 }, /* E   */
    { 0b11011011,    8, 0x46 }, /* F   */    { 0b11111101,    8, 0x47 }, /* G   */
    { 0b101010101,   9, 0x48 }, /* H   */    { 0b1111111,     7, 0x49 }, /* I   */
    { 0b111111101,   9, 0x4a }, /* J   */    { 0b101111101,   9, 0x4b }, /* K   */
    { 0b11010111,    8, 0x4c }, /* L   */    { 0b10111011,    8, 0x4d }, /* M   */
    { 0b11011101,    8, 0x4e }, /* N   */    { 0b10101011,    8, 0x4f }, /* O   */
    { 0b11010101,    8, 0x50 }, /* P   */    { 0b111011101,   9, 0x51 }, /* Q   */
    { 0b10101111,    8, 0x52 }, /* R   */    { 0b1101111,     7, 0x53 }, /* S   */
    { 0b1101101,     7, 0x54 }, /* T   */    { 0b101010111,   9, 0x55 }, /* U   */
    { 0b110110101,   9, 0x56 }, /* V   */    { 0b101011101,   9, 0x57 }, /* W   */
    { 0b101110101,   9, 0x58 }, /* X   */    { 0b101111011,   9, 0x59 }, /* Y   */
    { 0b1010101101, 10, 0x5a }, /* Z   */    { 0b111110111,   9, 0x5b }, /* [   */
    { 0b111101111,   9, 0x5c }, /* \   */    { 0b111111011,   9, 0x5d }, /* ]   */
    { 0b1010111111, 10, 0x5e }, /* ^   */    { 0b101101101,   9, 0x5f }, /* _   */
    { 0b1011011111, 10, 0x60 }, /* `   */    { 0b1011,        4, 0x61 }, /* a   */
    { 0b1011111,     7, 0x62 }, /* b   */    { 0b101111,      6, 0x63 }, /* c   */
    { 0b101101,      6, 0x64 }, /* d   */    { 0b11,          2, 0x65 }, /* e   */
    { 0b111101,      6, 0x66 }, /* f   */    { 0b1011011,     7, 0x67 }, /* g   */
    { 0b101011,      6, 0x68 }, /* h   */    { 0b1101,        4, 0x69 }, /* i   */
    { 0b111101011,   9, 0x6a }, /* j   */    { 0b10111111,    8, 0x6b }, /* k   */
    { 0b11011,       5, 0x6c }, /* l   */    { 0b111011,      6, 0x6d }, /* m   */
    { 0b1111,        4, 0x6e }, /* n   */    { 0b111,         3, 0x6f }, /* o   */
    { 0b111111,      6, 0x70 }, /* p   */    { 0b110111111,   9, 0x71 }, /* q   */
    { 0b10101,       5, 0x72 }, /* r   */    { 0b10111,       5, 0x73 }, /* s   */
    { 0b101,         3, 0x74 }, /* t   */    { 0b110111,      6, 0x75 }, /* u   */
    { 0b1111011,     7, 0x76 }, /* v   */    { 0b1101011,     7, 0x77 }, /* w   */
    { 0b11011111,    8, 0x78 }, /* x   */    { 0b1011101,     7, 0x79 }, /* y   */
    { 0b111010101,   9, 0x7a }, /* z   */    { 0b1010110111, 10, 0x7b }, /* {   */
    { 0b110111011,   9, 0x7c }, /* |   */    { 0b1010110101, 10, 0x7d }, /* }   */
    { 0b1011010111, 10, 0x7e }, /* ~   */    { 0b1110110101, 10, 0x7f }, /* DEL */
};
static const int N_PSK31_VARICODE_ITEMS =
    (int)(sizeof(psk31_varicode_items) / sizeof(psk31_varicode_items[0]));

/*
 * Bitmask helper: psk31_varicode_masklen_helper[n] = (1ULL << n) - 1, for
 * n in [0, 63]. Used to mask the low n bits of the decoder's shift register
 * when comparing against a candidate codeword.
 *
 * This is precomputed (rather than (1ULL<<n)-1 at runtime) to match T41's
 * structure exactly. Total size: 64 * 8 = 512 bytes.
 */
static const unsigned long long psk31_varicode_masklen_helper[64] = {
    0x0000000000000000ULL, 0x0000000000000001ULL, 0x0000000000000003ULL, 0x0000000000000007ULL,
    0x000000000000000fULL, 0x000000000000001fULL, 0x000000000000003fULL, 0x000000000000007fULL,
    0x00000000000000ffULL, 0x00000000000001ffULL, 0x00000000000003ffULL, 0x00000000000007ffULL,
    0x0000000000000fffULL, 0x0000000000001fffULL, 0x0000000000003fffULL, 0x0000000000007fffULL,
    0x000000000000ffffULL, 0x000000000001ffffULL, 0x000000000003ffffULL, 0x000000000007ffffULL,
    0x00000000000fffffULL, 0x00000000001fffffULL, 0x00000000003fffffULL, 0x00000000007fffffULL,
    0x0000000000ffffffULL, 0x0000000001ffffffULL, 0x0000000003ffffffULL, 0x0000000007ffffffULL,
    0x000000000fffffffULL, 0x000000001fffffffULL, 0x000000003fffffffULL, 0x000000007fffffffULL,
    0x00000000ffffffffULL, 0x00000001ffffffffULL, 0x00000003ffffffffULL, 0x00000007ffffffffULL,
    0x0000000fffffffffULL, 0x0000001fffffffffULL, 0x0000003fffffffffULL, 0x0000007fffffffffULL,
    0x000000ffffffffffULL, 0x000001ffffffffffULL, 0x000003ffffffffffULL, 0x000007ffffffffffULL,
    0x00000fffffffffffULL, 0x00001fffffffffffULL, 0x00003fffffffffffULL, 0x00007fffffffffffULL,
    0x0000ffffffffffffULL, 0x0001ffffffffffffULL, 0x0003ffffffffffffULL, 0x0007ffffffffffffULL,
    0x000fffffffffffffULL, 0x001fffffffffffffULL, 0x003fffffffffffffULL, 0x007fffffffffffffULL,
    0x00ffffffffffffffULL, 0x01ffffffffffffffULL, 0x03ffffffffffffffULL, 0x07ffffffffffffffULL,
    0x0fffffffffffffffULL, 0x1fffffffffffffffULL, 0x3fffffffffffffffULL, 0x7fffffffffffffffULL,
};

/* =================================================================
 * Decoder state (persists across calls)
 * =================================================================
 */
static unsigned long long s_status_shr   = 0;     /* varicode shift register */
static float              s_last_phase   = 0.0f;  /* DBPSK previous-sample phase */

/* =================================================================
 * Public API
 * =================================================================
 */

void ResetPSK31Decoder(void) {
    s_status_shr = 0;
    s_last_phase = 0.0f;
}

char PSK31VaricodeDecoderPush(uint8_t symbol) {
    s_status_shr = (s_status_shr << 1) | (symbol ? 1ULL : 0ULL);

    /* If the low 12 bits are all zero we can't yet have matched a codeword
     * (the shortest codeword is 1 bit + 2 trailing zeros = 3 bits, so 12
     * zeros means we haven't seen any signal yet). Bail early. */
    if ((s_status_shr & 0xFFFULL) == 0ULL) {
        return 0;
    }

    for (int i = 0; i < N_PSK31_VARICODE_ITEMS; i++) {
        unsigned long long candidate = psk31_varicode_items[i].code << 2;
        unsigned long long mask = psk31_varicode_masklen_helper[
            (psk31_varicode_items[i].bitcount + 4) & 63];
        unsigned long long got = s_status_shr & mask;
        if (candidate == got) {
            s_status_shr = 0;  /* reset shift register on match */
            return (char)psk31_varicode_items[i].ascii;
        }
    }
    return 0;
}

void PSK31VaricodeEncode(const uint8_t *input, uint8_t *output,
                         int input_size, int output_max_size,
                         int *input_processed, int *output_size) {
    *output_size = 0;
    for (*input_processed = 0; *input_processed < input_size; (*input_processed)++) {
        for (int ci = 0; ci < N_PSK31_VARICODE_ITEMS; ci++) {
            const psk31_varicode_item_t *cv = &psk31_varicode_items[ci];
            if (input[*input_processed] != cv->ascii) continue;

            /* Each output is bitcount data bits + 2 trailing zero bits. */
            const int total_bits = cv->bitcount + 2;
            if (output_max_size < total_bits) return;
            for (int bi = 0; bi < total_bits; bi++) {
                output[*output_size] = (bi < cv->bitcount)
                    ? (uint8_t)((cv->code >> (cv->bitcount - bi - 1)) & 1ULL)
                    : 0;
                (*output_size)++;
                output_max_size--;
            }
            break;
        }
    }
}

void PSK31DBPSKDecode(const DataBlock *data, uint8_t *output) {
    if (data == nullptr || output == nullptr || data->N == 0) return;

    for (unsigned i = 0; i < data->N; i++) {
        float phase = ApproxAtan2(data->Q[i], data->I[i]);
        float dphase = phase - s_last_phase;
        /* Wrap into (-PI, PI]. */
        while (dphase < -(float)PI) dphase += 2.0f * (float)PI;
        while (dphase >=  (float)PI) dphase -= 2.0f * (float)PI;

        /* Differential bit: 1 if no significant phase change, 0 if ~PI flip. */
        if (dphase > ((float)PI / 2.0f) || dphase < -((float)PI / 2.0f)) {
            output[i] = 0;
        } else {
            output[i] = 1;
        }
        s_last_phase = phase;
    }
}
