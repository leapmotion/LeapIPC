#pragma once
#include <memory>
#include <autowiring/ContextMember.h>
#include <autowiring/ObjectPool.h>
#include <vector>

namespace leap {
namespace ipc {

namespace MessageBuffers {
  template<typename T>
  class RawBuffer {
  public:
    RawBuffer(size_t size = 0, T* data = nullptr) : m_data(data), m_allocatedSize(0), m_size(size), m_isOwner(false) {
      if (!m_data) {
        m_isOwner = true;
        m_data = new T[m_size];
        m_allocatedSize = m_size;
      }
    }
    ~RawBuffer() {
      if (m_isOwner) {
        delete [] m_data;
      }
    }

    RawBuffer(const RawBuffer&) = delete;
    RawBuffer& operator=(const RawBuffer&) = delete;

    RawBuffer(RawBuffer&& rhs) {
      swap(rhs);
    }

    RawBuffer& operator=(RawBuffer&& rhs) {
      swap(rhs);
      return *this;
    }

    bool Resize(size_t size, bool moveAsNeeded = true) {
      if (!m_isOwner) {
        return false;
      }
      if (size == m_size) {
        return true;
      }
      if (size < m_size || size <= m_allocatedSize) {
        m_size = size;
      } else {
        T* data = new T[size];
        if (!data) {
          return false;
        }
        if (moveAsNeeded) {
          for (size_t i = 0; i < m_size; i++) {
            data[i] = std::move(m_data[i]);
          }
        }
        m_allocatedSize = size;
        m_size = size;
        delete [] m_data;
        m_data = data;
      }
      return true;
    }

    size_t Size() const { return m_size; }
    T* Data() const { return m_data; }

    bool HasOwnership() const { return m_isOwner; }

  private:
    void swap(RawBuffer&& rhs) {
      T* t_data = m_data;
      size_t t_size = m_size;
      bool t_isOwner = m_isOwner;
      m_data = rhs.m_data;
      m_size = rhs.m_size;
      m_isOwner = rhs.m_isOwner;
      rhs.m_data = t_data;
      rhs.m_size = t_size;
      rhs.m_isOwner = t_isOwner;
    }

    T* m_data;
    size_t m_size;
    size_t m_allocatedSize;
    bool m_isOwner;
  };

  using Buffer = RawBuffer<uint8_t>;
  using SharedBuffer = std::shared_ptr<Buffer>;
  using Buffers = std::vector<SharedBuffer>;

  template<typename T>
  class RawBufferPool : public ContextMember {
  public:
    std::shared_ptr<RawBuffer<T>> Get(size_t size) {
#if _MSC_VER
      static const size_t s_maxAllocatedBufferSize = 4096; // 4KB or smaller on Windows
#else
      static const size_t s_maxAllocatedBufferSize = 127*1024; // 127KB or smaller on Mac
#endif
      if (size <= s_maxAllocatedBufferSize) {
        return std::make_shared<MessageBuffers::Buffer>(size);
      }
      auto sb = m_pool();
      if (sb && !sb->Resize(size, false)) {
        sb.reset();
      }
      return sb;
    }

  private:
    ObjectPool<RawBuffer<T>> m_pool;
  };

  using SharedBufferPool = RawBufferPool<uint8_t>;
};

}}
