#include "dependency_types.h"
#include <gtest/gtest.h>
#include "structs.h"

#include <omnirefl/reflected_scope.hpp>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

namespace dependency_types {

// Keep this template otherwise unused. The test below verifies that a visible
// primary definition is sufficient when no specialization was instantiated
// beforehand.
template <typename T>
struct uninstantiated_primary_template {
  T value;
};

template <typename T>
struct template_template_box {
  T value;
};

template <template <typename> class Container>
struct template_template_record {
  Container<int> value;
};

template <typename... T>
struct variadic_primary_template {
  std::tuple<T...> value;
};

template <typename T, T Initial>
struct dependent_value_record {
  T value = Initial;
};

int dependent_pointer_initial = 37;

template <typename T, T *Initial>
struct dependent_pointer_record {
  T value = *Initial;
};

template <typename T>
using dependent_pointer = T *;

template <typename T, dependent_pointer<T> Initial>
struct aliased_dependent_pointer_record {
  T value = *Initial;
};

template <template <typename T, T Initial> class Record>
struct dependent_template_template_record {
  Record<int, 41> value;
};

struct private_alias_and_public_field {
  private:
  struct private_child {
    int value;
  };

  using value_type = private_child;

  public:
  private_child exposed;
};

namespace inspect {

static const struct first_field_type_is_reflected_t {
  template <typename Binding>
  bool operator()(const Binding &binding) const {
    using field = typename std::tuple_element<0,
      decltype(binding.accessible_data_members())>::type;

    return omni::is_reflected<typename field::type>::value;
  }
} first_field_type_is_reflected;

} // namespace inspect

} // namespace dependency_types

namespace suite_discovery {

struct first_model { int first; };
struct second_model { int second; };
struct third_model { int third; };
struct first_direct_model { int first; };
struct second_direct_model { int second; };
struct third_direct_model { int third; };
struct touched_model { int touched; };
struct later_model { int later; };
struct unrelated_root { int unrelated; };

template <typename Model>
struct mapping_case {
  // No Model field: discovery must follow this alias through each tuple case.
  using value_type = Model;
};

template <typename Duplicates, typename UnknownFields>
struct conformance_suite {
  /// Duplicate-field cases.
  Duplicates duplicates;

  /// Unknown-field cases.
  UnknownFields unknown_fields;
};

struct inspect_case {
  template <typename _M>
  std::string operator()(omni::record_meta_t<_M>) const {
    using model = typename omni::record_meta_t<_M>::reflected_type::value_type;
    return std::get</*model field=*/0>(
      omni::reflected(omni::type_t<model>{}).accessible_data_members())
      .name();
  }
};

struct inspect_suite {
  template <typename _M>
  std::vector<std::string> operator()(omni::record_meta_t<_M> meta) const {
    const auto fields = meta.accessible_data_members();
    const auto duplicates = std::get</*duplicates=*/0>(fields);
    const auto unknown = std::get</*unknown_fields=*/1>(fields);
    using first_case = typename std::tuple_element</*first case=*/0,
      typename decltype(duplicates)::type>::type;
    using second_case = typename std::tuple_element</*second case=*/1,
      typename decltype(duplicates)::type>::type;
    using third_case = typename std::tuple_element</*first case=*/0,
      typename decltype(unknown)::type>::type;

    return {
      duplicates.name(),
      duplicates.spelled_type_name(),
      duplicates.documentation(),
      unknown.name(),
      unknown.spelled_type_name(),
      unknown.documentation(),
      std::get</*model field=*/0>(
        omni::reflected(omni::type_t<typename first_case::value_type>{})
          .accessible_data_members())
        .name(),
      std::get</*model field=*/0>(
        omni::reflected(omni::type_t<typename second_case::value_type>{})
          .accessible_data_members())
        .name(),
      std::get</*model field=*/0>(
        omni::reflected(omni::type_t<typename third_case::value_type>{})
          .accessible_data_members())
        .name(),
    };
  }
};

struct touch {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.accessible_data_members())>::value;
  }
};

struct inspect_later_model {
  template <typename _M>
  bool operator()(omni::record_meta_t<_M>) const {
    // Test-only ad hoc: this deliberately concrete query is non-dependent.
    // C++ checks it before instantiation; generated traits are absent in tool
    // runs. The guard lets the test inspect the TU-wide result at runtime.
#if !defined(OMNI_TOOL_RUN)
    return omni::is_reflected<later_model>::value;
#else
    return false;
#endif
  }
};

