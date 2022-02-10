
#ifndef GLOBAL_DEFAULT_MR_INCLUDED
#define GLOBAL_DEFAULT_MR_INCLUDED

// Temporal fix for boost workaround.hpp 1.73.0 warnings
// See https://github.com/boostorg/config/pull/383/files
#ifndef __clang_major__
#define __clang_major___WORKAROUND_GUARD 1
#else
#define __clang_major___WORKAROUND_GUARD 0
#endif

#include <boost/container/pmr/polymorphic_allocator.hpp>

#include <boost/container/string.hpp>
#include <boost/container_hash/hash.hpp>

#include "psi_memory_resource.hpp"

inline ::boost::container::pmr::memory_resource
    * ::boost::container::pmr::get_default_resource() BOOST_NOEXCEPT {
  psi_memory_resource *global_default_mr = new psi_memory_resource{};
  return global_default_mr;
}

// using erasing_psi_memory_resource = erasing_memory_resource;
using pmr_string = boost::container::basic_string<
    char, std::char_traits<char>,
    boost::container::pmr::polymorphic_allocator<char>>;

#endif  // GLOBAL_DEFAULT_MR_INCLUDED
