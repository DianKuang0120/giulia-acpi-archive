/*
 * apictool.c - inspect and repair the Qualcomm MADT (APIC) table.
 *
 * Build:
 *   cc -O2 -o apictool apictool.c
 *
 * Commands (operate on APIC.aml in the current directory unless a file is
 * given as the last argument):
 *
 *   ./apictool info              list every subtable
 *   ./apictool fixlen            GICC Length -> real stride (82, or 80 after shrink)
 *   ./apictool shrink            rewrite each GICC from 82 to the standard 80 bytes
 *   ./apictool gicr BASE STEP    give each GICC its own redistributor base
 *   ./apictool trim N            keep only the first N GICC entries
 *   ./apictool strip 0x0E        drop every subtable of the given type
 *
 * Known problems this repairs:
 *   - SM8635-derived GICC entries say Length=52 but are laid out 82 bytes
 *     apart; a parser that trusts Length lands mid-entry after the first CPU.
 *   - every GICC carries the same GICRBaseAddress; each core needs its own.
 *   - GICC length 82 is non-standard; 80 (ACPI 6.0+) drops the TRBE field.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HDR_LEN      44     /* ACPI header + LocalApicAddress + Flags */
#define GICC_MIN     52
#define GICC_82      82
#define GICC_80      80
#define GICC_OFF_GICR  0x3C
#define GICC_OFF_MPIDR 0x44
#define GICC_OFF_EFF   0x4C

static uint8_t *load(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*len + 256);
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

static void fix_tail(uint8_t *buf, size_t len)
{
    uint32_t l = (uint32_t)len;
    buf[4] = (uint8_t)l; buf[5] = (uint8_t)(l >> 8);
    buf[6] = (uint8_t)(l >> 16); buf[7] = (uint8_t)(l >> 24);
    buf[9] = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    buf[9] = (uint8_t)(0x100 - sum);
}

static int checksum_ok(const uint8_t *buf, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + buf[i]);
    return sum == 0;
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t get64(const uint8_t *p)
{
    return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32;
}

static void put64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* Real distance to the next subtable.  GICC entries of the SM8635 table are
 * 82 bytes apart no matter what their Length byte says, so never trust it
 * for GICC. */
static size_t stride(const uint8_t *buf, size_t off)
{
    uint8_t type = buf[off];
    uint8_t len = buf[off + 1];
    if (type == 0x0B) {
        /* 82-byte layout, or a corrected 80/76 one - both are whole GICCs */
        return (len == GICC_MIN) ? GICC_82 : len;
    }
    return len < 2 ? 2 : len;
}

