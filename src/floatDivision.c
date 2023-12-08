#include "floatDivision.h"

#pragma code-name (push, "BANKRAM06")
void b6InitFloatDivision()
{
	byte** zpDivisionArea = (byte**)ZP_DIV_AREA;
	byte** zpDivBankMetadata = (byte**)ZP_DIV_BANK;
	byte** zpDivAddressMetadata = (byte**)ZP_DIV_ADDR;

	*zpDivisionArea = &DIVISION_AREA[0];
	*zpDivBankMetadata = divBankMetadata;
	*zpDivAddressMetadata = divAddressMetadata;
}
#pragma code-name (pop)
