// See https://aka.ms/new-console-template for more information
using ByteArrayGenerator;
using Newtonsoft.Json;
using System.Net.Http.Json;
using System.Text;

Console.WriteLine("Hello, World!");

BankInfos map = JsonConvert.DeserializeObject<BankInfos>(File.ReadAllText("Z:\\CLionProjects\\CX16Agi\\tools\\ByteArrayGenerator\\ByteArrayGenerator\\BankDoc.json"));
const int DefaultBank = 5;
const int NoCommands = 182;
const int SaidInstruction = 14;
int counter = 0;

StringBuilder debugStringBuilder = new StringBuilder($"${map.ZeroOpCodeBank},\n");
StringBuilder countStringBuilder = new StringBuilder($"${counter},\n");
StringBuilder stringBuilder = new StringBuilder($"${map.ZeroOpCodeBank},");
foreach (BankInfo bankInfo in map.Banks)
{
    for (int i = bankInfo.Low; i <= bankInfo.High; i++)
    {
        counter++;
        if (!map.NoOps.Contains(i))
        {
            if (counter != SaidInstruction)
            {
                stringBuilder.Append($"${bankInfo.Bank}");
                debugStringBuilder.AppendLine(counter + ":" + bankInfo.Bank.ToString());
            }
            else
            {
                stringBuilder.Append($"${map.SaidBank}");
                debugStringBuilder.AppendLine(counter + ":" + map.SaidBank);
            }
        }
        else
        {
            stringBuilder.Append($"${map.NoOpBank}");
            debugStringBuilder.AppendLine(counter + ":" + map.NoOpBank.ToString());
        }
        if (!(i == bankInfo.High && counter == NoCommands))
        {
            stringBuilder.Append(",");
        }
        countStringBuilder.Append($"{counter},");
    }
    debugStringBuilder.Append("*");
}
string result = stringBuilder.ToString();


int y = 4;

