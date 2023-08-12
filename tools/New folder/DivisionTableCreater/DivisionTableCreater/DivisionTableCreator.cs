using System;
using System.IO;

namespace DividingNumbers
{
    class Program
    {
        static void Main(string[] args)
        {
            string divisionTableFilePrefix = "DIV"; // Shortened prefix
            string divisionAddressTableFilePath = "DIVA.BIN"; // Shortened name and extension
            string divisionBankTableFilePath = "DIVB.BIN"; // Shortened name and extension
            int currentBank = 0x31; // Start at bank 0x31
            int currentByteCount = 0;
            int[] iGroupsPerFile = { 15, 15, 15, 15, 15, 16, 15, 15, 15, 15, 16 }; // 167 'i' groups across 11 files

            FileStream divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".BIN", FileMode.Create); // Uppercase hexadecimal for bank value, capital file extension
            BinaryWriter divisionWriter = new BinaryWriter(divisionFileStream);
            FileStream divisionAddressFileStream = new FileStream(divisionAddressTableFilePath, FileMode.Create);
            BinaryWriter divisionAddressWriter = new BinaryWriter(divisionAddressFileStream);
            FileStream divisionBankFileStream = new FileStream(divisionBankTableFilePath, FileMode.Create);
            BinaryWriter divisionBankWriter = new BinaryWriter(divisionBankFileStream);

            int address = 0xA000;
            int iGroupCount = 0;
            int currentFileIndex = 0;

            for (int i = 1; i <= 167; i++)
            {
                // Check if we need to create a new file
                if (iGroupCount == iGroupsPerFile[currentFileIndex])
                {
                    divisionWriter.Close();
                    divisionFileStream.Close();
                    currentBank++;
                    divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".BIN", FileMode.Create); // Uppercase hexadecimal for bank value, capital file extension
                    divisionWriter = new BinaryWriter(divisionFileStream);
                    currentByteCount = 0;
                    iGroupCount = 0;
                    currentFileIndex++;

                    // Reset the address for the new bank
                    address = 0xA000;
                }

                // Write the starting address for the current 'i' value
                divisionAddressWriter.Write((byte)(address & 0xFF));
                divisionAddressWriter.Write((byte)(address >> 8));

                // Write the bank where the current 'i' group will be stored
                divisionBankWriter.Write((byte)currentBank);

                for (int j = 2; j <= 167; j++)
                {
                    double result = (double)i / j;
                    int wholeNumber = (int)result;
                    int mantissa = (int)Math.Round((result - wholeNumber) * 1000); // Using 1000 for 3 decimal places

                    divisionWriter.Write((byte)(mantissa & 0xFF)); // Least significant byte of mantissa
                    divisionWriter.Write((byte)(mantissa >> 8));    // Most significant byte of mantissa
                    divisionWriter.Write((byte)wholeNumber);
                }

                // Increment the address by the number of bytes written for the current 'i' value
                address += 166 * 3;
                currentByteCount += 498; // Total bytes for one 'i' group
                iGroupCount++;
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
