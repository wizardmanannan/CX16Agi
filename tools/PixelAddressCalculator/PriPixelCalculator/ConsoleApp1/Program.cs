using System;

class Program
{
    static void Main()
    {
        while (true)
        {
            // Constants
            const int width = 160;
            const int height = 168;
            const int bytesPerPixel = 2;
            const int startAddress = 0x9600;

            // Input coordinates
            Console.Write("Enter x coordinate (0-159): ");
            int x = int.Parse(Console.ReadLine());

            Console.Write("Enter y coordinate (0-167): ");
            int y = int.Parse(Console.ReadLine());

            // Validate input
            if (x < 0 || x >= width || y < 0 || y >= height)
            {
                Console.WriteLine("Coordinates out of bounds!");
                return;
            }

            // Calculate byte offset
            int pixelIndex = (y * width) + x;
            int byteOffset = pixelIndex / bytesPerPixel;

            // Calculate memory address
            int memoryAddress = startAddress + byteOffset;

            // Output result
            Console.WriteLine($"Memory address for pixel ({x}, {y}) is: 0x{memoryAddress:X4}");
        }
    }
}
