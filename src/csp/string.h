/*
 * string.h
 *
 *  Created on: Apr 25, 2014
 *      Author: skim
 */

#ifndef STRING_H_
#define STRING_H_

#include <iterator>
#include <cstring>

// Extremely dumb string, no copy-on-write like GNU standard strings
// Not trying to be standards-compliant here

namespace csp{
class string : public std::vector<char>
{
public:
	enum : size_t { npos = std::string::npos };
	string()
	{
		this->reserve(8);
	}
	string(const char* a)
	{
		assign(a);
	}
	string(const char* a, size_t s)
	{
		assign(a, s);
	}
	string(const std::string& a)
	{
		assign(a);
	}
	string& operator +=(const string& rh)
	{
		return append(rh);
	}
	string& append(const string& rh)
	{
		for (auto& b : rh)
			this->push_back(b);
		return *this;
	}
	string& append(const std::string& rh)
	{
		for (auto& b : rh)
			this->push_back(b);
		return *this;
	}
	string& append(const char* a)
	{
		while (*a)
		{
			push_back(*a);
			a++;
		}
		return *this;
	}
	size_t length() const
	{
		return this->size();
	}
	size_t find(const std::string& str) const
	{
		return std_string().find(str);
	}
	size_t find(const string& str) const
	{
		return find(str.std_string());
	}
	size_t find(const char* str) const
	{
		std::string a (str);
		return find(a);
	}
	string substr(size_t pos = 0, size_t len = npos) const
	{
		string a;
		size_t siz = size();
		for (size_t i = pos; i < siz && i < len; i++)
			a.push_back(this->at(i));
		return a;
	}
	int compare(const std::string& str) const
	{
		return std_string().compare(str);
	}
	int compare(const string& str) const
	{
		return compare(str.std_string());
	}
	int compare(const char* str) const
	{
		std::string a (str);
		return compare(a);
	}
	void assign(const string& str)
	{
		*this = str;
	}
	void assign(const char* a)
	{
		while (*a)
		{
			push_back(*a);
			a++;
		}
	}
	void assign(const char* a, size_t l)
	{
		for (size_t i = 0; i < l; i++)
			push_back(a[i]);
	}
	void assign(const std::string& str)
	{
		for (auto a : str)
			push_back(a);
	}
	std::string std_string() const
	{
		std::string a (this->begin(), this->end());
		return a;
	}
};
std::istream& operator>> (std::istream& is, string& str)
{
	std::string a;
	is >> a;
	str.assign(a);
	return is;
}
std::ostream& operator<< (std::ostream& os, const string& str)
{
	os << str.std_string();
	return os;
}
string operator +(const string& lh, const string& rh)
{
	string a = lh;
	for (auto& b : rh)
		a.push_back(b);
	return a;
}
string operator +(const string& lh, const std::string& rh)
{
	string a = lh;
	a.append(rh);
	return a;
}
string operator +(const string& lh, const char* rh)
{
	string a = lh;
	a.append(rh);
	return a;
}
} /* namespace csp */

#endif /* STRING_H_ */