struct touch_assignment {
  template <typename Model>
  std::size_t operator=(omni::type_t<Model>) const {
    return omni::reflected_call(touch{}, omni::type_t<Model>{});
  }
};

template <typename Model>
struct inspect_touched {
  template <typename _M>
  std::string operator()(omni::record_meta_t<_M>) const {
    // Model is not reachable from the immediate reflected_call root.
    return std::get</*model field=*/0>(
      omni::reflected(omni::type_t<Model>{}).accessible_data_members())
      .name();
  }
};

} // namespace suite_discovery

namespace {

TEST(example_value, field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, reflected_name) {
  namespace dt = dependency_types;

  EXPECT_EQ("example_value",
    omni::reflected_call(dt::inspect::reflected_name,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, const_lvalue_route) {
  namespace dt = dependency_types;

  const dt::example_value value{"constant", 15, 16.23};
  EXPECT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(dt::inspect::field_names, value));
}

TEST(example_value, rvalue_route) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::example_value{"temporary", 23, 42.0}));
}

TEST(field_dependency, level_1_value) {
  namespace dt = dependency_types;

  const dt::field_dep_level_1 v{dt::resolved::as_field{1}};
  const std::string r =
    omni::reflected_call(dt::as_field::get_dependency_name, v);
  EXPECT_EQ("field_dep_level_1::as_field:int", r);
}

TEST(field_dependency, level_1_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_1"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::field_dep_level_1{dt::resolved::as_field{1}}));
}

TEST(field_dependency, level_1_first_field_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("as_field",
    omni::reflected_call(dt::inspect::first_field_type_name,
      dt::field_dep_level_1{dt::resolved::as_field{1}}));
}

TEST(field_dependency, private_alias_does_not_shadow_public_field_path) {
  namespace dt = dependency_types;

  dt::private_alias_and_public_field input{};

  EXPECT_TRUE(
    omni::reflected_call(dt::inspect::first_field_type_is_reflected, input));
}

TEST(field_dependency, level_2_value) {
  namespace dt = dependency_types;

  const dt::field_dep_level_2 v{
    dt::field_mid_level_2{dt::resolved::as_field_layer_2{2}},
  };
  const std::string r =
    omni::reflected_call(dt::as_field::get_dependency_name_layer_2, v);
  EXPECT_EQ("field_dep_level_2::field_mid_level_2::as_field_layer_2:int", r);
}

TEST(field_dependency, level_2_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_2"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_level_2{}));
}

TEST(field_dependency, level_2_first_field_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("field_mid_level_2",
    omni::reflected_call(dt::inspect::first_field_type_name,
      dt::field_dep_level_2{}));
}

TEST(field_dependency, two_fields_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"first_dependency", "second_dependency"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_two_fields{}));
}

TEST(field_dependency, two_fields_dependency_types) {
  namespace dt = dependency_types;

  const dt::field_dep_two_fields v{};
  EXPECT_EQ("as_field",
    omni::reflected_call(dt::inspect::first_field_type_name, v));
  EXPECT_EQ("as_field_layer_2",
    omni::reflected_call(dt::inspect::second_field_type_name, v));
}

TEST(field_dependency, level_3_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_3"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_level_3{}));
}

TEST(alias_dependency, level_1_value) {
  namespace dt = dependency_types;

  const dt::alias_dep_level_1 v{};
  const std::string r =
    omni::reflected_call(dt::as_alias::get_dependency_name, v);
  EXPECT_EQ("alias_dep_level_1::as_alias:int", r);
}

TEST(alias_dependency, level_1_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_1{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_1{}));
}

TEST(alias_dependency, level_2_value) {
  namespace dt = dependency_types;

  const dt::alias_dep_level_2 v{};
  const std::string r =
    omni::reflected_call(dt::as_alias::get_dependency_name_layer_2, v);
  EXPECT_EQ("alias_dep_level_2::alias_dep_mid_level_2::as_alias_layer_2:int",
    r);
}

TEST(alias_dependency, level_2_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_2{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_2{}));
}

TEST(alias_dependency, level_3_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_3{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_3{}));
}

TEST(alias_dependency, reflected_enum_alias_dependency) {
  namespace dt = dependency_types;

  const dt::enum_alias_dep v{};
  const std::string r =
    omni::reflected_call(dt::as_alias::get_dependency_name, v);
  EXPECT_EQ("enum_alias_dep::scoped_status:int", r);
}

