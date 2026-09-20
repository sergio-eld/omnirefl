#include <gtest/gtest.h>

#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace function_metadata_test {

struct request {
  int id;
};

struct response {
  int id;
};

struct opaque;

/** Create a response from a request.
 * \param[in] input Request to copy.
 * @param audited Records whether the call is audited.
 * \param context Optional opaque context.
 * \returns The created response.
 */
response create(const request &input, bool audited, opaque *context) {
  return {input.id + (audited && context ? 1 : 0)};
}

int undocumented(int) {
  return 0;
}

/**
 * @brief Read @p input.
 * @details Preserve the detailed operation documentation.
 * @param input Value to return.
 * @return The supplied value.
 */
int command_documented(int input) {
  return input;
}

template <int Value>
int specialized_operation() {
  return Value;
}

int inaccessible_owner_only() {
  return 0;
}

int public_dependency_only() {
  return 0;
}

#if defined(__cpp_nontype_template_parameter_auto)
using create_function = omni::function_t<create>;
using undocumented_function = omni::function_t<undocumented>;

static_assert( //
  omni::traits::is<create_function,
    omni::compat::decay_t<decltype(OMNI_FUNCTION_TAG(&create))>>(),
  "the compatibility tag must preserve the modern function tag type");
#else
using create_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&create))>;
using undocumented_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&undocumented))>;
#endif

using command_documented_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&command_documented))>;
using first_specialization_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&specialized_operation<1>))>;
using second_specialization_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&specialized_operation<2>))>;
using inaccessible_owner_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&inaccessible_owner_only))>;
using public_dependency_function = omni::compat::decay_t<decltype(
  OMNI_FUNCTION_TAG(&public_dependency_only))>;

static_assert(std::is_empty<create_function>::value,
  "a function tag must not carry metadata");

struct routes {
  create_function create;
};

struct qualified_routes {
  const create_function create;
};

struct undocumented_routes {
  undocumented_function operation;
};

struct command_documented_routes {
  command_documented_function operation;
};

struct specialized_routes {
  first_specialization_function first;
  second_specialization_function second;
};

struct inaccessible_routes_owner {
private:
  struct routes {
    inaccessible_owner_function operation;
  };

public:
  routes value;
};

struct public_dependency_routes {
  public_dependency_function operation;
};

struct public_dependency_owner {
  public_dependency_routes value;
};

struct method_routes {
  int offset;

  /** Read a response.
   * @param input Request to read.
   * @param audited Adds the audit offset.
   * @return The routed response.
   */
  response read(const request &input, bool audited) const {
    return {input.id + (audited ? offset : 0)};
  }

  int overloaded(int) const { return 0; }
  int overloaded(bool) const { return 0; }
  static int static_function() { return 0; }
};

struct method_base {
  int inherited() const { return 815; }
};

struct inherited_methods: method_base {};

struct hidden_inherited_method: method_base {
  int inherited;
};

struct method_name_collision {
  int method_read;

  int read_t() const { return 815; }
};

struct conversion_method {
  struct result {};

  operator result() const { return {}; }
};

struct call_operator_method {
  int operator()() const { return 815; }
};

template <typename... T>
struct packed_method {
  void call(T...) const {}
};

template <typename Function>
struct dependent_function_routes {
  Function operation;
};

template <typename Function>
struct reversed_dependent_function_routes {
  Function operation;
};

template <typename T>
struct stable_function_routes {
  create_function create;
};

struct parameter_observation {
  std::string first_name;
  std::string first_documentation;
  std::string first_type;
  std::string first_qualified_type;
  std::string second_name;
  std::string second_documentation;
  std::string third_name;
  std::string third_documentation;
  bool first_actual_type;
  bool second_actual_type;
  bool third_actual_type;
};

struct inspect_parameters {
  template <typename First, typename Second, typename Third>
  parameter_observation operator()(First, Second, Third) const {
    return {
      /*first_name=*/First::name(),
      /*first_documentation=*/First::documentation(),
      /*first_type=*/First::spelled_type_name(),
      /*first_qualified_type=*/First::spelled_qualified_type_name(),
      /*second_name=*/Second::name(),
      /*second_documentation=*/Second::documentation(),
      /*third_name=*/Third::name(),
      /*third_documentation=*/Third::documentation(),
      /*first_actual_type=*/
      std::is_same<const request &, typename First::type>::value,
      /*second_actual_type=*/std::is_same<bool, typename Second::type>::value,
      /*third_actual_type=*/
      std::is_same<opaque *, typename Third::type>::value,
    };
  }
};

