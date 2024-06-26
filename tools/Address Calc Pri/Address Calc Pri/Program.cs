using System;

public class PixelAddressConverter
{
    private const int BaseAddress = 0x9600;
    private const int RowBytes = 80;

    public static int GetAddress(int x, int y)
    {
        if (x < 0 || x >= 160 || y < 0 || y >= 168)
        {
            throw new ArgumentOutOfRangeException("Coordinates are out of bounds.");
        }

        int rowOffset = y * RowBytes;
        int byteOffset = x / 2;

        return BaseAddress + rowOffset + byteOffset;
    }

    public static void Main()
    {
        try
        {
            Console.WriteLine("Enter x coordinate (0-159): ");
            int x = int.Parse(Console.ReadLine());

            Console.WriteLine("Enter y coordinate (0-167): ");
            int y = int.Parse(Console.ReadLine());

            int address = GetAddress(x, y);
            Console.WriteLine($"Address for pixel ({x}, {y}) is: 0x{address:X}");
        }
        catch (FormatException)
        {
            Console.WriteLine("Invalid input. Please enter valid integers.");
        }
        catch (ArgumentOutOfRangeException ex)
        {
            Console.WriteLine(ex.Message);
        }
    }
}
