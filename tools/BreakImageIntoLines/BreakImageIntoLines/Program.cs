using System;
using System.IO;
using System.Diagnostics;

class Program
{
    static void Main()
    {
        // Assuming the data file is named "data.txt" and is located in the same directory as the executable
        // You may need to adjust the path depending on where your file is located
        string filePath = "C:\\meka\\CommanderX16Repo\\tools\\BreakImageIntoLines\\BreakImageIntoLines\\testFile.txt";

        try
        {
            // Reading the data from the file
            string data = File.ReadAllText(filePath);

            // Splitting the data by '0', removing empty entries
            string[] splitData = data.Split(new string[] { "00" }, StringSplitOptions.RemoveEmptyEntries);

            // Optionally, display each part of the split data
            foreach (string part in splitData)
            {
                Console.WriteLine(part);
            }

            // Call Debugger.Break() after the results are stored in the array
            // This will have an effect only when running under a debugger
            Debugger.Break();
        }
        catch (IOException e)
        {
            Console.WriteLine("An error occurred while reading the file:");
            Console.WriteLine(e.Message);
        }
    }
}
