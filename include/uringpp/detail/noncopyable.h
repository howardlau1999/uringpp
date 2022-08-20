#pragma once

namespace uringpp {

class noncopyable {
public:
  noncopyable() = default;
  noncopyable(noncopyable &&) = default;
  noncopyable(noncopyable const &) = delete;
  noncopyable &operator=(noncopyable const &) = delete;
  noncopyable &operator=(noncopyable &&) = default;
};

} // namespace uringpp