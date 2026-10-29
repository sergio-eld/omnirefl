#include <gtest/gtest.h>

#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct static_record {
  int local;
  static int shared;
  static const int constant = 23;

  private:
  static int hidden;
};

int static_record::shared = 9;
const int static_record::constant;

struct defaulted_record {
  int local = 7;
  static const int shared = 13;
};

struct static_only { static int shared; };

int static_only::shared = 17;

struct static_base {
  static int shared;
  int local;

  static int read(int value) { return value + 1; }
};

int static_base::shared = 5;
struct left_base: static_base {};
struct right_base: static_base {};
struct shared_base: left_base, right_base {};
struct hidden_base: static_base {
  int shared;
  int read;
};

template <typename T>
struct static_template {
  static T shared;
  T local;

  static T identity(T value) { return value; }
};

template <typename T>
T static_template<T>::shared{};

struct function_record {
  int local(int value) const { return value + 1; }
  static int shared(int value) { return value + 2; }
  static int overloaded(int value) { return value; }
  static int overloaded(double value) { return static_cast<int>(value); }

  private:
  static int hidden(int value) { return value; }
};

struct signature_payload { int value; };

struct signature_record {
  static signature_payload pass(signature_payload value) { return value; }
};

using shared_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&function_record::shared))>;

struct tagged_functions {
  shared_function local;
  static shared_function shared;
};

struct static_data_payload { int value; };
struct static_data_dependency { static static_data_payload shared; };

#if defined(__cpp_inline_variables)
struct inline_members {
  inline static int shared = 29;
  inline static constexpr int constant = 31;
  int local;
};
#endif

struct forward_record;
struct forward_record {
  int local;
  static int shared;

  static int read(int value) { return value + 4; }
};

struct selection_left {
  static int field;
  static int run() { return 1; }
};

struct selection_right {
  static long field;
  static int run() { return 2; }
};

struct selected_static: selection_left, selection_right {
  using selection_right::field;
  using selection_right::run;
};

struct ambiguous_static: selection_left, selection_right {};

struct inaccessible_static: selection_right {
  private:
  using selection_right::field;
  using selection_right::run;
};

struct instance_left {
  int field;
  int run() const { return 1; }
};

struct instance_right {
  long field;
  int run() const { return 2; }
};

struct selected_instance: instance_left, instance_right {
  using instance_right::field;
  using instance_right::run;
};

struct instance_root {
  int field;
  int run() const { return 3; }
};

struct instance_left_path: instance_root {};
struct instance_right_path: instance_root {};
struct ambiguous_instance: instance_left_path, instance_right_path {
  using instance_left_path::field;
  using instance_left_path::run;
};

struct private_base {
  int field;
  int run() const { return 4; }
};

struct public_selection: private private_base {
  public:
  using private_base::field;
  using private_base::run;
};

struct alternative_read {
  static int read(int value) { return value + 2; }
};

struct duplicate_function_selection: left_base, right_base, alternative_read {
  using left_base::read;
  using right_base::read;
};

struct enumerator_hiding: selection_left {
  enum { field = 7 };
};

struct instance_enumerator_hiding: instance_left {
  enum { field = 7 };
};

#if defined(__cpp_consteval)
struct immediate_hiding: selection_left {
  static consteval int run() { return 7; }
};
#endif

struct static_defaults {
  static int run(int value = 5) { return value; }
};

struct instance_defaults {
  int run(int value = 7) const { return value; }
};

struct member_properties {
  template <typename... Member>
  std::vector<std::pair<std::string, bool>> operator()(Member... member) const {
    return {{member.name(), member.is_static()}...};
  }
};

struct inspect_data {
  template <typename _M>
  std::vector<std::pair<std::string, bool>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(member_properties{},
      record.accessible_data_members());
  }
};

template <typename Predicate>
struct select_data {
  template <typename _M>
  std::vector<std::pair<std::string, bool>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(member_properties{},
      record.accessible_data_members() | omni::fn::filter(Predicate{}));
  }
};

struct inspect_functions {
  template <typename _M>
  std::vector<std::pair<std::string, bool>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(member_properties{},
      record.accessible_member_functions());
  }
};

struct data_types {
  template <typename... Field>
  std::vector<std::pair<std::string, std::string>> operator()(Field... field) const {
    return {{field.name(), field.spelled_type_name()}...};
  }
};

