#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <type_traits>
#include <vector>

struct record { int number; };
enum class state { ready };

struct names {
  template <typename M>
  std::vector<std::string> operator()(omni::record_meta_t<M> m) const {
    return {m.type_name()};
  }
};

struct enum_name {
  template <typename M>
  std::string operator()(omni::enum_meta_t<M> m) const {
    return m.type_name();
  }
};

struct reference {
  template <typename T>
  int &operator()(omni::record_binding_t<T> r) const {
    return r.value().number;
  }
};

struct enum_value {
  template <typename T>
  int operator()(omni::enum_binding_t<T> e) const {
    return static_cast<int>(e.value());
  }
};

static_assert(std::is_same<decltype(omni::reflected_call(
    names{}, omni::type_t<record>{})), std::vector<std::string>>::value,
  "Record visitors retain their declared result before generation");
static_assert(std::is_same<decltype(omni::reflected_call(
    enum_name{}, omni::type_t<state>{})), std::string>::value,
  "Enum visitors retain their declared result before generation");
static_assert(std::is_same<decltype(omni::reflected_call(
    reference{}, std::declval<record &>())), int &>::value,
  "Binding visitors retain reference results before generation");
static_assert(std::is_same<decltype(omni::reflected_call(
    enum_value{}, state::ready)), int>::value,
  "Enum binding visitors retain their declared result before generation");

// These calls must not instantiate visitor bodies while metadata is absent.
std::vector<std::string> use_names() {
  const auto result = omni::reflected_call(names{}, omni::type_t<record>{});
  return result;
}

#if defined(__cpp_concepts)
std::vector<std::string> use_concepts() {
  return omni::reflected_call(
    [](omni::record_meta auto, omni::enum_binding auto)
      -> std::vector<std::string> {
      return {};
    },
    omni::type_t<record>{}, state::ready);
}
#endif
