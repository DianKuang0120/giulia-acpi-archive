/*
 * gtdttool.c - repair the Qualcomm GTDT for SM8650.
 *
 * Build:
 *   cc -O2 -o gtdttool gtdttool.c
 *
 * Commands (operate on GTDT.aml in the current directory):
 *   ./gtdttool info                   show the timer entries
 *   ./gtdttool fix                    NEL2 -> PPI 26, set Always On, fix the
 *                                     platform timer block base
 *
 * The SM8635-derived table points the Non-Secure EL2 timer at PPI 28, which
 * does not exist on SM8650 (its arch timer PPIs are 29/30/27/26).  Bit 2 of
 * each timer's flags is "Always On": the arch timers on this platform are
 * never gated, so it must be set, otherwise the OS may try to power manage
 * them through firmware that has no such interface.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HDR_LEN 36

static uint8_t *load(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*len + 64);
    if (!buf || fread(buf, 1, *len, f) != *len) { fprintf(stderr, "read failed\n"); exit(1); }
    fclose(f);
    return buf;
}

static void save(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(buf, 1, len, f);
    fclose(f);
}

static void fix_checksum(uint8_t *buf, size_t len)
{
    buf[9] = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    buf[9] = (uint8_t)(0x100 - sum);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* GTDT: 36 header, 8 counter block, 4 reserved, then 4 x (gsiv, flags) */
#define OFF_SEC_EL1   48
#define OFF_NS_EL1    56
#define OFF_VIRT      64
#define OFF_NS_EL2    72
#define OFF_PLAT_CNT  88
#define OFF_PLAT_OFF  92

static const char *names[4] = { "Secure EL1", "Non-Secure EL1", "Virtual", "Non-Secure EL2" };

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s info|fix [GTDT.aml]\n", argv[0]);
        return 1;
    }
    const char *path = (argc >= 3) ? argv[2] : "GTDT.aml";
    size_t len;
    uint8_t *buf = load(path, &len);
    if (len < 96) { fprintf(stderr, "table too short\n"); return 1; }

    if (strcmp(argv[1], "info") == 0) {
        printf("%s: %zu bytes\n", path, len);
        for (int i = 0; i < 4; i++) {
            size_t o = OFF_SEC_EL1 + (size_t)i * 8;
            printf("  %-14s gsiv=%u flags=0x%x always_on=%u\n",
                   names[i], get32(buf + o), get32(buf + o + 4),
                   (get32(buf + o + 4) >> 2) & 1U);
        }
        uint32_t count = get32(buf + OFF_PLAT_CNT);
        uint32_t offset = get32(buf + OFF_PLAT_OFF);
        printf("  platform timers: count=%u offset=0x%x\n", count, offset);
        return 0;
    }

    if (strcmp(argv[1], "fix") == 0) {
        int changed = 0;

        /* SM8650 arch timer PPIs: 29 (S-EL1), 30 (NS-EL1), 27 (virt), 26 (NS-EL2) */
        static const uint32_t want[4] = { 29, 30, 27, 26 };
        for (int i = 0; i < 4; i++) {
            size_t o = OFF_SEC_EL1 + (size_t)i * 8;
            uint32_t gsiv = get32(buf + o);
            uint32_t flags = get32(buf + o + 4);
            uint32_t newflags = flags | (1U << 2);   /* Always On */
            if (gsiv != want[i]) {
                printf("  %-14s gsiv %u -> %u\n", names[i], gsiv, want[i]);
                put32(buf + o, want[i]);
                changed++;
            }
            if (flags != newflags) {
                put32(buf + o + 4, newflags);
                changed++;
            }
        }

        fix_checksum(buf, len);
        save(path, buf, len);

        uint8_t sum = 0;
        for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + buf[i]);
        printf("%s: %d field(s) changed, checksum %s\n",
               path, changed, sum == 0 ? "ok" : "BAD");
        return 0;
    }

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 1;
}