struct inspect_data_types {
  template <typename _M>
  std::vector<std::pair<std::string, std::string>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(data_types{}, record.accessible_data_members());
  }
};

struct call_selected {
  template <typename... Function>
  std::vector<std::pair<std::string, int>> operator()(Function... function) const {
    return {{function.name(), function()}...};
  }
};

struct invoke_selected {
  template <typename T>
  std::vector<std::pair<std::string, int>> operator()(
    omni::record_binding_t<T> record) const {
    return omni::compat::apply(call_selected{}, record.accessible_member_functions());
  }
};

struct assign_shared {
  template <typename Field, typename Constant>
  std::pair<int, int *> operator()(Field field, Constant constant) const {
    static_assert(Field::is_static(), "shared must be static");
    static_assert(Constant::is_static(), "constant must be static");
    static_assert(Constant::is_const(), "constant must retain constness");
    field.set_value(constant.value());
    return {field.value(), &field.ref()};
  }
};

struct update_static {
  template <typename T>
  std::pair<int, int *> operator()(omni::record_binding_t<T> record) const {
    return omni::compat::apply(assign_shared{},
      record.accessible_data_members()
        | omni::fn::filter(omni::fn::is_static{}));
  }
};

struct initializer_properties {
  template <typename... Field>
  std::vector<std::pair<bool, bool>> operator()(Field... field) const {
    return {{field.has_default_member_initializer(),
      field.has_default_value_access()}...};
  }
};

struct inspect_initializers {
  template <typename _M>
  std::vector<std::pair<bool, bool>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(initializer_properties{},
      record.accessible_data_members());
  }
};

template <typename _M>
OMNI_CPP14_CONSTEXPR std::size_t constant_function_count(
  omni::record_meta_t<_M> metadata) {
  typename omni::record_meta_t<_M>::reflected_type value{};
  auto binding = metadata.bind(value);
  const auto functions = binding.accessible_member_functions();
  return std::tuple_size<decltype(functions)>::value;
}

struct check_constant_function_binding {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> metadata) const {
#if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
    using record = typename omni::record_meta_t<_M>::reflected_type;
    constexpr auto constant_metadata = omni::reflected(omni::type_t<record>{});
    static_assert(2 == constant_function_count(constant_metadata),
      "non-const member-function queries must support constant evaluation");
#endif
    return constant_function_count(metadata);
  }
};

struct invoke_function {
  template <typename Function>
  int operator()(Function function) const {
    static_assert(Function::is_static(), "selected function must be static");
    return function(5);
  }
};

struct invoke_static {
  template <typename T>
  int operator()(omni::record_binding_t<T> record) const {
    return omni::compat::apply(invoke_function{},
      record.accessible_member_functions()
        | omni::fn::filter(omni::fn::is_static{}));
  }
};

struct inspect_signature {
  template <typename Function>
  bool operator()(Function) const {
    return omni::is_reflected<typename Function::return_type>::value;
  }
};

struct inspect_static_dependency {
  template <typename _M>
  bool operator()(omni::record_meta_t<_M> record) const {
    return omni::compat::apply(inspect_signature{},
      record.accessible_member_functions());
  }
};

struct inspect_tag_properties {
  template <typename... Field>
  std::vector<std::pair<bool, bool>> operator()(Field... field) const {
    return {{field.is_static(), field.callable_metadata().is_static()}...};
  }
};

struct inspect_tags {
  template <typename _M>
  std::vector<std::pair<bool, bool>> operator()(
    omni::record_meta_t<_M> record) const {
    return omni::compat::apply(inspect_tag_properties{},
      record.accessible_data_members());
  }
};

struct inspect_data_type {
  template <typename Field>
  bool operator()(Field) const {
    return omni::is_reflected<typename Field::type>::value;
  }
};

struct inspect_static_data_dependency {
  template <typename _M>
  bool operator()(omni::record_meta_t<_M> record) const {
    return omni::compat::apply(inspect_data_type{},
      record.accessible_data_members());
  }
};

struct build_static_only {
  template <typename _M>
  static_only operator()(omni::record_meta_t<_M>) const {
    using record = typename omni::record_meta_t<_M>::reflected_type;
    return omni::refl::aggregate_into<record>(std::tuple<>{});
  }
};