TEST(template_dependency, tuple_level_1_value) {
  namespace dt = dependency_types;

  const dt::template_dep_level_1 v{
    std::tuple<dt::resolved::as_template_arg>{
      dt::resolved::as_template_arg{1},
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name, v);
  EXPECT_EQ("template_dep_level_1::as_template_arg:int", r);
}

TEST(template_dependency, tuple_level_1_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"tpl_field_1"}),
    omni::reflected_call(dt::inspect::field_names, dt::template_dep_level_1{}));
}

TEST(template_dependency, reflected_enum_template_arg_dependency) {
  namespace dt = dependency_types;

  const dt::enum_template_dep v{
    std::tuple<dt::scoped_fixed_status>{
      dt::scoped_fixed_status::medium,
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name, v);
  EXPECT_EQ("enum_template_dep::scoped_fixed_status:int", r);
}

TEST(template_dependency, tuple_level_2_value) {
  namespace dt = dependency_types;

  const dt::template_dep_level_2 v{
    omni::compat::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2},
      },
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2, v);
  EXPECT_EQ("template_dep_level_2::tuple::as_template_arg_layer_2:int", r);
}

TEST(template_dependency, mpark_tuple_level_2_value) {
  namespace dt = dependency_types;

  const dt::mpark_template_dep_level_2 v{
    mpark::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2},
      },
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2, v);
  EXPECT_EQ("mpark_template_dep_level_2::tuple::as_template_arg_layer_2:int",
    r);
}

TEST(record_template, type_parameter_field_names) {
  namespace dt = dependency_types;

  using record = dt::primary_template_record<dt::resolved::as_template_arg>;
  EXPECT_EQ((std::vector<std::string>{"value", "count"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, metadata_for_uninstantiated_primary_template) {
  namespace dt = dependency_types;

  // This must remain the specialization's first reference in this translation
  // unit; omni::type_t itself does not instantiate the specialization.
  EXPECT_EQ(1,
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<dt::uninstantiated_primary_template<int>>{}));
}

TEST(record_template, metadata_for_template_template_parameter) {
  namespace dt = dependency_types;

  // This must remain the specialization's first reference in this translation
  // unit; a template-template argument must not make it appear incomplete.
  EXPECT_EQ(1,
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<dt::template_template_record<dt::template_template_box>>{}));
}

TEST(record_template, metadata_for_variadic_primary_template) {
  namespace dt = dependency_types;

  EXPECT_EQ(1,
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<dt::variadic_primary_template<int, double>>{}));
}

TEST(record_template, metadata_for_dependent_nontype_parameter) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<dt::dependent_value_record<int, 31>>{}));
}

TEST(record_template, metadata_for_composed_dependent_nontype_parameter) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<
        dt::dependent_pointer_record<int, &dt::dependent_pointer_initial>>{}));
}

TEST(record_template, metadata_for_aliased_dependent_nontype_parameter) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<dt::aliased_dependent_pointer_record<int,
        &dt::dependent_pointer_initial>>{}));
}

TEST(record_template, metadata_for_dependent_template_template_parameter) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      omni::type_t<
        dt::dependent_template_template_record<dt::dependent_value_record>>{}));
}

TEST(annotations, record_type_annotation) {
  namespace dt = dependency_types;

  EXPECT_EQ("annotation: inherited base type",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::resolved::as_inherited_struct{}));
}

TEST(annotations, enum_type_annotation) {
  namespace dt = dependency_types;

  EXPECT_EQ("annotation: scoped status enum",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::scoped_status{}));
}

TEST(annotations, public_field_annotations) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"annotation: as_field value"}),
    omni::reflected_call(dt::inspect::field_annotations,
      dt::resolved::as_field{}));
}

TEST(annotations, documentation_comment_forms) {
  namespace dt = dependency_types;

  EXPECT_EQ("annotation: comment forms type",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::annotation_comment_forms{}));
  EXPECT_EQ((std::vector<std::string>{
              "annotation: slash line field",
              "annotation: bang line field",
              "annotation: slash block field",
              "annotation: bang block field",
              "annotation: trailing slash field",
              "annotation: trailing bang field",
            }),
    omni::reflected_call(dt::inspect::field_annotations,
      dt::annotation_comment_forms{}));
}

