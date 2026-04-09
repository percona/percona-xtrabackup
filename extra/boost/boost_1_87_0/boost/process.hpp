// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011 Jeff Flinn, Boris Schaeling
// Copyright (c) 2012 Boris Schaeling
// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROCESS_HPP
#define BOOST_PROCESS_HPP

/**
 * \file boost/process.hpp
 *
 * Convenience header which includes all public and platform-independent
 * boost.process header files.
 */

#if !defined(BOOST_PROCESS_VERSION)
#define BOOST_PROCESS_VERSION 1
#endif

#if BOOST_PROCESS_VERSION == 1
#include <boost/process/v1.hpp>
#elif BOOST_PROCESS_VERSION == 2
#include <boost/process/v2.hpp>
#else
#error "Unknown boost process version"
#endif

#endif
