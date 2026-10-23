/* Run the actual C keyboard/controller producer with array-bound checks. */
#define _MEMORYMANAGER_H_
#include "../src/controllers.c"

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s\n", m); exit(1); } } while (0)

int main(void)
{
    b1InitControllers();
    for (unsigned c = 0; c < NOT_ASSOCIATED; ++c) {
        b1ResetControllers();
        CHECK(b1SetController(c), "Valid controller was rejected");
        for (unsigned i = 0; i < NO_CONTROLLER_BITS; ++i)
            CHECK(b1ControllerBits[i] == (i == c / 8 ? 1U << (c & 7) : 0),
                  "Controller setter changed an adjacent bit");
    }
    CHECK(!b1SetController(NOT_ASSOCIATED), "Sentinel controller was activated");
    b1AssociateController(AGI_A_ASCII, AGI_A_SCAN, 254);
    b1ResetControllers();
    CHECK(b1SetControllerByPetscii(KEY_CAP_A) && b1ControllerBits[31] == 0x40,
          "Keyboard association did not set its controller");
    for (unsigned keycode = 0; keycode < 256; ++keycode) {
        b1ResetControllers();
        CHECK(b1SetControllerByPetscii(keycode) == (keycode == KEY_CAP_A),
              "Unassociated/out-of-range PETSCII activated a controller");
    }
    puts("Controller checks pass: all bit positions, sentinel, association and all 256 PETSCII values");
    return 0;
}
