// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/20.
//

#ifndef RPC_IO_BUFFER_REFINE_H
#define RPC_IO_BUFFER_REFINE_H

#include <vector>
#include <memory>
#include <sys/uio.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include "common/common_defines.h"

namespace antflash {

struct Block;

//IO buffer, not thread safe
class IOBuffer {
friend class IOBufferZeroCopyOutputStream;
friend class IOBufferZeroCopyInputStream;
public:
    //IOBuffer with Slice, reference to Block. Slice memory allocation
    //is much smaller than Block, and several Slices shared one Block's
    // reference with different offset and length.
    struct Slice {
        std::shared_ptr<Block> block;
        uint32_t offset;
        uint32_t length;

        Slice() = default;
        ~Slice() = default;
        Slice(const Slice&) = default;
        Slice& operator=(const Slice&) = default;
        Slice(Slice&&) = default;
        Slice& operator=(Slice&&) = default;
        Slice(const std::shared_ptr<Block>& b, uint32_t o, uint32_t l) :
                block(b), offset(o), length(l) {}
    };

    IOBuffer();

    ~IOBuffer();

    IOBuffer(const IOBuffer &right);

    IOBuffer(IOBuffer &&right);

    IOBuffer &operator=(const IOBuffer &right);

    IOBuffer &operator=(IOBuffer &&right);

    void swap(IOBuffer &right);

    std::string to_string() const;

    std::string dump() const;

    size_t pop_front(size_t n);

    size_t pop_back(size_t n);

    void append(const IOBuffer &other);

    void append(IOBuffer &&other);

    void append(char c);

    inline void append(const std::string &data) {
        append(data.data(), data.size());
    }

    void append(const void *data, size_t count);

    void append(const char *data);

    ssize_t append_from_file_descriptor(int fd, size_t max_size);

    size_t cut(IOBuffer *out, size_t n);

    size_t cut(std::string &out, size_t n);

    size_t cut(void *out, size_t n);

    ssize_t cut_into_file_descriptor(int fd, size_t size_hint);

    size_t copy_to(void *buf, size_t n) const;

    void clear() {
        _ref.clear();
        _ref_offset = 0;
    }

    bool empty() const {
        return length() == 0;
    }

    size_t length() const {
        size_t total_size = 0;
        for (size_t i = _ref_offset; i < _ref.size(); ++i) {
            total_size += _ref[i].length;
        }
        return total_size;
    }

    size_t size() const { return length(); }
    inline size_t slice_num() const {
        return _ref.size() - _ref_offset;
    }
    std::pair<const char*, size_t> slice(size_t i) const;

    /****** For  unit test begin *******/
    std::pair<const char*, size_t> block(size_t i) const;
    struct BlockUnitTest {
        std::weak_ptr<Block> block;
        bool exist();
    };
    BlockUnitTest weakBlock(size_t i);
    /****** For  unit test end *******/
private:
    void tryReleaseBlock();

    std::vector<Slice> _ref;
    size_t _ref_offset;
};

class IOBufferZeroCopyInputStream
        : public google::protobuf::io::ZeroCopyInputStream {
public:
    IOBufferZeroCopyInputStream(const IOBuffer &);

    bool Next(const void **data, int *size) override;

    ~IOBufferZeroCopyInputStream() override;

    void BackUp(int count) override;

    bool Skip(int count) override;

    google::protobuf::int64 ByteCount() const override;

private:
    const IOBuffer *_buf;
    size_t _byte_count;
    size_t _add_offset;
    size_t _ref_index;
};

class IOBufferZeroCopyOutputStream
        : public google::protobuf::io::ZeroCopyOutputStream {
public:
    IOBufferZeroCopyOutputStream(IOBuffer *);

    ~IOBufferZeroCopyOutputStream() {}

    bool Next(void **data, int *size) override;

    void BackUp(int count) override;

    google::protobuf::int64 ByteCount() const override;

private:
    IOBuffer *_buf;
    size_t _byte_count;
};

}

#endif //RPC_IO_BUFFER_REFINE_H