TEST(static_members, reports_data_members_in_declaration_order) {
  const std::vector<std::pair<std::string, bool>> expected{{"local", false},
    {"shared", true},
    {"constant", true}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_data{}, omni::type_t<static_record>{}));
}

TEST(static_members, follows_an_earlier_forward_declaration) {
  const std::vector<std::pair<std::string, bool>> data{
    {"local", false}, {"shared", true}};
  const std::vector<std::pair<std::string, bool>> functions{{"read", true}};

  EXPECT_EQ(data,
    omni::reflected_call(inspect_data{}, omni::type_t<forward_record>{}));
  EXPECT_EQ(functions,
    omni::reflected_call(inspect_functions{}, omni::type_t<forward_record>{}));
  EXPECT_EQ(9, omni::reflected_call(invoke_static{}, forward_record{}));
}

TEST(static_members, filters_static_and_non_static_data_members) {
  const std::vector<std::pair<std::string, bool>> statics{{"shared", true},
    {"constant", true}};
  const std::vector<std::pair<std::string, bool>> non_statics{{"local", false}};

  EXPECT_EQ(statics,
    omni::reflected_call(select_data<omni::fn::is_static>{},
      omni::type_t<static_record>{}));
  EXPECT_EQ(non_statics,
    omni::reflected_call(select_data<omni::fn::non_static>{},
      omni::type_t<static_record>{}));
}

TEST(static_members, writes_shared_state_through_a_const_record) {
  const int saved = static_record::shared;
  const static_record record{7};
  const auto result = omni::reflected_call(update_static{}, record);

  EXPECT_EQ(23, result.first);
  EXPECT_EQ(&static_record::shared, result.second);
  EXPECT_EQ(23, static_record::shared);
  EXPECT_EQ(7, record.local);
  static_record::shared = saved;
}

TEST(static_members, static_initializers_are_not_default_member_initializers) {
  const std::vector<std::pair<bool, bool>> expected{{true, true},
    {false, false}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_initializers{},
      omni::type_t<defaulted_record>{}));
}

TEST(static_members, ignores_statics_when_constructing_an_aggregate) {
  const auto result =
    omni::reflected_call(build_static_only{}, omni::type_t<static_only>{});

  EXPECT_EQ(17, result.shared);
}

TEST(static_members, member_function_bindings_are_constant_from_cpp14) {
  EXPECT_EQ(std::size_t{2},
    omni::reflected_call(check_constant_function_binding{},
      omni::type_t<function_record>{}));
}

TEST(static_members, reports_non_overloaded_static_functions) {
  const std::vector<std::pair<std::string, bool>> expected{{"local", false},
    {"shared", true}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_functions{}, omni::type_t<function_record>{}));
  const function_record record{};
  EXPECT_EQ(7, omni::reflected_call(invoke_static{}, record));
}

TEST(static_members, discovers_static_function_signature_types) {
  EXPECT_TRUE(omni::reflected_call(inspect_static_dependency{},
    omni::type_t<signature_record>{}));
}

TEST(static_members, distinguishes_a_tag_field_from_its_callable) {
  const std::vector<std::pair<bool, bool>> expected{
    {false, true}, {true, true}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_tags{}, omni::type_t<tagged_functions>{}));
}

TEST(static_members, discovers_static_data_member_types) {
  EXPECT_TRUE(omni::reflected_call(inspect_static_data_dependency{},
    omni::type_t<static_data_dependency>{}));
}

#if defined(__cpp_inline_variables)
TEST(static_members, supports_inline_static_data_members) {
  const std::vector<std::pair<std::string, bool>> expected{
    {"shared", true}, {"constant", true}, {"local", false}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_data{}, omni::type_t<inline_members>{}));
}
#endif

TEST(static_members, shared_base_statics_appear_once) {
  const std::vector<std::pair<std::string, bool>> data{{"shared", true}};
  const std::vector<std::pair<std::string, bool>> functions{{"read", true}};

  EXPECT_EQ(data,
    omni::reflected_call(inspect_data{}, omni::type_t<shared_base>{}));
  EXPECT_EQ(functions,
    omni::reflected_call(inspect_functions{}, omni::type_t<shared_base>{}));
  EXPECT_EQ(6, omni::reflected_call(invoke_static{}, shared_base{}));
}

