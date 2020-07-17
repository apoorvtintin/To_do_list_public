/*
 * @file state.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file provide dbg assert macros;
 */

#ifndef _ASSERT_H_
#define _ASSERT_H_
#include <stdio.h>
#include <assert.h>

#define dbg_requires(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)

#endif
