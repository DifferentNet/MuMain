#pragma once

#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <type_traits>

// Cross-platform swprintf replacement
// MinGW blocks swprintf, so we provide mu_swprintf as a safe cross-platform alternative

// Undefine MinGW's blocking macro first
#undef swprintf
#undef swprintf_instead_use_StringCbPrintfW_or_StringCchPrintfW

// Use inline template functions instead of macros to avoid issues with #ifdef in arguments
#ifdef _MSC_VER
  // _snwprintf and the two-argument swprintf are "deprecated" on MSVC (C4996); stdafx.h silences that with
  // _CRT_SECURE_NO_DEPRECATE, and this keeps it quiet for a file that includes only this header.
#pragma warning(push)
#pragma warning(disable : 4996)
  // MSVC: into an array the text is cut at the array's size (a translation longer than the English text once ran
  // into the next field); into a plain pointer it is the unbounded swprintf it always was. _snwprintf, not
  // _snwprintf_s: a cut text must not end the game through the invalid-parameter handler. On a cut the length
  // returned is the cut text's, so a caller adding it to an offset stays inside the array.
  template<typename Buf, typename... Args>
  inline int mu_swprintf(Buf&& buffer, const wchar_t* format, Args... args) {
      using Array = std::remove_reference_t<Buf>;
      if constexpr (std::is_array_v<Array>)
      {
          constexpr size_t count = std::extent_v<Array>;
          static_assert(count > 0, "mu_swprintf needs an array of at least one character");
          const int written = _snwprintf(buffer, count, format, args...);
          if (written >= 0 && static_cast<size_t>(written) < count)
              return written;
          buffer[count - 1] = L'\0';
          return static_cast<int>(wcslen(buffer));
      }
      else
          return swprintf(buffer, format, args...);
  }
  // mu_swprintf_s with explicit size
  template<typename... Args>
  inline int mu_swprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, Args... args) {
      return swprintf_s(buffer, size, format, args...);
  }
  // mu_swprintf_s with array - auto-deduce size (like MSVC's swprintf_s)
  template<size_t N, typename... Args>
  inline int mu_swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, Args... args) {
      return swprintf_s(buffer, N, format, args...);
  }
#pragma warning(pop)
#else
  // GCC/MinGW/Clang: use std::swprintf with an explicit buffer size. For an array
  // the bound is its real size. For a plain pointer, ask the compiler for the size
  // it can see: the same size glibc's _FORTIFY_SOURCE checks swprintf against, so the
  // two can't disagree -- once inlining reveals that the pointer aims into a smaller
  // buffer, the call truncates instead of aborting. Only when the size is unknown
  // (heap, hidden object) does the historical 1024 assumption remain.
#ifndef MU_SWPRINTF_DEFINED
#define MU_SWPRINTF_DEFINED
  // Historical assumption for buffers whose size the compiler cannot see.
  inline constexpr size_t kMuSwprintfUnknownBufferChars = 1024;
  template<typename Buf, typename... Args>
  inline int mu_swprintf(Buf&& buffer, const wchar_t* format, Args... args) {
      using Array = std::remove_reference_t<Buf>;
      if constexpr (std::is_array_v<Array>)
          return std::swprintf(buffer, std::extent_v<Array>, format, args...);
      else
      {
          // Object size type 1 asks for the maximum size: exactly what glibc's
          // _FORTIFY_SOURCE checks swprintf against, so the two can't disagree.
#if defined(__has_builtin)
#if __has_builtin(__builtin_dynamic_object_size)
          const size_t bytes = __builtin_dynamic_object_size(buffer, 1);
#else
          const size_t bytes = __builtin_object_size(buffer, 1);
#endif
#else
          const size_t bytes = __builtin_object_size(buffer, 1);
#endif
          // Unknown (or empty) size keeps the historical assumption; a truly smaller
          // buffer truncates instead of aborting.
          const size_t count = (bytes == static_cast<size_t>(-1) || bytes == 0)
              ? kMuSwprintfUnknownBufferChars
              : bytes / sizeof(wchar_t);
          return std::swprintf(buffer, count, format, args...);
      }
  }
  // mu_swprintf_s with explicit size
  template<typename... Args>
  inline int mu_swprintf_s(wchar_t* buffer, size_t size, const wchar_t* format, Args... args) {
      return std::swprintf(buffer, size, format, args...);
  }
  // mu_swprintf_s with array - auto-deduce size (compatible with MSVC swprintf_s)
  template<size_t N, typename... Args>
  inline int mu_swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, Args... args) {
      return std::swprintf(buffer, N, format, args...);
  }
#endif
#endif
