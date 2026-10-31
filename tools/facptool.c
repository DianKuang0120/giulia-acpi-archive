/*
 * facptool.c - adjust FADT power management flags.
 *
 * Build:
 *   cc -O2 -o facptool facptool.c
 *
 * Commands (operate on FACP.aml in the current directory):
 *   ./facptool info              show the relevant flags
 *   ./facptool nolpids0          clear LOW_POWER_S0_IDLE_CAPABLE (bit 21)
 *   ./facptool lpids0            set   LOW_POWER_S0_IDLE_CAPABLE (bit 21)
 *
 * LOW_POWER_S0_IDLE_CAPABLE tells Windows the platform implements Modern
 * Standby (S0ix).  It was cleared once to stop a black screen that appeared
 * ~15 s after the shell started, but that made Windows fail during early boot
 * (the boot animation hung half way), so the flag is set again.  Whatever
 * causes the late black screen, it is not this flag alone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define OFF_FLAGS 0x70

#define HW_REDUCED_ACPI           (1U << 20)
#define LOW_POWER_S0_IDLE_CAPABLE (1U << 21)

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static int checksum_ok(const uint8_t *b, long len)
{
    uint8_t s = 0;
    for (long i = 0; i < len; i++) s = (uint8_t)(s + b[i]);
    return s == 0;
}

int main(int argc, char **argv)
{
    const char *path = (argc >= 3) ? argv[2] : "FACP.aml";
    if (argc < 2) {
        fprintf(stderr, "usage: %s info|nolpids0|lpids0 [FACP.aml]\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)len);
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); return 1; }
    fclose(f);

    uint32_t flags = get32(buf + OFF_FLAGS);

    if (strcmp(argv[1], "info") == 0) {
        printf("%s: %ld bytes, checksum %s\n", path, len, checksum_ok(buf, len) ? "ok" : "BAD");
        printf("  HW_REDUCED_ACPI           : %u\n", (flags & HW_REDUCED_ACPI) ? 1 : 0);
        printf("  LOW_POWER_S0_IDLE_CAPABLE : %u\n", (flags & LOW_POWER_S0_IDLE_CAPABLE) ? 1 : 0);
        return 0;
    }

    int set;
    if (strcmp(argv[1], "nolpids0") == 0)      set = 0;
    else if (strcmp(argv[1], "lpids0") == 0)   set = 1;
    else { fprintf(stderr, "unknown command: %s\n", argv[1]); return 1; }

    flags = set ? (flags | LOW_POWER_S0_IDLE_CAPABLE)
                : (flags & ~LOW_POWER_S0_IDLE_CAPABLE);
    put32(buf + OFF_FLAGS, flags);

    buf[9] = 0;
    uint8_t sum = 0;
    for (long i = 0; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    buf[9] = (uint8_t)(0x100 - sum);

    f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite(buf, 1, (size_t)len, f);
    fclose(f);

    printf("%s: LOW_POWER_S0_IDLE_CAPABLE=%d, flags=0x%08x, checksum %s\n",
           path, set, flags, checksum_ok(buf, len) ? "ok" : "BAD");
    return 0;
}
