using System;
using System.IO;

namespace DividingNumbers
{
    class Program
    {
        static void Main(string[] args)
        {
            string divisionTableFilePrefix = "0x";
            string divisionAddressTableFilePath = "divisionAddressTable.bin";
            string divisionBankTableFilePath = "divisionBankTable.bin";
            int currentBank = 0x33;
            int currentByteCount = 0;

            FileStream divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".bin", FileMode.Create);
            BinaryWriter divisionWriter = new BinaryWriter(divisionFileStream);
            FileStream divisionAddressFileStream = new FileStream(divisionAddressTableFilePath, FileMode.Create);
            BinaryWriter divisionAddressWriter = new BinaryWriter(divisionAddressFileStream);
            FileStream divisionBankFileStream = new FileStream(divisionBankTableFilePath, FileMode.Create);
            BinaryWriter divisionBankWriter = new BinaryWriter(divisionBankFileStream);

            int address = 0xA000;

            for (int i = 1; i <= 167; i++)
            {
                // Write the starting address for the current 'i' value
                divisionAddressWriter.Write((byte)(address & 0xFF));
                divisionAddressWriter.Write((byte)(address >> 8));

                // Write the bank where the current 'i' group will be stored
                divisionBankWriter.Write((byte)currentBank);

                for (int j = 2; j <= 167; j++)
                {
                    if (currentByteCount >= 8192)
                    {
                        divisionWriter.Close();
                        divisionFileStream.Close();
                        currentBank++;
                        divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".bin", FileMode.Create);
                        divisionWriter = new BinaryWriter(divisionFileStream);
                        currentByteCount = 0;
                    }

                    double result = (double)i / j;
                    int wholeNumber = (int)result;
                    int mantissa = (int)Math.Round((result - wholeNumber) * 1000); // Using 1000 for 3 decimal places

                    divisionWriter.Write((byte)(mantissa & 0xFF)); // Least significant byte of mantissa
                    divisionWriter.Write((byte)(mantissa >> 8));    // Most significant byte of mantissa
                    divisionWriter.Write((byte)wholeNumber);

                    currentByteCount += 3;
                }

                // Increment the address by the number of bytes written for the current 'i' value
                address += 166 * 3;
            }

            divisionWriter.Close();
            divisionFileStream.Close();
            divisionAddressWriter.Close();
            divisionAddressFileStream.Close();
            divisionBankWriter.Close();
            divisionBankFileStream.Close();

            Console.WriteLine("Results, division addresses, and division bank information have been written to files!");
        }
    }
}
