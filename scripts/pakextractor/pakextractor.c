#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* Standard big-endian unsigned 32-bit conversion */
static inline uint32_t read_u32_be(const uint8_t *buffer) {
    return ((uint32_t)buffer[0] << 24) |
    ((uint32_t)buffer[1] << 16) |
    ((uint32_t)buffer[2] << 8)  |
    (uint32_t)buffer[3];
}

typedef struct {
    uint32_t hash;
    uint32_t name_offset;
    uint32_t data_offset;
    uint32_t size;
} pak_entry_t;

static void create_parent_directories(char *path) {
    char *p = path;
    /* Skip leading slash or drive letter */
    if (*p == '/' || *p == '\\') p++;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            char ch = *p;
            *p = '\0';
            mkdir_p(path);
            *p = ch;
        }
        p++;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("[ X ] Usage: %s <file.pak> [output_dir]\n", argv[0]);
        return 1;
    }

    const char *pak_path = argv[1];
    const char *out_dir = (argc >= 3) ? argv[2] : "extracted";

    FILE *f = fopen(pak_path, "rb");
    if (!f) {
        printf("[ X ] Error: Failed to open PAK file: %s\n", pak_path);
        return 1;
    }

    /* Read Header (32 bytes) */
    uint8_t header[32];
    if (fread(header, 1, 32, f) != 32) {
        printf("[ X ] Error: Corrupted header or file too small.\n");
        fclose(f);
        return 1;
    }

    /* Check Magic 'MCPK' */
    if (memcmp(header, "MCPK", 4) != 0) {
        printf("[ X ] Error: Invalid header magic. Not an MCPK file.\n");
        fclose(f);
        return 1;
    }

    uint32_t version     = read_u32_be(header + 4);
    uint32_t entry_count = read_u32_be(header + 8);
    uint32_t table_off   = read_u32_be(header + 12);
    uint32_t names_off   = read_u32_be(header + 16);
    uint32_t names_bytes = read_u32_be(header + 20);

    if (version != 1) {
        printf("[ X ] Error: Unsupported version %u\n", version);
        fclose(f);
        return 1;
    }

    printf("[ + ] Valid MCPK archive detected.\n");
    printf("[ + ] Entries: %u | Table Offset: 0x%X | Names Size: %u bytes\n",
           entry_count, table_off, names_bytes);

    /* Read Entry Table */
    pak_entry_t *entries = malloc(entry_count * sizeof(pak_entry_t));
    if (!entries) {
        printf("[ X ] Error: Memory allocation failure for entry table.\n");
        fclose(f);
        return 1;
    }

    fseek(f, table_off, SEEK_SET);
    for (uint32_t i = 0; i < entry_count; i++) {
        uint8_t rec[16];
        if (fread(rec, 1, 16, f) != 16) {
            printf("[ X ] Error: Failed reading table record %u\n", i);
            free(entries);
            fclose(f);
            return 1;
        }
        entries[i].hash        = read_u32_be(rec);
        entries[i].name_offset = read_u32_be(rec + 4);
        entries[i].data_offset = read_u32_be(rec + 8);
        entries[i].size        = read_u32_be(rec + 12);
    }

    /* Read Names Buffer */
    char *names_buf = malloc(names_bytes);
    if (!names_buf) {
        printf("[ X ] Error: Memory allocation failure for names buffer.\n");
        free(entries);
        fclose(f);
        return 1;
    }

    fseek(f, names_off, SEEK_SET);
    if (fread(names_buf, 1, names_bytes, f) != names_bytes) {
        printf("[ X ] Error: Failed reading names block.\n");
        free(names_buf);
        free(entries);
        fclose(f);
        return 1;
    }

    /* Extract Files */
    mkdir_p(out_dir);

    char full_path[1024];
    uint8_t buffer[65536]; /* 64 KB Copy Buffer */

    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].name_offset >= names_bytes) {
            printf("[ X ] Skipping entry %u: invalid name offset\n", i);
            continue;
        }

        const char *rel_path = names_buf + entries[i].name_offset;
        snprintf(full_path, sizeof(full_path), "%s/%s", out_dir, rel_path);

        /* Standardize slashes for current OS */
        for (char *p = full_path; *p; p++) {
            #ifdef _WIN32
            if (*p == '/') *p = '\\';
            #else
            if (*p == '\\') *p = '/';
            #endif
        }

        create_parent_directories(full_path);

        FILE *out = fopen(full_path, "wb");
        if (!out) {
            printf("[ X ] Failed to create file: %s\n", full_path);
            continue;
        }

        fseek(f, entries[i].data_offset, SEEK_SET);
        uint32_t remaining = entries[i].size;

        while (remaining > 0) {
            uint32_t chunk = (remaining < sizeof(buffer)) ? remaining : (uint32_t)sizeof(buffer);
            size_t bytes_read = fread(buffer, 1, chunk, f);
            if (bytes_read == 0) break;
            fwrite(buffer, 1, bytes_read, out);
            remaining -= (uint32_t)bytes_read;
        }

        fclose(out);
        printf("[ + ] Extracted: %s (%u bytes)\n", rel_path, entries[i].size);
    }

    free(names_buf);
    free(entries);
    fclose(f);

    printf("[ + ] Extraction complete.\n");
    return 0;
}
