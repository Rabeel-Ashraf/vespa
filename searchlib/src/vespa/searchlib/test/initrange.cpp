// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include "initrange.h"
#include "docid_iterator.h"
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/searchlib/queryeval/emptysearch.h>
#include <vespa/searchlib/queryeval/truesearch.h>
#include <algorithm>

namespace search::test {

using namespace search::queryeval;
using std::make_unique;

InitRangeVerifier::InitRangeVerifier() :
    _trueTfmd(),
    _docIds()
{
    // (0),1 and 10,11 and 20,21 .... 200,201 etc are hits
    // 0 is of course invalid.
    for (size_t i(0); (i*10+1) < getDocIdLimit(); i++) {
        if (i > 0) {
            _docIds.push_back(i * 10);
        }
        _docIds.push_back(i*10 + 1);
    }
}

InitRangeVerifier::~InitRangeVerifier() = default;

InitRangeVerifier::DocIds
InitRangeVerifier::invert(const DocIds & docIds, uint32_t docIdlimit)
{
    DocIds inverted;
    inverted.reserve(docIdlimit);
    for (size_t i(1), next(0); i < docIdlimit; i++) {
        if (next < docIds.size()) {
            if (i >= docIds[next]) {
                if (i == docIds[next++]) {
                    continue;
                }
            }
        }
        inverted.push_back(i);
    }
    return inverted;
}

SearchIterator::UP
InitRangeVerifier::createIterator(const DocIds &docIds, bool strict)
{
    return make_unique<DocIdIterator>(docIds, strict);
}

SearchIterator::UP
InitRangeVerifier::createEmptyIterator()
{
    return make_unique<EmptySearch>();
}

SearchIterator::UP
InitRangeVerifier::createFullIterator() const
{
    return make_unique<TrueSearch>(_trueTfmd);
}

void
InitRangeVerifier::verify(SearchIterator * iterator) const
{
    SearchIterator::UP up(iterator);
    verify(*up);
}

void
InitRangeVerifier::verify(SearchIterator & iterator) const
{
    ASSERT_TRUE(iterator.is_strict() != vespalib::Trinary::Undefined);
    if (iterator.is_strict() == vespalib::Trinary::True) {
        verify(iterator, true);
    }
    verify(iterator, false);
}

void
InitRangeVerifier::verify(SearchIterator & iterator, bool strict) const
{
    verify(iterator, Ranges({{1, 202}}), strict);
    verify(iterator, Ranges({{1, 202}}), strict);
    for (uint32_t rangeWidth : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 202 }) {
        Ranges ranges;
        for (uint32_t sum(1); sum < getDocIdLimit(); sum += rangeWidth) {
            ranges.emplace_back(sum, std::min(sum+rangeWidth, getDocIdLimit()));
        }
        verify(iterator, ranges, strict);
        std::reverse(ranges.begin(), ranges.end());
        verify(iterator, ranges, strict);
    }
}

void
InitRangeVerifier::verify(SearchIterator & iterator, const Ranges & ranges, bool strict) const
{
    DocIds result = search(iterator, ranges, strict);
    ASSERT_EQ(_docIds.size(), result.size());
    for (size_t i(0); i < _docIds.size(); i++) {
        EXPECT_EQ(_docIds[i], result[i]);
    }
}

InitRangeVerifier::DocIds
InitRangeVerifier::search(SearchIterator & it, const Ranges & ranges, bool strict)
{
    DocIds result;
    for (Range range: ranges) {
        DocIds part = strict ? searchStrict(it, range) : searchRelaxed(it, range);
        result.insert(result.end(), part.begin(), part.end());
    }
    std::sort(result.begin(), result.end());
    return result;
}

InitRangeVerifier::DocIds
InitRangeVerifier::searchRelaxed(SearchIterator & it, Range range)
{
    DocIds result;
    it.initRange(range.first, range.second);
    for (uint32_t docid = range.first; docid < range.second; ++docid) {
        if (it.seek(docid)) {
            result.emplace_back(docid);
            it.unpack(docid);
        }
    }
    return result;
}

InitRangeVerifier::DocIds
InitRangeVerifier::searchStrict(SearchIterator & it, Range range)
{
    DocIds result;
    it.initRange(range.first, range.second);
    for (uint32_t docId = it.seekFirst(range.first); docId < range.second; docId = it.seekNext(docId + 1)) {
        result.push_back(docId);
        it.unpack(docId);
    }
    return result;
}

}
