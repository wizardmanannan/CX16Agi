
const int PICTURE_WIDTH  =  160;  /* Picture resolution */
const int PICTURE_HEIGHT =  168;
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int STARTING_ROW = ((SCREEN_HEIGHT / 2) - (PICTURE_HEIGHT / 2));
const int BYTES_PER_ROW = (SCREEN_WIDTH / 2);
const int STARTING_BYTE = (STARTING_ROW * BYTES_PER_ROW);

while (true)
{
    Console.WriteLine("Please enter an x coord\n");
    byte x = byte.Parse(Console.ReadLine());

    Console.WriteLine("Please enter an y coord\n");
    byte y = byte.Parse(Console.ReadLine());

    int result = (STARTING_BYTE + x) + (y * PICTURE_WIDTH);
    Console.WriteLine($"The address is {result:x} \n");

    int priResult = (SCREEN_WIDTH * SCREEN_HEIGHT) / 2 + (int) Math.Floor(((float)x + (y * PICTURE_WIDTH)) / 2);
    string evenOrOdd = ((float)x + (y * PICTURE_WIDTH)) % 2 == 0 ? "even" : "odd";
    Console.WriteLine($"The priority address is {priResult:x}. It is {evenOrOdd}\n");
}
