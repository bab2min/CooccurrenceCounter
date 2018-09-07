#pragma once
#include "utils.h"
#include "ThreadPool.hpp"
#include <cstring>

namespace std
{
	template<>
	struct hash<pair<uint16_t, uint16_t>>
	{
		size_t operator()(const pair<uint16_t, uint16_t>& o) const
		{
			return hash<uint16_t>{}(o.first) | (hash<uint16_t>{}(o.second) << 16);
		}
	};
}

class u16light
{
public:
	class iterator
	{
		
	};
private:
	union
	{
		struct
		{
			size_t len;
			char16_t* data;
		};
		struct
		{
			uint16_t rawLen;
			std::array<char16_t, 7> rawData;
		};
	};
public:
	u16light() : rawLen(0), rawData({})
	{
	}

	u16light(const u16light& o)
	{
		if (o.rawLen <= 7)
		{
			rawLen = o.rawLen;
			rawData = o.rawData;
		}
		else
		{
			len = o.len;
			data = new char16_t[len];
			memcpy(data, o.data, len * sizeof(char16_t));
		}
	}

	u16light(u16light&& o) : rawLen(0), rawData({})
	{
		swap(o);
	}

	template<class Iter>
	u16light(Iter begin, Iter end)
	{
		size_t l = std::distance(begin, end);
		if (l <= 7)
		{
			rawLen = l;
			copy(begin, end, rawData.begin());
		}
		else
		{
			len = l;
			data = new char16_t[l];
			copy(begin, end, data);
		}
	}

	~u16light()
	{
		if (rawLen > 7) delete[] data;
	}

	u16light& operator=(const u16light& o)
	{
		if (rawLen > 7) delete[] data;

		if (o.rawLen <= 7)
		{
			rawLen = o.rawLen;
			rawData = o.rawData;
		}
		else
		{
			len = o.len;
			data = new char16_t[len];
			memcpy(data, o.data, len * sizeof(char16_t));
		}
		return *this;
	}

	u16light& operator=(u16light&& o)
	{
		swap(o);
		return *this;
	}

	bool operator<(const u16light& o) const
	{
		return std::lexicographical_compare(begin(), end(), o.begin(), o.end());
	}

	void swap(u16light& o)
	{
		std::swap(rawLen, o.rawLen);
		std::swap(rawData, o.rawData);
	}

	size_t size() const
	{
		return rawLen;
	}

	bool empty() const
	{
		return !rawLen;
	}

	const char16_t* begin() const
	{
		if (rawLen <= 7) return &rawData[0];
		else return data;
	}

	const char16_t* end() const
	{
		if (rawLen <= 7) return &rawData[0] + rawLen;
		else return data + len;
	}

	std::reverse_iterator<const char16_t*> rbegin() const
	{
		return std::make_reverse_iterator(end());
	}

	std::reverse_iterator<const char16_t*> rend() const
	{
		return std::make_reverse_iterator(begin());
	}

	char16_t& front()
	{
		if (rawLen <= 7) return rawData[0];
		else return data[0];
	}

	char16_t& back()
	{
		if (rawLen <= 7) return rawData[rawLen - 1];
		else return data[len - 1];
	}

	const char16_t& front() const
	{
		if (rawLen <= 7) return rawData[0];
		else return data[0];
	}

	const char16_t& back() const
	{
		if (rawLen <= 7) return rawData[rawLen - 1];
		else return data[len - 1];
	}

	bool startsWith(const u16light& o) const
	{
		if (o.size() > size()) return false;
		return std::equal(o.begin(), o.end(), begin());
	}
};

class KWordDetector
{
	struct Counter
	{
		WordDictionary<std::string, uint32_t> chrDict;
		std::vector<uint32_t> cntUnigram;
		std::unordered_set<std::pair<uint16_t, uint16_t>> candBigram;
		std::map<u16light, uint32_t> forwardCnt, backwardCnt;
	};
protected:
	size_t minCnt, maxWordLen;
	float minScore;
	size_t numThread;

	template<class LocalData, class FuncReader, class FuncProc>
	std::vector<LocalData> readProc(const FuncReader& reader, const FuncProc& processor, LocalData&& ld = {}) const
	{
		ThreadPool workers(numThread);
		std::vector<LocalData> ldByTid(workers.getNumWorkers(), ld);
		for (size_t id = 0; ; ++id)
		{
			auto ustr = reader(id);
			if (ustr.empty()) break;
			workers.enqueue([this, ustr, id, &ldByTid, &processor](size_t tid)
			{
				auto& ld = ldByTid[tid];
				processor(ustr, id, ld);
			});
		}
		return ldByTid;
	}
	void countUnigram(Counter&, const std::function<std::string(size_t)>& reader) const;
	void countBigram(Counter&, const std::function<std::string(size_t)>& reader) const;
	void countNgram(Counter&, const std::function<std::string(size_t)>& reader) const;
	float branchingEntropy(const std::map<u16light, uint32_t>& cnt, std::map<u16light, uint32_t>::iterator it) const;
public:

	struct WordInfo
	{
		std::vector<std::string> form;
		float score, lBranch, rBranch, lCohesion, rCohesion;
		uint32_t freq;

		WordInfo(const std::vector<std::string>& _form = {},
			float _score = 0, float _lBranch = 0, float _rBranch = 0, 
			float _lCohesion = 0, float _rCohesion = 0, uint32_t _freq = 0)
			: form(_form), score(_score), lBranch(_lBranch), rBranch(_rBranch), 
			lCohesion(_lCohesion), rCohesion(_rCohesion), freq(_freq)
		{}
	};

	KWordDetector(size_t _minCnt = 10, size_t _maxWordLen = 10, float _minScore = 0.1f,
		size_t _numThread = 0)
		: minCnt(_minCnt), maxWordLen(_maxWordLen), minScore(_minScore),
		numThread(_numThread ? _numThread : std::thread::hardware_concurrency())
	{}
	void setParameters(size_t _minCnt = 10, size_t _maxWordLen = 10, float _minScore = 0.1f)
	{
		minCnt = _minCnt;
		maxWordLen = _maxWordLen;
		minScore = _minScore;
	}
	std::vector<WordInfo> extractWords(const std::function<std::string(size_t)>& reader) const;
};

