// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/20.
//

#include "io_buffer.h"
#include <algorithm> //std::swap before c++11
#include <utility>   //std::swap in c++11
#include <memory>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include "common/macro.h"
#include "common/common_defines.h"

namespace antflash {

struct Block {
    std::shared_ptr<Block> next;
    uint32_t size;
    uint32_t capacity;
    char data[0];

    Block(uint32_t hint_size) :
            size(0), capacity(hint_size - offsetof(Block, data)) {
    }
    ~Block() = default;

    inline uint32_t freeSpace() const {
        return capacity - size;
    }
};

using TLSBlockChain = std::shared_ptr<Block>;

//When pushing data to block, a thread-local block is always used, which it's thread-compatible,
//and io buffer just hold a block ref of this block and index range it use.
//When popping data from block, io buffer's block may refer to block belonged to other thread,
//so only block ref's range is changed or removed but block stay unchanged.
//When block is full, it will be removed and waiting for reclaimed when shared counter
//decrease to 0.
thread_local TLSBlockChain s_tls_block_chain;

static std::shared_ptr<Block> createBlock(
        uint32_t size = BUFFER_DEFAULT_BLOCK_SIZE) {
    void *mem = std::malloc(size);
    if (LIKELY(nullptr != mem)) {
        return std::shared_ptr<Block>(new(mem) Block(size), [](Block* p) {
                        p->~Block();
                        std::free(p);
                        });
    }
    return std::shared_ptr<Block>();
}

static std::shared_ptr<Block> shareBlockFromTLSChain() {
    //Pop head until one block has free space, if this block ref count is 1,
    //it will be reclaimed.
    while (s_tls_block_chain) {
        if (s_tls_block_chain->freeSpace() > 0) {
            return s_tls_block_chain;
        }
        s_tls_block_chain = s_tls_block_chain->next;
    }

    //Original hazard block's counter is decrease, and if decreased to 0,
    //memory will be free, and if not, memory will be free when all io
    //buffer which holds shared its pointer deconstructed.
    s_tls_block_chain = createBlock();

    //After return, both s_tls_block_chain and io buffer which calling
    //shareBlockFromTLSChain holds the shared pointer, the shared counter
    //is 2 but not 1, and deconstruction among threads is safe.
    return s_tls_block_chain;
}

static void releaseBlockToTLSChain(std::shared_ptr<Block>& block) {
    if (block->freeSpace() == 0) {
        return;
    }
    block->next = s_tls_block_chain;
    s_tls_block_chain = block;
}

static std::shared_ptr<Block> acquireBlockFromTLSChain() {
    //Pop head until one block has free space, if this block ref count is 1,
    //it will be reclaimed.
    while (s_tls_block_chain) {
        if (s_tls_block_chain->freeSpace() > 0) {
            auto block = s_tls_block_chain;
            s_tls_block_chain = block->next;
            block->next.reset();
            return block;
        }
        s_tls_block_chain = s_tls_block_chain->next;
    }

    //After return, only io buffer which calling acquireBlockFromTLSChain
    // holds the shared pointer, the shared counter is 1.
    return createBlock();
}

static constexpr size_t IO_BUFFER_DEFAULT_SLICE_REF_SIZE = 4;
IOBuffer::IOBuffer() : _ref_offset(0) {
    _ref.reserve(IO_BUFFER_DEFAULT_SLICE_REF_SIZE);
}

IOBuffer::~IOBuffer() {

}

IOBuffer::IOBuffer(const IOBuffer &right) {
    _ref = right._ref;
    _ref_offset = right._ref_offset;
}

IOBuffer::IOBuffer(IOBuffer &&right) {
    _ref = std::move(right._ref);
    _ref_offset = right._ref_offset;
    right.clear();
}

IOBuffer &IOBuffer::operator=(const IOBuffer &right) {
    if (this != &right) {
        _ref = right._ref;
        _ref_offset = right._ref_offset;
    }
    return *this;
}

IOBuffer &IOBuffer::operator=(IOBuffer &&right) {
    if (this != &right) {
        _ref = std::move(right._ref);
        right.clear();
    }
    return *this;
}

void IOBuffer::swap(IOBuffer &right) {
    if (this != &right) {
        _ref.swap(right._ref);
        std::swap(_ref_offset, right._ref_offset);
    }
}

std::string IOBuffer::to_string() const {
    std::string s;
    for (size_t i = _ref_offset; i < _ref.size(); ++i) {
        s.append(_ref[i].block->data + _ref[i].offset,
                 _ref[i].length);
    }
    return s;
}

size_t IOBuffer::pop_front(size_t n) {
    size_t cur_size = 0;
    size_t ref_idx = _ref_offset;
    while (cur_size < n) {
        if (ref_idx >= _ref.size()) {
            break;
        }

        size_t pop_size = n - cur_size;
        if (pop_size < _ref[ref_idx].length) {
            _ref[ref_idx].offset += pop_size;
            _ref[ref_idx].length -= pop_size;
            cur_size += pop_size;
        } else {
            cur_size += _ref[ref_idx].length;
            ++ref_idx;
        }
    }

    _ref_offset = ref_idx;
    tryReleaseBlock();

    return cur_size;
}

size_t IOBuffer::pop_back(size_t n) {
    size_t cur_size = 0;
    ssize_t ref_idx = _ref.size() - 1;
    while (cur_size < n) {
        if (ref_idx < (ssize_t)_ref_offset) {
            break;
        }

        size_t pop_size = n - cur_size;
        if (pop_size < _ref[ref_idx].length) {
            _ref[ref_idx].length -= pop_size;
            cur_size += pop_size;
        } else {
            cur_size += _ref[ref_idx].length;
            _ref.pop_back();
            --ref_idx;
        }
    }

    return cur_size;
}

void IOBuffer::append(const IOBuffer &other) {
    for (auto& ref : other._ref) {
        _ref.emplace_back(ref);
    }
}

void IOBuffer::append(IOBuffer &&other) {
    for (auto& ref : other._ref) {
        _ref.emplace_back(ref);
    }
    other.clear();
}

void IOBuffer::append(char c) {
    auto block = shareBlockFromTLSChain();
    if (UNLIKELY(!block)) {
        return;
    }

    block->data[block->size] = c;
    do {
        if (_ref.size() > _ref_offset) {
            auto &back = _ref.back();
            if (back.block == block
                && back.offset + back.length == block->size) {
                ++back.length;
                break;
            }
        }

        _ref.emplace_back(block, block->size, 1);
    } while (0);

    ++block->size;
}

void IOBuffer::append(const void *data, size_t count) {
    if (UNLIKELY(nullptr == data || 0 == count)) {
        return;
    }

    size_t cur_size = 0;
    while (cur_size < count) {
        auto block = shareBlockFromTLSChain();
        if (UNLIKELY(!block)) {
            return;
        }
        size_t push_size = std::min(
                count - cur_size, (size_t)block->freeSpace());

        memmove(block->data + block->size, (char*)data + cur_size, push_size);

        do {
            if (_ref.size() > _ref_offset) {
                auto &back = _ref.back();
                if (back.block == block
                    && back.offset + back.length == block->size) {
                    back.length += push_size;
                    break;
                }
            }

            _ref.emplace_back(block, block->size, push_size);
        } while (0);

        block->size += push_size;
        cur_size += push_size;
    }
}

void IOBuffer::append(const char *data) {
    append(data, strlen(data));
}

ssize_t IOBuffer::append_from_file_descriptor(int fd, size_t max_size) {
    constexpr int MAX_APPEND_IOVEC = 64;
    iovec vec[MAX_APPEND_IOVEC];
    int nvec = 0;
    size_t space = 0;
    std::shared_ptr<Block> block_head;
    std::shared_ptr<Block> block_tail;
    do {
        auto block = acquireBlockFromTLSChain();
        if (UNLIKELY(!block)) {
            return -1;
        }
        if (block_tail) {
            block_tail->next = block;
        } else {
            block_head = block;
        }

        vec[nvec].iov_base = block->data + block->size;
        vec[nvec].iov_len = std::min((size_t)block->freeSpace(), max_size - space);
        space += vec[nvec].iov_len;
        ++nvec;
        if (space >= max_size || nvec >= MAX_APPEND_IOVEC) {
            break;
        }
        block_tail = block;
    } while (true);

    ssize_t nr = readv(fd, vec, nvec);
    if (nr > 0) {
        size_t total_len = nr;
        do {
            const size_t len = std::min(total_len, (size_t) block_head->freeSpace());
            total_len -= len;
            _ref.emplace_back(block_head, block_head->size, len);
            block_head->size += len;
            if (block_head->freeSpace() == 0) {
                block_head = block_head->next;
            }
        } while (total_len > 0);
    }

    while (block_head) {
        releaseBlockToTLSChain(block_head);
        block_head = block_head->next;
    }

    return nr;
}

size_t IOBuffer::cut(IOBuffer *out, size_t n) {
    size_t cur_size = 0;
    size_t idx = _ref_offset;
    while (cur_size < n) {
        if (idx >= _ref.size()) {
            break;
        }

        size_t cut_size = n - cur_size;
        if (cut_size < _ref[idx].length) {
            out->_ref.emplace_back(_ref[idx].block,
                                   _ref[idx].offset,
                                   cut_size);
            _ref[idx].offset += cut_size;
            _ref[idx].length -= cut_size;
            cur_size += cut_size;
        } else {
            out->_ref.emplace_back(_ref[idx]);
            cur_size += _ref[idx].length;
            ++_ref_offset;
            ++idx;
        }
    }

    tryReleaseBlock();

    return cur_size;
}

size_t IOBuffer::cut(std::string &out, size_t n) {
    if (UNLIKELY(n == 0)) {
        return 0;
    }

    n = std::min(n, length());
    auto old_size = out.size();
    out.resize(old_size + n);
    return cut(&out[old_size], n);
}

size_t IOBuffer::cut(void *out, size_t n) {
    size_t cur_size = 0;
    size_t idx = _ref_offset;
    while (cur_size < n) {
        if (idx >= _ref.size()) {
            break;
        }

        size_t cut_size = n - cur_size;
        if (cut_size < _ref[idx].length) {
            memmove((char*)out + cur_size,
                    _ref[idx].block->data + _ref[idx].offset,
                    cut_size);
            _ref[idx].offset += cut_size;
            _ref[idx].length -= cut_size;
            cur_size += cut_size;
        } else {
            memmove((char*)out + cur_size,
                    _ref[idx].block->data + _ref[idx].offset,
                    _ref[idx].length);
            cur_size += _ref[idx].length;
            ++_ref_offset;
            ++idx;
        }
    }

    tryReleaseBlock();

    return cur_size;
}

ssize_t IOBuffer::cut_into_file_descriptor(int fd, size_t size_hint) {
    constexpr size_t IOBUF_IOV_MAX = 256;
    const size_t nref = std::min(_ref.size() - _ref_offset, IOBUF_IOV_MAX);
    struct iovec vec[nref];
    size_t nvec = 0;
    size_t cur_len = 0;

    do {
        vec[nvec].iov_base = _ref[_ref_offset + nvec].block->data
                             + _ref[_ref_offset + nvec].offset;
        vec[nvec].iov_len = _ref[_ref_offset + nvec].length;
        cur_len += vec[nvec].iov_len;
        ++nvec;
    } while (nvec < nref && cur_len < size_hint);

    ssize_t nw = writev(fd, vec, nvec);

    if (nw > 0) {
        pop_front(nw);
    }

    return nw;
}

size_t IOBuffer::copy_to(void *out, size_t n) const {
    size_t cur_size = 0;
    size_t idx = _ref_offset;
    while (cur_size < n) {
        if (idx >= _ref.size()) {
            break;
        }

        size_t cut_size = n - cur_size;
        if (cut_size < _ref[idx].length) {
            memmove((char*)out + cur_size,
                    _ref[idx].block->data + _ref[idx].offset,
                    cut_size);
            cur_size += cut_size;
        } else {
            memmove((char*)out + cur_size,
                    _ref[idx].block->data + _ref[idx].offset,
                    _ref[idx].length);
            cur_size += _ref[idx].length;
            ++idx;
        }
    }

    return cur_size;
}

std::pair<const char *, size_t> IOBuffer::slice(size_t i) const {
    return std::make_pair<const char*, size_t>(
            _ref[_ref_offset + i].block->data
            + _ref[_ref_offset + i].offset,
            (size_t)_ref[_ref_offset + i].length);
}

std::pair<const char*, size_t> IOBuffer::block(size_t i) const {
    return std::make_pair<const char*, size_t>(
            _ref[_ref_offset + i].block->data,
            (size_t)_ref[_ref_offset + i].block->size);
}

void IOBuffer::tryReleaseBlock() {
    if (_ref_offset > IO_BUFFER_DEFAULT_SLICE_REF_SIZE) {
        size_t size = _ref.size() - _ref_offset;
        if (size == 0) {
            _ref.clear();
            _ref_offset = 0;
            return;
        }

        auto itr = _ref.begin();
        for (size_t i = _ref_offset; i < _ref.size(); ++i, ++itr) {
            *itr = _ref[i];
        }

        _ref.erase(itr, _ref.end());
        _ref_offset = 0;
    }
}

std::string IOBuffer::dump() const {
    constexpr size_t ONE_LINE_CHAR_SIZE = 8;
    std::stringstream ss;

    size_t round = 0;
    char tmp[8];
    char tmp2[ONE_LINE_CHAR_SIZE + 1];
    tmp2[ONE_LINE_CHAR_SIZE] = 0;

    for (auto& c : to_string()) {
        if (round == ONE_LINE_CHAR_SIZE - 1) {
            if (c > 32 && c < 127) {
                tmp2[round] = c;
            } else {
                tmp2[round] = '.';
            }
            sprintf(tmp, " %02x ", (unsigned char)c);
            ss << tmp;
            for (size_t i = 0; i < ONE_LINE_CHAR_SIZE; ++i) {
                ss << tmp2[i];
            }
            ss << "\n";
            round = 0;
            tmp2[0] = 0;
        } else {
            sprintf(tmp, " %02x ", (unsigned char)c);
            ss << tmp;
            if (c > 32 && c < 127) {
                tmp2[round] = c;
            } else {
                tmp2[round] = '.';
            }
            ++round;
        }
    }

    if (round > 0) {
        for (size_t i = round; i < ONE_LINE_CHAR_SIZE; ++i) {
            ss << "    ";
        }
    }

    for (size_t i = 0; i < strlen(tmp2); ++i) {
        ss << tmp2[i];
    }

    return ss.str();
}

IOBufferZeroCopyInputStream::IOBufferZeroCopyInputStream(const IOBuffer& buf)
        : _buf(&buf), _byte_count(0), _add_offset(0), _ref_index(buf._ref_offset) {
}

IOBufferZeroCopyInputStream::~IOBufferZeroCopyInputStream() {

}

bool IOBufferZeroCopyInputStream::Next(const void **data, int *size) {
    if (_ref_index >= _buf->_ref.size()) {
        return false;
    }

    auto& slice = _buf->_ref[_ref_index];
    *data = slice.block->data + slice.offset + _add_offset;
    *size = slice.length - _add_offset;
    _byte_count += slice.length - _add_offset;
    _add_offset = 0;
    ++_ref_index;

    return true;
}

void IOBufferZeroCopyInputStream::BackUp(int count) {
    if (_ref_index > _buf->_ref_offset) {
        --_ref_index;
        auto& slice = _buf->_ref[_ref_index];
        _add_offset = slice.length - count;
        _byte_count -= count;
    }
}

bool IOBufferZeroCopyInputStream::Skip(int count) {
    if (_ref_index >= _buf->_ref.size()) {
        return false;
    }

    do {
        auto& slice = _buf->_ref[_ref_index];
        const int left_bytes = slice.length - _add_offset;
        if (count < left_bytes) {
            _add_offset += count;
            _byte_count += count;
            return true;
        }
        count -= left_bytes;
        _add_offset = 0;
        _byte_count += left_bytes;
        if (++_ref_index >= _buf->_ref.size()) {
            return false;
        }
    } while (1);

    return false;
}

google::protobuf::int64 IOBufferZeroCopyInputStream::ByteCount() const {
    return _byte_count;
}


IOBufferZeroCopyOutputStream::IOBufferZeroCopyOutputStream(IOBuffer* buf) :
        _buf(buf), _byte_count(0) {

}

bool IOBufferZeroCopyOutputStream::Next(void **data, int *size) {
    auto block = shareBlockFromTLSChain();
    *data = block->data + block->size;
    *size = block->freeSpace();
    _byte_count += *size;

    //assume all left size used
    _buf->_ref.emplace_back(block, block->size, block->freeSpace());
    block->size = block->capacity;

    return true;
}

void IOBufferZeroCopyOutputStream::BackUp(int count) {
    //return unused 'count' data to last slice of buffer
    while (!_buf->_ref.empty()) {
        if (count == 0) {
            return;
        }
        auto& slice = _buf->_ref.back();
        if (LIKELY((int)slice.length > count)) {
            slice.length -= count;
            _byte_count -= count;
            slice.block->size -= count;
            break;
        } else {
            _byte_count -= slice.length;
            slice.block->size -= slice.length;
            _buf->_ref.pop_back();
            count -= slice.length;
        }
    }
}

google::protobuf::int64 IOBufferZeroCopyOutputStream::ByteCount() const {
    return _byte_count;
}

bool IOBuffer::BlockUnitTest::exist() {
    return !block.expired();
}

IOBuffer::BlockUnitTest IOBuffer::weakBlock(size_t i) {
    BlockUnitTest block;
    block.block = _ref[_ref_offset + i].block;
    return block;
};
}