TEST(annotations, documentation_preserves_full_sanitized_text) {
  namespace dt = dependency_types;

  EXPECT_EQ("Account data shared across requests and persisted records.\n\n"
            "Assigned storage is an implementation detail.",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::documentation_example{}));
  EXPECT_EQ((std::vector<std::string>{
              "Stable identifier used to reference this account\n"
              "across requests and persisted records.\n\n"
              "Assigned by the storage layer when the account is created.",
            }),
    omni::reflected_call(dt::inspect::field_annotations,
      dt::documentation_example{}));
}

TEST(annotations, unannotated_type_and_field_are_empty) {
  namespace dt = dependency_types;

  EXPECT_EQ("",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::unannotated_record{}));
  EXPECT_EQ((std::vector<std::string>{"", ""}),
    omni::reflected_call(dt::inspect::field_annotations,
      dt::unannotated_record{}));
}

TEST(annotations, inherited_base_field_annotation_is_preserved) {
  namespace dt = dependency_types;

  using record = dt::template_derived<dt::resolved::as_template_arg>;
  EXPECT_EQ("annotation: template derived type",
    omni::reflected_call(dt::inspect::reflected_annotation, record{}));
  EXPECT_EQ((std::vector<std::string>{
              "annotation: template base value",
              "annotation: template derived own field",
            }),
    omni::reflected_call(dt::inspect::field_annotations, record{}));
}

TEST(annotations, primary_template_annotation_is_shared_by_instantiations) {
  namespace dt = dependency_types;

  using first = dt::primary_template_record<dt::resolved::as_template_arg>;
  using second = dt::primary_template_record<dt::resolved::as_field>;

  EXPECT_EQ("annotation: primary template record",
    omni::reflected_call(dt::inspect::reflected_annotation, first{}));
  EXPECT_EQ("annotation: primary template record",
    omni::reflected_call(dt::inspect::reflected_annotation, second{}));
  EXPECT_EQ((std::vector<std::string>{
              "annotation: primary template value",
              "annotation: primary template count",
            }),
    omni::reflected_call(dt::inspect::field_annotations, first{}));
}

TEST(annotations, crtp_annotations_are_preserved) {
  namespace dt = dependency_types;

  EXPECT_EQ("annotation: CRTP derived type",
    omni::reflected_call(dt::inspect::reflected_annotation,
      dt::crtp_derived{}));
  EXPECT_EQ((std::vector<std::string>{
              "annotation: CRTP base field",
              "annotation: CRTP own field",
            }),
    omni::reflected_call(dt::inspect::field_annotations, dt::crtp_derived{}));
}

TEST(annotations, constexpr_type_and_field_annotations) {
  namespace dt = dependency_types;
  using record = dt::primary_template_record<dt::resolved::as_template_arg>;

  EXPECT_TRUE(omni::reflected_call(dt::inspect::constexpr_annotations,
    record{}));
}