static const char *type_name(uint8_t type)
{
    switch (type) {
    case 0x0B: return "GICC";
    case 0x0C: return "GICD";
    case 0x0D: return "MSI ";
    case 0x0E: return "GICR";
    case 0x0F: return "ITS ";
    default:   return "??  ";
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "usage: %s info|fixlen|shrink|gicr BASE STEP|trim N|strip TYPE [file]\n", argv[0]);
        return 1;
    }
    const char *cmd = argv[1];
    const char *path = "APIC.aml";
    if (argc >= 3 && strcmp(cmd, "info") != 0 && strcmp(cmd, "fixlen") != 0 &&
        strcmp(cmd, "shrink") != 0) {
        /* last argument may be a file path */
        const char *last = argv[argc - 1];
        if (strstr(last, ".aml")) path = last;
    }

    size_t len;
    uint8_t *buf = load(path, &len);
    if (len < HDR_LEN) { fprintf(stderr, "table too short\n"); return 1; }

    if (strcmp(cmd, "info") == 0) {
        printf("%s: %zu bytes, checksum %s\n", path, len, checksum_ok(buf, len) ? "ok" : "BAD");
        int index = 0;
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            uint8_t type = buf[off], slen = buf[off + 1];
            printf("  off=0x%03zx type=0x%02x %s len=%u", off, type, type_name(type), slen);
            if (type == 0x0B) {
                printf(" cpu#%d mpidr=0x%03llx eff=%u gicr=0x%llx spe=0x%04x",
                       index++,
                       (unsigned long long)get64(buf + off + GICC_OFF_MPIDR),
                       buf[off + GICC_OFF_EFF],
                       (unsigned long long)get64(buf + off + GICC_OFF_GICR),
                       (unsigned)get32(buf + off + 0x4E) & 0xFFFF);
            }
            printf("\n");
        }
        return 0;
    }

    if (strcmp(cmd, "fixlen") == 0) {
        int n = 0;
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            if (buf[off] == 0x0B && buf[off + 1] != stride(buf, off)) {
                buf[off + 1] = (uint8_t)stride(buf, off);
                n++;
            }
        }
        fix_tail(buf, len);
        save(path, buf, len);
        printf("%s: %d GICC length field(s) corrected, checksum %s\n",
               path, n, checksum_ok(buf, len) ? "ok" : "BAD");
        return 0;
    }

    if (strcmp(cmd, "shrink") == 0) {
        uint8_t *out = malloc(len + 256);
        size_t olen = HDR_LEN, n = 0;
        memcpy(out, buf, HDR_LEN);
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            size_t st = stride(buf, off);
            if (buf[off] == 0x0B && st > GICC_80) {
                memcpy(out + olen, buf + off, GICC_80);
                out[olen + 1] = GICC_80;
                olen += GICC_80;
                n++;
            } else {
                memcpy(out + olen, buf + off, st);
                olen += st;
            }
        }
        fix_tail(out, olen);
        save(path, out, olen);
        printf("%s: %zu bytes, %zu GICC shrunk to %d, checksum %s\n",
               path, olen, n, GICC_80, checksum_ok(out, olen) ? "ok" : "BAD");
        return 0;
    }

    if (strcmp(cmd, "gicr") == 0) {
        if (argc < 4) { fprintf(stderr, "gicr needs BASE and STEP\n"); return 1; }
        uint64_t base = strtoull(argv[2], NULL, 0);
        uint64_t step = strtoull(argv[3], NULL, 0);
        int index = 0;
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            if (buf[off] == 0x0B) {
                put64(buf + off + GICC_OFF_GICR, base + (uint64_t)index * step);
                index++;
            }
        }
        fix_tail(buf, len);
        save(path, buf, len);
        printf("%s: %d GICC given base 0x%llx step 0x%llx, checksum %s\n",
               path, index, (unsigned long long)base, (unsigned long long)step,
               checksum_ok(buf, len) ? "ok" : "BAD");
        return 0;
    }

    if (strcmp(cmd, "trim") == 0) {
        if (argc < 3) { fprintf(stderr, "trim needs N\n"); return 1; }
        size_t keep = (size_t)atoi(argv[2]);
        uint8_t *out = malloc(len + 256);
        size_t olen = HDR_LEN, seen = 0;
        memcpy(out, buf, HDR_LEN);
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            size_t st = stride(buf, off);
            if (buf[off] == 0x0B && ++seen > keep) continue;
            memcpy(out + olen, buf + off, st);
            olen += st;
        }
        fix_tail(out, olen);
        save(path, out, olen);
        printf("%s: %zu bytes, %zu/%zu GICC kept, checksum %s\n",
               path, olen, keep, seen, checksum_ok(out, olen) ? "ok" : "BAD");
        return 0;
    }

    if (strcmp(cmd, "strip") == 0) {
        if (argc < 3) { fprintf(stderr, "strip needs a type, e.g. 0x0E\n"); return 1; }
        uint8_t want = (uint8_t)strtoul(argv[2], NULL, 0);
        uint8_t *out = malloc(len + 256);
        size_t olen = HDR_LEN, n = 0;
        memcpy(out, buf, HDR_LEN);
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            size_t st = stride(buf, off);
            if (buf[off] == want) { n++; continue; }
            memcpy(out + olen, buf + off, st);
            olen += st;
        }
        fix_tail(out, olen);
        save(path, out, olen);
        printf("%s: %zu bytes, %zu subtable(s) of type 0x%02x removed, checksum %s\n",
               path, olen, n, want, checksum_ok(out, olen) ? "ok" : "BAD");
        return 0;
    }

    if (strcmp(cmd, "its") == 0) {
        /*
         * Qualcomm tables describe the GICv3 ITS with an MSI Frame subtable
         * (type 0x0D) at the ITS register base.  ACPI has a dedicated type
         * for that: GIC ITS, 0x0F, 20 bytes:
         *   type, length, reserved, GIC ITS ID, Base Address(8), reserved(4)
         * Convert every MSI Frame entry into that form, using its base.
         */
        uint8_t *out = malloc(len + 256);
        size_t olen = HDR_LEN, n = 0;
        memcpy(out, buf, HDR_LEN);
        for (size_t off = HDR_LEN; off + 2 <= len; off += stride(buf, off)) {
            size_t st = stride(buf, off);
            if (buf[off] == 0x0D && st >= 16) {
                uint64_t base = get64(buf + off + 8);
                memset(out + olen, 0, 20);
                out[olen + 0] = 0x0F;
                out[olen + 1] = 20;
                out[olen + 4] = 0;              /* GIC ITS ID */
                put64(out + olen + 8, base);
                olen += 20;
                n++;
            } else {
                memcpy(out + olen, buf + off, st);
                olen += st;
            }
        }
        fix_tail(out, olen);
        save(path, out, olen);
        printf("%s: %zu bytes, %zu MSI frame(s) converted to GIC ITS, checksum %s\n",
               path, olen, n, checksum_ok(out, olen) ? "ok" : "BAD");
        return 0;
    }

    fprintf(stderr, "unknown command: %s\n", cmd);
    return 1;
}
