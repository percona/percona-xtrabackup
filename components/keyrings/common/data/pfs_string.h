
#ifndef PFS_STRING_INCLUDED
#define PFS_STRING_INCLUDED

#include <limits>
#include <optional>
#include <sstream>
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/psi_memory_key.h"

extern PSI_memory_key KEY_mem_keyring;

/**
  Malloc_allocator is based on sql/malloc_allocator.h, but uses a fixed PSI key
  instead
*/
template <class T = void *>
class Comp_malloc_allocator {
  // This cannot be const if we want to be able to swap.
  PSI_memory_key m_key = KEY_mem_keyring;

 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  typedef T *pointer;
  typedef const T *const_pointer;

  typedef T &reference;
  typedef const T &const_reference;

  pointer address(reference r) const { return &r; }
  const_pointer address(const_reference r) const { return &r; }

  explicit Comp_malloc_allocator() {}

  template <class U>
  Comp_malloc_allocator(const Comp_malloc_allocator<U> &other [[maybe_unused]])
      : m_key(other.psi_key()) {}

  template <class U>
  Comp_malloc_allocator &operator=(const Comp_malloc_allocator<U> &other
                                   [[maybe_unused]]) {
    assert(m_key == other.psi_key());  // Don't swap key.
  }

  pointer allocate(size_type n, const_pointer hint [[maybe_unused]] = nullptr) {
    if (n == 0) return nullptr;
    if (n > max_size()) throw std::bad_alloc();

    pointer p = static_cast<pointer>(
        my_malloc(m_key, n * sizeof(T), MYF(MY_WME | ME_FATALERROR)));
    if (p == nullptr) throw std::bad_alloc();
    return p;
  }

  void deallocate(pointer p, size_type) { my_free(p); }

  template <class U, class... Args>
  void construct(U *p, Args &&... args) {
    assert(p != nullptr);
    try {
      ::new ((void *)p) U(std::forward<Args>(args)...);
    } catch (...) {
      assert(false);  // Constructor should not throw an exception.
    }
  }

  void destroy(pointer p) {
    assert(p != nullptr);
    try {
      p->~T();
    } catch (...) {
      assert(false);  // Destructor should not throw an exception
    }
  }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }

  template <class U>
  struct rebind {
    typedef Comp_malloc_allocator<U> other;
  };

  PSI_memory_key psi_key() const { return m_key; }
};

template <class T>
bool operator==(const Comp_malloc_allocator<T> &a1,
                const Comp_malloc_allocator<T> &a2) {
  return a1.psi_key() == a2.psi_key();
}

template <class T>
bool operator!=(const Comp_malloc_allocator<T> &a1,
                const Comp_malloc_allocator<T> &a2) {
  return a1.psi_key() != a2.psi_key();
}

using pfs_string = std::basic_string<char, std::char_traits<char>,
                                     Comp_malloc_allocator<char>>;

using pfs_optional_string = std::optional<pfs_string>;

using pfs_secure_ostringstream =
    std::basic_ostringstream<char, std::char_traits<char>,
                             Comp_malloc_allocator<char>>;

#endif  // PFS_STRING_INCLUDED