struct observation {
  std::string documentation;
  std::size_t arity;
  parameter_observation parameters;
  std::string return_documentation;
  std::string return_type;
  std::string return_qualified_type;
  bool return_actual_type;
  bool request_reflected;
  bool response_reflected;
  int invoked_id;
};

struct inspect_function_field {
  template <typename Field>
  observation operator()(Field) const {
    using meta = typename Field::meta;

    return {
      /*documentation=*/meta::documentation(),
      /*arity=*/meta::arity(),
      /*parameters=*/
      omni::compat::apply(inspect_parameters{}, meta::parameters()),
      /*return_documentation=*/meta::returns().documentation(),
      /*return_type=*/meta::returns().spelled_type_name(),
      /*return_qualified_type=*/
      meta::returns().spelled_qualified_type_name(),
      /*return_actual_type=*/
      std::is_same<response, typename meta::return_type>::value,
#if defined(OMNI_TOOL_RUN)
      // Generated dependency metadata is available only after discovery.
      /*request_reflected=*/false,
      /*response_reflected=*/false,
#else
      /*request_reflected=*/omni::is_reflected<request>::value,
      /*response_reflected=*/omni::is_reflected<response>::value,
#endif
      /*invoked_id=*/
      omni::compat::invoke(meta::pointer(), request{815}, true, nullptr).id,
    };
  }
};

struct inspect_routes {
  template <typename Meta>
  observation operator()(Meta meta) const {
    return omni::compat::apply(inspect_function_field{}, meta.public_fields());
  }
};

struct documentation_observation {
  std::string function;
  std::string parameter_name;
  std::string parameter;
  std::string result;
};

struct parameter_documentation {
  std::string name;
  std::string documentation;
};

struct inspect_parameter_documentation {
  template <typename Parameter>
  parameter_documentation operator()(Parameter) const {
    return {
      /*name=*/Parameter::name(),
      /*documentation=*/Parameter::documentation(),
    };
  }
};

struct inspect_documentation_field {
  template <typename Field>
  documentation_observation operator()(Field) const {
    using meta = typename Field::meta;
    const parameter_documentation parameter = omni::compat::apply(
      inspect_parameter_documentation{},
      meta::parameters());

    return {
      /*function=*/meta::documentation(),
      /*parameter_name=*/parameter.name,
      /*parameter=*/parameter.documentation,
      /*result=*/meta::returns().documentation(),
    };
  }
};

struct inspect_documentation_routes {
  template <typename Meta>
  documentation_observation operator()(Meta meta) const {
    return omni::compat::apply(inspect_documentation_field{},
      meta.public_fields());
  }
};

struct inspect_specialized_fields {
  template <typename First, typename Second>
  std::pair<int, int> operator()(First, Second) const {
    using first = typename First::meta;
    using second = typename Second::meta;

    return {
      omni::compat::invoke(first::pointer()),
      omni::compat::invoke(second::pointer()),
    };
  }
};

struct inspect_specialized_routes {
  template <typename Meta>
  std::pair<int, int> operator()(Meta meta) const {
    return omni::compat::apply(inspect_specialized_fields{},
      meta.public_fields());
  }
};

struct inspect_nested_function {
  template <typename Function>
  int operator()(Function) const {
    return omni::compat::invoke(Function::pointer());
  }
};

struct inspect_nested_routes {
  template <typename Field>
  int operator()(Field) const {
    using meta = omni::meta_for<typename Field::type>;

    return omni::compat::apply(inspect_nested_function{},
      meta::public_fields());
  }
};

struct inspect_nested_owner {
  template <typename Meta>
  int operator()(Meta meta) const {
    return omni::compat::apply(inspect_nested_routes{},
      meta.public_fields());
  }
};

struct method_observation {
  std::string name;
  std::string documentation;
  std::string first_parameter;
  std::string return_documentation;
  std::size_t arity;
  bool member_pointer;
  int invoked_id;
};

