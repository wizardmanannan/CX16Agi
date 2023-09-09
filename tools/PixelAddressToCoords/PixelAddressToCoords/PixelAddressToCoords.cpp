// AddressToCoordinates.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define STARTING_ROW ((BITMAP_HEIGHT / 2) - (PICTURE_HEIGHT / 2))
#define STARTING_BYTE (STARTING_ROW * BYTES_PER_ROW)
#define BITMAP_WIDTH 320
#define BITMAP_HEIGHT 240
#define PICTURE_WIDTH   160  /* Picture resolution */
#define PICTURE_HEIGHT  168
#define BYTES_PER_ROW (BITMAP_WIDTH / 2)

#include <iostream>
#include <stdlib.h>

using namespace std;

int main()
{
    int address, x, y;

    while (true)
    {
        printf("Enter the address (in hex)\n");
        cin >> hex >> address;

        address = address - STARTING_BYTE;
        x = address % BYTES_PER_ROW;
        y = address / BYTES_PER_ROW;

        printf("The coordinates are: (%d, %d)\n", x, y);
    }
}
