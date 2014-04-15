//============================================================================
// Name        : cspcpp.cpp
// Author      : skim
// Version     :
// Copyright   : Licensed under GPL
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <string>
#include <fstream>

#include "csp/csplib.h"

using namespace CSP;
using std::string;
using std::vector;

struct wordpair
{
    string word, whatmadeit;
};

// First key is length
vector<vector<string> > dict;

void fill_vec_till(vector<vector<string> >& fillerup, int size)
{
	int vsiz = fillerup.size();
	for (int i = vsiz; i < size; i++)
	{
		vector<string> a;
		fillerup.push_back(a);
	}
}

// Create a dictionary vector that is accessed by word length
CSP_DECL(elementize, string, vector<string>) ()
{
	vector<vector<string> > result;
	string word;
	while (read(word))
	{
		if (word.size() == 0)
			continue;
		if (word.size() > result.size())
			fill_vec_till(result, word.size());
		result[word.size()-1].push_back(word);
	}
	for (auto& a : result)
		put(a);
}

CSP_DECL(genlist, CSP::nothing, wordpair, int)(int length)
{
	if (length == 1)
		// Return all words length 1
		for (auto& a : dict[0])
			put({a, ""});
	else
	{
		genlist(length - 1) |
			// Read every word genlist sends
			pipe_read<wordpair,nothing>([this,length]
			                 (CSP::csp_pipe<wordpair,nothing>*, wordpair& shorter)
			{
				// Try to get a word that differs in one character
				// Turn i into si, into sin, sing, sting ,string
				for (auto& longer : dict[length-1])
				{
					int difference = 0;
					for (int i = 0; i < length && difference <= 1; i++)
					{
						if (shorter.word[i-difference] != longer[i])
							difference++;
					}
					if (difference <= 1)
						this->put({longer, shorter.word + " " + shorter.whatmadeit});
				}
			});
	}
}

int main(int argc, const char* argv[])
{
	string file = argc > 1? argv[1] : "/usr/share/dict/words";

	cat(file) | grab("'", true) | to_lower() | sort<string>(false) |
			uniq<string>() | elementize() >>= dict;

	genlist(9) | pipe_read<wordpair,nothing>([](csp_pipe<wordpair,nothing>*, wordpair& pair)
			{
				std::cout << pair.word + " " + pair.whatmadeit + "\n";
			});
	return 0;
}