struct inspect_method_parameters {
  template <typename First, typename Second>
  std::string operator()(First, Second) const {
    static_assert(std::is_same<const request &, typename First::type>::value,
      "the first method parameter type must be preserved");
    static_assert(std::is_same<bool, typename Second::type>::value,
      "the second method parameter type must be preserved");

    return First::name();
  }
};

struct inspect_method {
  template <typename Method>
  method_observation operator()(Method method) const {
    return {
      /*name=*/Method::name(),
      /*documentation=*/Method::documentation(),
      /*first_parameter=*/omni::compat::apply(inspect_method_parameters{},
        Method::parameters()),
      /*return_documentation=*/Method::returns().documentation(),
      /*arity=*/Method::arity(),
      /*member_pointer=*/
      std::is_member_function_pointer<typename Method::pointer_type>::value,
      /*invoked_id=*/method(request{800}, true).id,
    };
  }
};

struct inspect_methods {
  template <typename Binding>
  method_observation operator()(Binding binding) const {
    return omni::compat::apply(inspect_method{}, binding.public_methods());
  }
};

struct inspect_inherited_method {
  template <typename Method>
  std::pair<std::string, int> operator()(Method method) const {
    return {Method::name(), method()};
  }
};

struct inspect_inherited_methods {
  template <typename Binding>
  std::pair<std::string, int> operator()(Binding binding) const {
    return omni::compat::apply(inspect_inherited_method{},
      binding.public_methods());
  }
};

struct inspect_method_count {
  template <typename Binding>
  std::size_t operator()(Binding binding) const {
    return std::tuple_size<decltype(binding.public_methods())>::value;
  }
};

struct inspect_field_count {
  template <typename Binding>
  std::size_t operator()(Binding binding) const {
    return std::tuple_size<decltype(binding.public_fields())>::value;
  }
};

} // namespace function_metadata_test

TEST(function_metadata, binds_non_overloaded_public_methods) {
  namespace fm = function_metadata_test;

  const fm::method_observation result = omni::reflected_call(
    fm::inspect_methods{},
    fm::method_routes{/*offset=*/15});

  EXPECT_EQ("read", result.name);
  EXPECT_EQ("Read a response.", result.documentation);
  EXPECT_EQ("input", result.first_parameter);
  EXPECT_EQ("The routed response.", result.return_documentation);
  EXPECT_EQ(std::size_t{2}, result.arity);
  EXPECT_TRUE(result.member_pointer);
  EXPECT_EQ(815, result.invoked_id);
}

TEST(function_metadata, binds_visible_inherited_methods) {
  namespace fm = function_metadata_test;

  const std::pair<std::string, int> result = omni::reflected_call(
    fm::inspect_inherited_methods{},
    fm::inherited_methods{});

  EXPECT_EQ("inherited", result.first);
  EXPECT_EQ(815, result.second);
}

TEST(function_metadata, omits_hidden_inherited_methods) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(fm::inspect_method_count{},
      fm::hidden_inherited_method{}));
}

TEST(function_metadata, method_names_do_not_collide_with_field_names) {
  namespace fm = function_metadata_test;

  fm::method_name_collision value{/*method_read=*/0};

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_field_count{}, value));
  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_method_count{}, value));
}

TEST(function_metadata, omits_conversion_operators) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(fm::inspect_method_count{}, fm::conversion_method{}));
}

TEST(function_metadata, names_and_binds_call_operators) {
  namespace fm = function_metadata_test;

  const std::pair<std::string, int> result = omni::reflected_call(
    fm::inspect_inherited_methods{},
    fm::call_operator_method{});

  EXPECT_EQ("operator()", result.first);
  EXPECT_EQ(815, result.second);
}

TEST(function_metadata, omits_parameter_pack_methods) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(fm::inspect_method_count{}, fm::packed_method<int>{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(fm::inspect_method_count{},
      fm::packed_method<int, int>{}));
}

TEST(function_metadata, treats_dependent_function_tags_as_ordinary_fields) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_field_count{},
      fm::dependent_function_routes<fm::create_function>{}));
  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_field_count{},
      fm::dependent_function_routes<int>{}));
}

