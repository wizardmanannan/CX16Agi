#ifndef CX16AGI_TEST_GRAPHICS_CLEAR_H
#define CX16AGI_TEST_GRAPHICS_CLEAR_H

#include <stddef.h>
#include <stdint.h>

#define GRAPHICS_CLEAR_TEST_LOAD_ADDRESS 0x2000u
#define GRAPHICS_CLEAR_TEST_IMAGE_CAPACITY 0x6000u
#define GRAPHICS_CLEAR_TEST_VRAM_BYTES 0x20000u
#define GRAPHICS_CLEAR_TEST_PRIORITY_ADDRESS ((320u * 240u) / 2u)
#define GRAPHICS_CLEAR_TEST_PRIORITY_BYTES ((160u * 168u) / 2u)
#define GRAPHICS_CLEAR_TEST_SLICE_COUNT 7u

typedef struct {
    size_t image_bytes;
    uint16_t reset_entry;
    uint16_t slice_entry;
    uint16_t return_entry;
    uint16_t cursor_address;
    uint16_t remaining_address;
} GraphicsClearTestImage;

/* Export the linked test fixture produced from the production graphics assembly. */
int graphics_clear_export_test_image(const char *graphics_source, const char *image_path,
                                     GraphicsClearTestImage *image,
                                     char *error, size_t error_capacity);

/* Run the bounded priority-slice workload and return the complete VERA VRAM image. */
int graphics_clear_run_test_image(const char *image_path, const GraphicsClearTestImage *image,
                                  uint8_t *output_vram, size_t output_capacity,
                                  uint32_t *worst_slice_cycles,
                                  uint64_t *total_workload_cycles,
                                  char *error, size_t error_capacity);

#endif