TEST(record_template, value_parameter_field_names) {
  namespace dt = dependency_types;

  using record = dt::value_param_template_record<dt::resolved::as_template_arg, 3>;
  EXPECT_EQ((std::vector<std::string>{"value", "fixed_values"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, default_allocator_parameter_field_names) {
  namespace dt = dependency_types;

  using record =
    dt::default_allocator_template_record<dt::resolved::as_template_arg>;
  EXPECT_EQ((std::vector<std::string>{"values"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, typed_allocator_parameter_field_names) {
  namespace dt = dependency_types;

  using record = dt::typed_allocator_template_record<dt::resolved::as_template_arg,
    dt::custom_allocator<dt::resolved::as_template_arg>>;
  EXPECT_EQ((std::vector<std::string>{"values"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, allocator_policy_template_parameter_field_names) {
  namespace dt = dependency_types;

  using record = dt::allocator_policy_template_record<dt::custom_allocator>;
  EXPECT_EQ((std::vector<std::string>{"vec", "map"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, template_base_fields_are_flattened) {
  namespace dt = dependency_types;

  using record = dt::template_derived<dt::resolved::as_template_arg>;
  EXPECT_EQ((std::vector<std::string>{"base_value", "own_field"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, crtp_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"crtp_base_field", "crtp_own_field"}),
    omni::reflected_call(dt::inspect::field_names, dt::crtp_derived{}));
}

TEST(record_template, crtp_template_base_fields_are_flattened) {
  namespace dt = dependency_types;

  using record = dt::crtp_template_derived<dt::resolved::as_template_arg>;
  EXPECT_EQ((std::vector<std::string>{"crtp_base_field", "crtp_template_field"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

TEST(record_template, nested_template_inside_non_template_parent) {
  namespace dt = dependency_types;

  using record = dt::nested_template_parent::nested_template<int>;
  EXPECT_EQ((std::vector<std::string>{"nested_value"}),
    omni::reflected_call(dt::inspect::field_names, record{}));
}

// TODO: nested records inside record template parents need the enclosing
// primary template declaration to be generated for the `_wrt` root accessor.
//
// TEST(record_template, non_template_nested_inside_template_parent) {
//   namespace dt = dependency_types;
//
//   using record = dt::template_parent<int>::nested_non_template;
//   EXPECT_EQ((std::vector<std::string>{"nested_value", "count"}),
//     omni::reflected_call(dt::inspect::field_names, record{}));
// }

// fixme: std::vector<T> dependency fields are not supported by source/header
// shared dependency collection yet. The backend currently special-cases tuple
// and variant only; this stays enabled as a regression route for dependency
// collection.

TEST(sequence_dependency, vector_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("vector_dep_level_1::vector::as_sequence_vector",
    omni::reflected_call(dt::as_sequence_arg::get_vector_value_name,
      dt::vector_dep_level_1{}));
}

TEST(sequence_dependency, vector_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"vector_field"}),
    omni::reflected_call(dt::inspect::field_names, dt::vector_dep_level_1{}));
}

TEST(sequence_dependency, tuple_first_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("tuple_dep_two_values::tuple::as_sequence_tuple",
    omni::reflected_call(dt::as_sequence_arg::get_tuple_value_name,
      dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, tuple_second_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("tuple_dep_two_values::tuple::as_field",
    omni::reflected_call(dt::as_sequence_arg::get_tuple_second_value_name,
      dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, tuple_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"tuple_field"}),
    omni::reflected_call(dt::inspect::field_names, dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, pair_first_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("pair_dep_two_values::pair::as_sequence_pair_first",
    omni::reflected_call(dt::as_sequence_arg::get_pair_first_value_name,
      dt::pair_dep_two_values{}));
}

TEST(sequence_dependency, pair_second_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("pair_dep_two_values::pair::as_sequence_pair_second",
    omni::reflected_call(dt::as_sequence_arg::get_pair_second_value_name,
      dt::pair_dep_two_values{}));
}

TEST(sequence_dependency, dependent_pair_alias_types) {
  namespace dt = dependency_types;
  using record = dt::dependent_pair_dep<dt::resolved::as_dependent_pair_first,
    dt::resolved::as_dependent_pair_second>;

  EXPECT_EQ((std::vector<std::string>{
              "as_dependent_pair_first",
              "as_dependent_pair_second",
            }),
    omni::reflected_call(dt::as_sequence_arg::get_dependent_pair_value_names,
      record{}));
}

TEST(sequence_dependency, compat_variant_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("variant_dep_level_1::variant::as_sequence_variant",
    omni::reflected_call(dt::as_sequence_arg::get_variant_value_name,
      dt::variant_dep_level_1{}));
}

TEST(sequence_dependency, mpark_variant_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("mpark_variant_dep_level_1::variant::as_sequence_variant",
    omni::reflected_call(dt::as_sequence_arg::get_variant_value_name,
      dt::mpark_variant_dep_level_1{}));
}

// fixme: same unsupported vector dependency route as vector_dep_level_1.

TEST(sequence_dependency, nested_vector_tuple_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("nested_vector_tuple_dep::vector::tuple::as_template_arg_layer_2",
    omni::reflected_call(
      dt::as_sequence_arg::get_nested_vector_tuple_value_name,
      dt::nested_vector_tuple_dep{}));
}

TEST(sequence_dependency, nested_vector_tuple_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"nested_field"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::nested_vector_tuple_dep{}));
}

TEST(inheritance_dependency, base_is_reflected) {
  namespace dt = dependency_types;

  const dt::derived_struct derived{4, 8.15};
  const dt::as_inherited_struct::is_base_reflected_t<
    dt::resolved::as_inherited_struct>
    check{};

  EXPECT_TRUE(omni::reflected_call(check, derived));
}

TEST(inheritance_dependency, public_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "derived_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::derived_struct{4, 8.15}));
}

TEST(inheritance_dependency, std_enable_shared_from_this_base_is_ignored) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "name",
      "count",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::shared_from_this_derived{"record", 7}));
}

TEST(inheritance_dependency, multi_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "second_base_field",
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::multi_base_derived{4, "second", 15}));
}

TEST(inheritance_dependency, multi_base_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::multi_base_derived{4, "second", 15}));
}

TEST(inheritance_dependency, deep_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "mid_field",
      "deep_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::deep_derived{4, "middle", 8.15}));
}

TEST(inheritance_dependency, deep_base_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::deep_derived{4, "middle", 8.15}));
}

TEST(inheritance_dependency, three_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "second_base_field",
      "third_base_field",
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::three_base_derived{4, "second", 8.15, "own"}));
}

TEST(inheritance_dependency, three_base_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{4},
    omni::reflected_call(dt::inspect::field_count,
      dt::three_base_derived{4, "second", 8.15, "own"}));
}

TEST(enum_dependency, scoped_enum_is_reflected_directly) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"idle", "busy", "blocked"}),
    omni::reflected_call(dt::inspect::enum_names, dt::scoped_status::idle));
}

