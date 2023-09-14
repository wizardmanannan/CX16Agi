using System;
using System.IO;

namespace DividingNumbers
{
    class Program
    {
        static void Main(string[] args)
        {
            string divisionTableFilePrefix = "DIV";
            string divisionAddressTableFilePath = "DIVA.BIN";
            string divisionBankTableFilePath = "DIVB.BIN";
            int currentBank = 0x31;
            int currentByteCount = 0;
            int[] iGroupsPerFile = { 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16 }; // 167 'i' groups across 11 files

            FileStream divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".BIN", FileMode.Create);
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
                if (iGroupCount == iGroupsPerFile[currentFileIndex])
                {
                    divisionWriter.Close();
                    divisionFileStream.Close();
                    currentBank++;
                    divisionFileStream = new FileStream(divisionTableFilePrefix + currentBank.ToString("X2") + ".BIN", FileMode.Create);
                    divisionWriter = new BinaryWriter(divisionFileStream);
                    currentByteCount = 0;
                    iGroupCount = 0;
                    currentFileIndex++;

                    address = 0xA000; // Reset the address for the new bank
                }

                divisionAddressWriter.Write((byte)(address & 0xFF));
                divisionAddressWriter.Write((byte)(address >> 8));
                divisionBankWriter.Write((byte)currentBank);

                for (int j = 2; j <= 167; j++)
                {
                    double result = (double)i / j;
                    int wholeNumber = (int)result;
                    int mantissa = (int)Math.Round((result - wholeNumber) * 10000);

                    divisionWriter.Write((byte)(mantissa & 0xFF));
                    divisionWriter.Write((byte)(mantissa >> 8));
                    divisionWriter.Write((byte)wholeNumber);

                    if (i == 167 && j == 2)
                    {
                        int _ = 5;
                    }
                }

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

            Console.WriteLine("Results, division addresses, and division bank information have been written to equally sized files!");
        }
    }
}
