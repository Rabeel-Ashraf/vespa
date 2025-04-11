// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "pagedict4_mem_rand_reader.h"

namespace search::diskindex::test {

PageDict4MemRandReader::PageDict4MemRandReader(uint32_t chunkSize, uint64_t numWordIds,
                                               ThreeLevelCountWriteBuffers &wb)
    : _decoders(chunkSize, numWordIds),
      _buffers(_decoders.ssd, _decoders.spd, _decoders.pd, wb),
      _ssr(_buffers._ss.get_read_context(),
           wb._ss.get_header_len(), wb._ss.get_file_bit_size(),
           wb._sp.get_header_len(), wb._sp.get_file_bit_size(),
           wb._p.get_header_len(), wb._p.get_file_bit_size()),
      _spData(reinterpret_cast<const char *>(_buffers._sp.get_read_context().getComprBuf())),
      _pData(reinterpret_cast<const char *>(_buffers._p.get_read_context().getComprBuf())),
      _pageSize(search::bitcompression::PageDict4PageParams::getPageByteSize())
{
    _ssr.setup(_decoders.ssd);
}

PageDict4MemRandReader::~PageDict4MemRandReader() = default;

bool
PageDict4MemRandReader::lookup(const std::string &key, uint64_t &wordNum,
                               PostingListCounts &counts, StartOffset &offsets)
{
    PageDict4SSLookupRes sslr;

    sslr = _ssr.lookup(key);
    if (!sslr._res) {
        counts.clear();
        offsets = sslr._l6StartOffset;
        wordNum = sslr._l6WordNum;
        return false;
    }

    if (sslr._overflow) {
        wordNum = sslr._l6WordNum;
        counts = sslr._counts;
        offsets = sslr._startOffset;
        return true;
    }
    PageDict4SPLookupRes splr;
    splr.lookup(_ssr,
                _spData +
                _pageSize * sslr._sparsePageNum,
                key,
                sslr._l6Word,
                sslr._lastWord,
                sslr._l6StartOffset,
                sslr._l6WordNum,
                sslr._pageNum);

    PageDict4PLookupRes plr;
    plr.lookup(_ssr,
               _pData + _pageSize * splr._pageNum,
               key,
               splr._l3Word,
               splr._lastWord,
               splr._l3StartOffset,
               splr._l3WordNum);
    wordNum = plr._wordNum;
    offsets = plr._startOffset;
    if (plr._res) {
        counts = plr._counts;
        return true;
    }
    counts.clear();
    return false;
}

}