TEST(enum_dependency, fixed_enum_is_reflected_directly) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{
              "fixed_status_low",
              "fixed_status_medium",
              "fixed_status_high",
            }),
    omni::reflected_call(dt::inspect::enum_names, dt::fixed_status_low));
}

TEST(enum_dependency, scoped_fixed_enum_is_reflected_directly) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"low", "medium", "high"}),
    omni::reflected_call(dt::inspect::enum_names,
      dt::scoped_fixed_status::low));
}

TEST(enum_dependency, fixed_underlying_aliases_are_canonicalized) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"ready", "done"}),
    omni::reflected_call(dt::inspect::enum_names,
      dt::std_alias_fixed_status::ready));

  EXPECT_EQ((std::vector<std::string>{"waiting", "complete"}),
    omni::reflected_call(dt::inspect::enum_names,
      dt::custom_alias_fixed_status::waiting));
}

TEST(enum_dependency, forward_declarable_enum_holder_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"scoped", "fixed", "scoped_fixed"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::forward_declarable_enum_holder{}));
}

TEST(enum_dependency, forward_declarable_enum_holder_first_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("scoped_status",
    omni::reflected_call(dt::inspect::first_field_type_name,
      dt::forward_declarable_enum_holder{}));
}

TEST(enum_dependency, forward_declarable_enum_holder_second_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("fixed_status",
    omni::reflected_call(dt::inspect::second_field_type_name,
      dt::forward_declarable_enum_holder{}));
}

// FIXME: does not compile in generated-header reflection: the plain unscoped enum dependency
// cannot be forward-declared.
//
// TEST(enum_dependency, enum_holder_field_names) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ((std::vector<std::string>{"plain", "scoped", "fixed"}),
//     omni::reflected_call(dt::inspect::field_names,
//       dt::enum_holder{
//         dt::plain_status_pending,
//         dt::scoped_status::idle,
//         dt::fixed_status_low,
//       }));
// }
//
// TEST(enum_dependency, enum_holder_first_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("plain_status",
//     omni::reflected_call(dt::inspect::first_field_type_name,
//       dt::enum_holder{}));
// }
//
// TEST(enum_dependency, enum_holder_second_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("scoped_status",
//     omni::reflected_call(dt::inspect::second_field_type_name,
//       dt::enum_holder{}));
// }

// FIXME: mixed_dependency_holder contains vector_dep_level_1, so it currently
// hits the same unsupported std::vector dependency route.
//
// TEST(mixed_dependency, holder_field_names) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ(
//     (std::vector<std::string>{
//       "value",
//       "field_dependency",
//       "vector_dependency",
//       "enum_dependency",
//     }),
//     omni::reflected_call(dt::inspect::field_names,
//       dt::mixed_dependency_holder{}));
// }
//
// TEST(mixed_dependency, holder_first_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("example_value",
//     omni::reflected_call(dt::inspect::first_field_type_name,
//       dt::mixed_dependency_holder{}));
// }
//
// TEST(mixed_dependency, holder_field_count) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ(std::size_t{4},
//     omni::reflected_call(dt::inspect::field_count,
//       dt::mixed_dependency_holder{}));
// }

