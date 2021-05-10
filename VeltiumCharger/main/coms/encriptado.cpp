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

char* Encipher(char* input)
{
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
	String out = Cipher(String(input), plainAlphabet, cipherAlphabet);
	return (char*)out.c_str();
}

void Decipher(char* input, char* output){
	String out = String(output);
	String plainAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
    String cipherAlphabet= "y5aPZAjw067QklpgMLW4qOf3SbXm9IErcvK1DhNCxsFJdunViR2eotTUYGH8zB";
	out = Cipher(String(input), cipherAlphabet, plainAlphabet);
	output = (char*)out.c_str();
	return;
}