TEST(static_members, derived_members_hide_inherited_statics) {
  const std::vector<std::pair<std::string, bool>> none;

  EXPECT_EQ(none,
    omni::reflected_call(select_data<omni::fn::is_static>{},
      omni::type_t<hidden_base>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_functions{}, omni::type_t<hidden_base>{}));
}

TEST(static_members, using_declarations_select_the_accessible_static_member) {
  const std::vector<std::pair<std::string, std::string>> data{{"field", "long"}};
  const std::vector<std::pair<std::string, int>> functions{{"run", 2}};

  EXPECT_EQ(data,
    omni::reflected_call(inspect_data_types{}, omni::type_t<selected_static>{}));
  EXPECT_EQ(functions,
    omni::reflected_call(invoke_selected{}, selected_static{}));
}

TEST(static_members, using_declarations_select_the_accessible_instance_member) {
  const std::vector<std::pair<std::string, std::string>> data{{"field", "long"}};
  const std::vector<std::pair<std::string, int>> functions{{"run", 2}};

  EXPECT_EQ(data,
    omni::reflected_call(inspect_data_types{}, omni::type_t<selected_instance>{}));
  EXPECT_EQ(functions,
    omni::reflected_call(invoke_selected{}, selected_instance{}));
}

TEST(static_members, excludes_ambiguous_or_inaccessible_names) {
  const std::vector<std::pair<std::string, bool>> none;

  EXPECT_EQ(none,
    omni::reflected_call(inspect_data{}, omni::type_t<ambiguous_static>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_functions{}, omni::type_t<ambiguous_static>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_data{}, omni::type_t<inaccessible_static>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_functions{}, omni::type_t<inaccessible_static>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_data{}, omni::type_t<ambiguous_instance>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_functions{}, omni::type_t<ambiguous_instance>{}));
}

TEST(static_members, public_using_of_private_base_members_uses_record_access) {
  const std::vector<std::pair<std::string, bool>> data{{"field", false}};
  const std::vector<std::pair<std::string, int>> functions{{"run", 4}};

  EXPECT_EQ(data,
    omni::reflected_call(inspect_data{}, omni::type_t<public_selection>{}));
  EXPECT_EQ(functions,
    omni::reflected_call(invoke_selected{}, public_selection{}));
}

TEST(static_members, repeated_using_of_the_same_function_is_not_an_overload) {
  const std::vector<std::pair<std::string, bool>> expected{{"read", true}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_functions{},
      omni::type_t<duplicate_function_selection>{}));
  EXPECT_EQ(6,
    omni::reflected_call(invoke_static{}, duplicate_function_selection{}));
}

TEST(static_members, non_data_declarations_hide_inherited_data_members) {
  const std::vector<std::pair<std::string, bool>> none;

  EXPECT_EQ(none,
    omni::reflected_call(inspect_data{}, omni::type_t<enumerator_hiding>{}));
  EXPECT_EQ(none,
    omni::reflected_call(inspect_data{},
      omni::type_t<instance_enumerator_hiding>{}));
}

#if defined(__cpp_consteval)
TEST(static_members, excluded_functions_still_hide_inherited_functions) {
  const std::vector<std::pair<std::string, bool>> none;

  EXPECT_EQ(none,
    omni::reflected_call(inspect_functions{}, omni::type_t<immediate_hiding>{}));
}
#endif

TEST(static_members, bound_calls_use_declared_default_arguments) {
  const std::vector<std::pair<std::string, int>> statics{{"run", 5}};
  const std::vector<std::pair<std::string, int>> instances{{"run", 7}};

  EXPECT_EQ(statics,
    omni::reflected_call(invoke_selected{}, static_defaults{}));
  EXPECT_EQ(instances,
    omni::reflected_call(invoke_selected{}, instance_defaults{}));
}

TEST(static_members, supports_primary_template_specializations) {
  const std::vector<std::pair<std::string, bool>> expected{{"shared", true},
    {"local", false}};

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_data{}, omni::type_t<static_template<int>>{}));
  EXPECT_EQ(expected,
    omni::reflected_call(inspect_data{},
      omni::type_t<static_template<double>>{}));
  EXPECT_EQ(5, omni::reflected_call(invoke_static{}, static_template<int>{}));
}

} // namespace
