using System;

public class FixedPointConverter
{
    public static int doubleToFixedPoint(double number)
    {
        // Ensure the number is within the acceptable range (0 to 255.99999)
        if (number < 0.0f || number > 255.99999f)
            throw new ArgumentOutOfRangeException(nameof(number), "The number is out of the range supported by the fixed-point format.");

        // Extract whole and fractional parts
        byte wholePart = (byte)Math.Truncate(number);
        double fractionalPart = number - wholePart;

        // Convert fractional part to fixed-point mantissa
        ushort mantissa = (ushort)(fractionalPart * 65536);  // 2^16 = 65536

        // Represent the fixed-point number in an int
        int result = (wholePart << 24) | mantissa;

        return result;
    }

    public static void Main()
    {
        for (double number = 0.333; number <= 0.502; number = number + 0.001)
        {
            int fixedPoint = doubleToFixedPoint(number);
            Console.WriteLine($"Fixed Point Representation: {fixedPoint:X} of {number}");
        }
        Console.Read();
    }
}
