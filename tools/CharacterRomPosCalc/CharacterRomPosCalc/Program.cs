using System;

while (true)
{
    //int baseAddress = 0xD400;

    //Console.WriteLine("Please enter column \n");
    //int col = int.Parse(Console.ReadLine());


    //Console.WriteLine("Please enter row \n");
    //int row = int.Parse(Console.ReadLine());

    //Console.WriteLine("Address: {0:X}", ((row * 16 + col) * 8) + baseAddress);


    int baseAddress = 0x1F000;

    Console.WriteLine("Please enter column \n");
    int codeNum = int.Parse(Console.ReadLine());

    Console.WriteLine("Address: {0:X}", codeNum * 8 + baseAddress);
}