TEST(function_metadata,
  treats_dependent_function_tags_as_fields_in_reverse_order) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_field_count{},
      fm::reversed_dependent_function_routes<int>{}));
  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(fm::inspect_field_count{},
      fm::reversed_dependent_function_routes<fm::create_function>{}));
}

TEST(function_metadata, keeps_non_dependent_function_tags_in_templates) {
  namespace fm = function_metadata_test;

  const fm::observation result = omni::reflected_call(fm::inspect_routes{},
    fm::stable_function_routes<int>{});

  EXPECT_EQ(std::size_t{3}, result.arity);
  EXPECT_EQ(815, result.invoked_id);
}

TEST(function_metadata, exposes_signature_documentation_and_pointer) {
  namespace fm = function_metadata_test;

  const fm::observation result =
    omni::reflected_call(fm::inspect_routes{}, fm::routes{});

  EXPECT_EQ("Create a response from a request.", result.documentation);
  EXPECT_EQ(std::size_t{3}, result.arity);

  EXPECT_EQ("input", result.parameters.first_name);
  EXPECT_EQ("Request to copy.", result.parameters.first_documentation);
  EXPECT_EQ("const request &", result.parameters.first_type);
  EXPECT_EQ("const function_metadata_test::request &",
    result.parameters.first_qualified_type);
  EXPECT_EQ("audited", result.parameters.second_name);
  EXPECT_EQ("Records whether the call is audited.",
    result.parameters.second_documentation);
  EXPECT_EQ("context", result.parameters.third_name);
  EXPECT_EQ("Optional opaque context.",
    result.parameters.third_documentation);
  EXPECT_TRUE(result.parameters.first_actual_type);
  EXPECT_TRUE(result.parameters.second_actual_type);
  EXPECT_TRUE(result.parameters.third_actual_type);

  EXPECT_EQ("The created response.", result.return_documentation);
  EXPECT_EQ("response", result.return_type);
  EXPECT_EQ("function_metadata_test::response",
    result.return_qualified_type);
  EXPECT_TRUE(result.return_actual_type);
  EXPECT_TRUE(result.request_reflected);
  EXPECT_TRUE(result.response_reflected);
  EXPECT_EQ(815, result.invoked_id);
}

TEST(function_metadata, treats_qualified_fields_as_the_same_function_tag) {
  namespace fm = function_metadata_test;

  const fm::observation result = omni::reflected_call(fm::inspect_routes{},
    fm::qualified_routes{});

  EXPECT_EQ(815, result.invoked_id);
}

TEST(function_metadata, undocumented_signature_has_empty_documentation) {
  namespace fm = function_metadata_test;

  const fm::documentation_observation result = omni::reflected_call(
    fm::inspect_documentation_routes{},
    fm::undocumented_routes{});

  EXPECT_TRUE(result.function.empty());
  EXPECT_TRUE(result.parameter_name.empty());
  EXPECT_TRUE(result.parameter.empty());
  EXPECT_TRUE(result.result.empty());
}

TEST(function_metadata, preserves_block_and_inline_commands) {
  namespace fm = function_metadata_test;

  const fm::documentation_observation result = omni::reflected_call(
    fm::inspect_documentation_routes{},
    fm::command_documented_routes{});

  EXPECT_EQ("Read input.\nPreserve the detailed operation documentation.",
    result.function);
  EXPECT_EQ("input", result.parameter_name);
  EXPECT_EQ("Value to return.", result.parameter);
  EXPECT_EQ("The supplied value.", result.result);
}

TEST(function_metadata, keeps_distinct_function_template_specializations) {
  namespace fm = function_metadata_test;

  const std::pair<int, int> result = omni::reflected_call(
    fm::inspect_specialized_routes{},
    fm::specialized_routes{});

  EXPECT_EQ(1, result.first);
  EXPECT_EQ(2, result.second);
}

TEST(function_metadata, resolves_a_function_through_a_private_nested_owner) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(0,
    omni::reflected_call(fm::inspect_nested_owner{},
      fm::inaccessible_routes_owner{}));
}

TEST(function_metadata, resolves_a_function_through_a_public_dependency) {
  namespace fm = function_metadata_test;

  EXPECT_EQ(0,
    omni::reflected_call(fm::inspect_nested_owner{},
      fm::public_dependency_owner{}));
}
