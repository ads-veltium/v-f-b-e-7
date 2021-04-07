#include "../control.h"

static String Cipher(String input, String oldAlphabet, String newAlphabet)
{
	String output = "";
	int inputLen = input.length();

	for (int i = 0; i < inputLen; ++i)
	{
		int oldCharIndex = oldAlphabet.indexOf(input[i]);

		if (oldCharIndex >= 0)
			output += newAlphabet[oldCharIndex];
		else
			output += input[i];
	}

	return output;
}

String Encipher(String input)
{
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
	return Cipher(input, plainAlphabet, cipherAlphabet);
}

String Decipher(String input)
{
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
	return Cipher(input, cipherAlphabet, plainAlphabet);
}

