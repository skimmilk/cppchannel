#include <csp/csplib.h>
#include <csp/parallel.h>

using namespace csp;
using std::vector;

struct wordpair
{
    string word, whatmadeit;
};

// First key is length
vector<vector<string> > dict;

// Create a dictionary vector that is accessed by word length
// Arguments to CSP_DECL macro are:
//       name         input     output   arguments... (arguments... args)
CSP_DECL(elementize, string, vector<string>) ()
{
	vector<vector<string> > result;
	string word;
	while (read(word))
	{
		if (word.size() == 0)
			continue; // Ignore blanks
		if (word.size() > result.size())
			result.resize(word.size());
		result[word.size()-1].push_back(word);
	}
	for (auto& a : result)
		put(a);
}

// This finds 'word stacks' (I don't know what its really called)
// Outputs something like
// splittings splitting slitting sitting siting sting sing sin in i
CSP_DECL(genlist, nothing, wordpair, int)(int length)
{
	if (length == 1)
		// Return all words length 1
		for (auto& a : dict[0])
			put({a, ""});
	else
	{
		auto thisptr = this;
		// Recursion works!
		genlist(length - 1) |
			// Read every word genlist sends into variable 'shorter'
			parallel(3,
					chan_read<wordpair>([thisptr,length](wordpair& shorter)
			{
				// Try to get a word that differs in one character
				// If previous is 'i', next will be 'in', 'is',...
				for (const auto& longer : dict[length-1])
				{
					int difference = 0;
					for (int i = 0; difference <= 1 && i < length - 1 + difference; i++)
						if (shorter.word[i-difference] != longer[i])
							difference++;

					if (difference <= 1)
						thisptr->safe_put({longer,
							shorter.word + " " + shorter.whatmadeit});
				}
			})
			);
	}
}

int main(int argc, const char* argv[])
{
	const char* file = argc > 1? argv[1] : "/usr/share/dict/words";

	std::atomic<int> error (0);
	cat(file, &error) | grab("'", true) | to_lower() | sort<string>() |
			uniq<string>() | elementize() >>= dict;

	if (error)
	{
		std::cerr << "Could not open dictionary file " << file << "\n";
		return 1;
	}

	genlist(9) |
			chan_iter<wordpair,string>(
					[](wordpair& s)
			{
				return s.word + " " + s.whatmadeit;
			})
				| sort<string>() | print();
	return 0;
}
