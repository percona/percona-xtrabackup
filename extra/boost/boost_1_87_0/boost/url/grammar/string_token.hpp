//
// Copyright (c) 2021 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_STRING_TOKEN_HPP
#define BOOST_URL_GRAMMAR_STRING_TOKEN_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/detail/except.hpp>
#include <memory>
#include <string>

namespace boost {
namespace urls {
namespace string_token {

/** Base class for string tokens, and algorithm parameters

    This abstract interface provides a means
    for an algorithm to generically obtain a
    modifiable, contiguous character buffer
    of prescribed size. As the author of an
    algorithm simply declare an rvalue
    reference as a parameter type.

    <br>

    Instances of this type are intended only
    to be used once and then destroyed.

    @par Example
    The declared function accepts any
    temporary instance of `arg` to be
    used for writing:
    @code
    void algorithm( string_token::arg&& dest );
    @endcode

    To implement the interface for your type
    or use-case, derive from the class and
    implement the prepare function.
*/
struct arg
{
    /** Return a modifiable character buffer

        This function attempts to obtain a
        character buffer with space for at
        least `n` characters. Upon success,
        a pointer to the beginning of the
        buffer is returned. Ownership is not
        transferred; the caller should not
        attempt to free the storage. The
        buffer shall remain valid until
        `this` is destroyed.

        @note
        This function may only be called once.
        After invoking the function, the only
        valid operation is destruction.
    */
    virtual char* prepare(std::size_t n) = 0;

    /// Virtual destructor
    virtual ~arg() = default;

    /// Default constructor
    arg() = default;

    /// Default move constructor
    arg(arg&&) = default;

    /// Deleted copy constructor
    arg(arg const&) = delete;

    /// Deleted move assignment
    arg& operator=(arg&&) = delete;

    /// Deleted copy assignment
    arg& operator=(arg const&) = delete;
};

//------------------------------------------------

/** Metafunction returning true if T is a StringToken
*/
#ifdef BOOST_URL_DOCS
template<class T>
using is_token = __see_below__;
#else
namespace see_below {
/** Metafunction returning true if T is a StringToken
 */
template<class T, class = void>
struct is_token : std::false_type {};

/** Metafunction returning true if T is a StringToken
 */
template<class T>
struct is_token<T, void_t<
    decltype(std::declval<T&>().prepare(
        std::declval<std::size_t>())),
    decltype(std::declval<T&>().result())
    > > : std::integral_constant<bool,
        std::is_convertible<decltype(
            std::declval<T&>().result()),
            typename T::result_type>::value &&
        std::is_same<decltype(
            std::declval<T&>().prepare(0)),
            char*>::value &&
        std::is_base_of<arg, T>::value &&
        std::is_convertible<T const volatile*,
            arg const volatile*>::value
    >
{
};
} // see_below

/** Metafunction returning true if T is a StringToken
 */
template<class T>
using is_token = see_below::is_token<T>;
#endif

//------------------------------------------------

/** A token for returning a plain string
*/
#ifdef BOOST_URL_DOCS
using return_string = __implementation_defined__;
#else
namespace implementation_defined {
struct return_string
    : arg
{
    using result_type = std::string;

    char*
    prepare(std::size_t n) override
    {
        s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return std::move(s_);
    }

private:
    result_type s_;
};
} // implementation_defined

/** A token for returning a plain string
 */
using return_string = implementation_defined::return_string;
#endif

//------------------------------------------------

/** A token for appending to a plain string
*/
#ifdef BOOST_URL_DOCS
template<
    class Allocator =
        std::allocator<char>>
__implementation_defined__
append_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Allocator>& s);
#else
namespace implementation_defined {
template<class Alloc>
struct append_to_t
    : arg
{
    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    using result_type = string_type&;

    explicit
    append_to_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        std::size_t n0 = s_.size();
        if(n > s_.max_size() - n0)
            urls::detail::throw_length_error();
        s_.resize(n0 + n);
        return &s_[n0];
    }

    result_type
    result() noexcept
    {
        return s_;
    }

private:
    string_type& s_;
};
} // implementation_defined

/** Create a token for appending to a plain string
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::append_to_t<Alloc>
append_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::append_to_t<Alloc>(s);
}
#endif

//------------------------------------------------

/** A token for assigning to a plain string
*/
#ifdef BOOST_URL_DOCS
template<
    class Allocator =
        std::allocator<char>>
__implementation_defined__
assign_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Allocator>& s);
#else
namespace implementation_defined {
template<class Alloc>
struct assign_to_t
    : arg
{
    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    using result_type = string_type&;

    explicit
    assign_to_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return s_;
    }

private:
    string_type& s_;
};
} // implementation_defined

/** A token for assigning to a plain string
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::assign_to_t<Alloc>
assign_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::assign_to_t<Alloc>(s);
}
#endif

//------------------------------------------------

/** A token for producing a durable core::string_view from a temporary string
*/
#ifdef BOOST_URL_DOCS
template<
    class Allocator =
        std::allocator<char>>
__implementation_defined__
preserve_size(
    std::basic_string<
        char,
        std::char_traits<char>,
        Allocator>& s);
#else
namespace implementation_defined {
template<class Alloc>
struct preserve_size_t
    : arg
{
    using result_type = core::string_view;

    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    explicit
    preserve_size_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        n_ = n;
        // preserve size() to
        // avoid value-init
        if(s_.size() < n)
            s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return core::string_view(
            s_.data(), n_);
    }

private:
    string_type& s_;
    std::size_t n_ = 0;
};
} // implementation_defined

/** A token for producing a durable core::string_view from a temporary string
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::preserve_size_t<Alloc>
preserve_size(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::preserve_size_t<Alloc>(s);
}
#endif

} // string_token

namespace grammar {
namespace string_token = ::boost::urls::string_token;
} // grammar

} // urls
} // boost

#endif
