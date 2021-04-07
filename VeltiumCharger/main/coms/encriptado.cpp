#include "../control.h"

static bool Cipher(String input, String oldAlphabet, String newAlphabet, String &output)
{
	output = "";
	int inputLen = input.length();

	for (int i = 0; i < inputLen; ++i)
	{
		int oldCharIndex = oldAlphabet.indexOf(input[i]);

		if (oldCharIndex >= 0)
			output += newAlphabet[oldCharIndex];
		else
			output += input[i];
	}

	return true;
}

bool Encipher(String input, String &output)
{
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
    printf("%s %s\n",input.c_str(),  output.c_str());
	return Cipher(input, plainAlphabet, cipherAlphabet, output);
}

bool Decipher(String input, String &output)
{
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
	return Cipher(input, cipherAlphabet, plainAlphabet, output);
}

