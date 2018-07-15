#pragma once
#include "ThreadPool.hpp"

class WordDictionary
{
protected:
	std::map<std::string, int> word2id;
	std::vector<std::string> id2word;
	std::mutex mtx;
public:

	WordDictionary() {}
	WordDictionary(const WordDictionary& o) : word2id(o.word2id), id2word(o.id2word) {}
	WordDictionary(WordDictionary&& o)
	{
		std::swap(word2id, o.word2id);
		std::swap(id2word, o.id2word);
	}

	enum { npos = (size_t)-1 };
	int add(const std::string& str)
	{
		if (word2id.emplace(str, word2id.size()).second) id2word.emplace_back(str);
		return word2id.size() - 1;
	}

	int getOrAdd(const std::string& str)
	{
		std::lock_guard<std::mutex> lg(mtx);
		auto it = word2id.find(str);
		if (it != word2id.end()) return it->second;
		return add(str);
	}

	template<class Iter>
	std::vector<int> getOrAdds(Iter begin, Iter end)
	{
		std::lock_guard<std::mutex> lg(mtx);
		std::vector<int> ret;
		for (; begin != end; ++begin)
		{
			auto it = word2id.find(*begin);
			if (it != word2id.end()) ret.emplace_back(it->second);
			else ret.emplace_back(add(*begin));
		}
		return ret;
	}

	int get(const std::string& str) const
	{
		auto it = word2id.find(str);
		if (it != word2id.end()) return it->second;
		return npos;
	}

	std::string getStr(int id) const
	{
		return id2word[id];
	}

	size_t size() const { return id2word.size(); }

	void writeToFile(std::ostream& str) const
	{
		uint32_t vocab = id2word.size();
		str.write((const char*)&vocab, 4);
		for (auto w : id2word)
		{
			uint32_t len = w.size();
			str.write((const char*)&len, 4);
			str.write(&w[0], len);
		}
	}

	void readFromFile(std::istream& str)
	{
		uint32_t vocab;
		str.read((char*)&vocab, 4);
		id2word.resize(vocab);
		for (auto& w : id2word)
		{
			uint32_t len;
			str.read((char*)&len, 4);
			w.resize(len);
			str.read(&w[0], len);
		}

		for (size_t i = 0; i < id2word.size(); ++i)
		{
			word2id[id2word[i]] = i;
		}
	}
};


template<class LocalDataType>
std::vector<LocalDataType> scanText(std::istream& input, size_t worker, size_t maxLine, const std::function<void(LocalDataType&, std::string, size_t)>& func, const LocalDataType& ldInitVal = {})
{
	ThreadPool pool(worker);
	std::vector<LocalDataType> ld(worker, ldInitVal);
	std::string doc;
	int numLine = 0;
	std::vector<std::future<void>> futures(worker);
	while (getline(input, doc))
	{
		futures[numLine % futures.size()] = pool.enqueue([&ld, doc, &func](size_t tId, size_t nLine)
		{
			if (tId == 0) std::cerr << "Line " << nLine << std::endl;
			return func(ld[tId], doc, nLine);
		}, numLine + 1);
		numLine++;
		if (numLine >= maxLine) break;
	}
	for (auto && p : futures)
	{
		if (p.valid()) p.wait();
	}
	return ld;
}