TEST(negative_public_only, non_public_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"visible"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::non_public_fields{"visible", 4, 8.15}));
}

TEST(negative_public_only, non_public_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      dt::non_public_fields{"visible", 4, 8.15}));
}

TEST(negative_public_only, private_base_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::private_base_derived{4, "own"}));
}

TEST(negative_public_only, protected_base_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::protected_base_derived{4, "own"}));
}

TEST(negative_public_only, mixed_access_keeps_only_public_members) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "visible",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::mixed_access_derived{1, "visible", 2, 3.5}));
}

TEST(suite_discovery, tuple_cases_discover_every_model_specialization) {
  namespace sd = suite_discovery;
  using suite = sd::conformance_suite<
    std::tuple<sd::mapping_case<sd::first_model>,
      sd::mapping_case<sd::second_model>>,
    std::tuple<sd::mapping_case<sd::third_model>>>;

  EXPECT_EQ((std::vector<std::string>{
              "duplicates", "Duplicates", "Duplicate-field cases.",
              "unknown_fields", "UnknownFields", "Unknown-field cases.",
              "first", "second", "third",
            }),
    omni::reflected_call(sd::inspect_suite{}, omni::type_t<suite>{}));
}

TEST(suite_discovery, separate_calls_discover_every_model_specialization) {
  namespace sd = suite_discovery;

  EXPECT_EQ("first", omni::reflected_call(sd::inspect_case{},
    omni::type_t<sd::mapping_case<sd::first_direct_model>>{}));
  EXPECT_EQ("second", omni::reflected_call(sd::inspect_case{},
    omni::type_t<sd::mapping_case<sd::second_direct_model>>{}));
  EXPECT_EQ("third", omni::reflected_call(sd::inspect_case{},
    omni::type_t<sd::mapping_case<sd::third_direct_model>>{}));
}

TEST(suite_discovery, assignment_touch_exposes_model_in_another_reflected_scope) {
  namespace sd = suite_discovery;

  EXPECT_EQ(1U, (sd::touch_assignment{} = omni::type_t<sd::touched_model>{}));
  EXPECT_EQ("touched", omni::reflected_call(
    sd::inspect_touched<sd::touched_model>{},
    omni::type_t<sd::unrelated_root>{}));
}

// Incidental reflection: the later call supplies TU-wide metadata used by the
// earlier concrete query. Avoid this pattern: removing an unrelated call would
// change the query's result. Derive queries from the root or a discovery route.
TEST(suite_discovery, incidental_query_sees_model_discovered_by_a_later_call) {
  namespace sd = suite_discovery;

  EXPECT_TRUE(omni::reflected_call(sd::inspect_later_model{},
    omni::type_t<sd::unrelated_root>{}));
  EXPECT_EQ(1U, omni::reflected_call(sd::touch{},
    omni::type_t<sd::later_model>{}));
}

namespace tuple_apply_discovery {

struct invoke {
  template <typename Model>
  std::size_t operator()(omni::type_t<Model>) const {
    return omni::reflected_call(suite_discovery::touch{}, omni::type_t<Model>{});
  }
};

// Complete before invocation; generic callable definitions may precede models.
struct model {
  int value;
};

} // namespace tuple_apply_discovery

TEST(suite_discovery, tuple_apply_accepts_a_model_defined_after_the_callable) {
  // Discovered when a model defined after its generic visitor was passed
  // through std::apply: the tool incorrectly reported it as incomplete.
  // Uses std::apply in C++17 and newer, the local implementation before C++17.
  EXPECT_EQ(1U, omni::compat::apply(tuple_apply_discovery::invoke{},
    std::make_tuple(omni::type_t<tuple_apply_discovery::model>{})));
}

namespace lambda_instantiation {

struct count_fields {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.accessible_data_members())>::value;
  }
};

template <typename Model>
std::function<std::size_t()> defer_field_count(omni::type_t<Model> type) {
  return [type] { return omni::reflected_call(count_fields{}, type); };
}

// Keep the model after the template: completeness must be checked at
// instantiation, rather than at the template's definition.
struct model { int value; };

} // namespace lambda_instantiation

TEST(suite_discovery, lambda_in_function_template_accepts_complete_model) {
  namespace li = lambda_instantiation;

  const auto count = li::defer_field_count(omni::type_t<li::model>{});
  EXPECT_EQ(1U, count());
}

} // namespace
