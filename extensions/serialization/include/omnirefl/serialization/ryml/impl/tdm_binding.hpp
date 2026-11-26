#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

struct serialize_config_t;

template <typename Config, typename SelectMapping>
struct map_by_schema_t;

template <typename Config, typename SelectMapping>
struct deserialize_t;

template <typename SelectMapping>
struct as_owning_tree_t;

template <typename SelectMapping>
struct as_ref_tree_t;

template <typename SelectMapping>
struct serialize_t;

namespace detail {

template <typename Config, typename SelectMapping>
struct tdm_for_map_by_schema {
  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> map_by_schema_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>, SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
      /*select_mapping=*/this->derived().select_mapping,
    };
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> map_by_schema_t<
      compat::decay_t<compat::invoke_result_t<F &&, Config &&>>,
      SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
      /*select_mapping=*/std::move(this->derived().select_mapping),
    };
  }

  /** (QoL) Transform data member `.select_mapping` inplace, in one expression.
   * f :: const SelectMapping& -> NewSelector. Other bound data is copied. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const SelectMapping &>::value))
  constexpr auto tdm_select_mapping(F &&f) const &
    -> map_by_schema_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, const SelectMapping &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/this->derived().config,
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        this->derived().select_mapping),
    };
  }

  /** (QoL) Transform `.select_mapping`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_select_mapping(F &&f) &&
    -> map_by_schema_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, SelectMapping &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/std::move(this->derived().config),
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().select_mapping)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_by_schema_t<Config, SelectMapping> &derived() const & {
    return static_cast<const map_by_schema_t<Config, SelectMapping> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_by_schema_t<Config, SelectMapping> &derived() & {
    return static_cast<map_by_schema_t<Config, SelectMapping> &>(*this);
  }
};

template <typename Config, typename SelectMapping>
struct tdm_for_deserialize {
  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>, SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
      /*select_mapping=*/this->derived().select_mapping,
    };
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, Config &&>>, SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
      /*select_mapping=*/std::move(this->derived().select_mapping),
    };
  }

  /** (QoL) Transform data member `.select_mapping` inplace, in one expression.
   * f :: const SelectMapping& -> NewSelector. Other bound data is copied. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const SelectMapping &>::value))
  constexpr auto tdm_select_mapping(F &&f) const &
    -> deserialize_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, const SelectMapping &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/this->derived().config,
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        this->derived().select_mapping),
    };
  }

  /** (QoL) Transform `.select_mapping`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_select_mapping(F &&f) &&
    -> deserialize_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, SelectMapping &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/std::move(this->derived().config),
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().select_mapping)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const deserialize_t<Config, SelectMapping> &derived() const & {
    return static_cast<const deserialize_t<Config, SelectMapping> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR deserialize_t<Config, SelectMapping> &derived() & {
    return static_cast<deserialize_t<Config, SelectMapping> &>(*this);
  }
};

template <typename SelectMapping>
struct tdm_for_as_owning_tree {
  /// (QoL) Transform `.select_mapping` in one expression.
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const SelectMapping &>::value))
  constexpr auto tdm_select_mapping(F &&f) const &
    -> as_owning_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, const SelectMapping &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        this->derived().select_mapping),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.select_mapping`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_select_mapping(F &&f) &&
    -> as_owning_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, SelectMapping &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().select_mapping)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<SelectMapping>::value))
  constexpr auto tdm_config(F &&f) const & -> as_owning_tree_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/this->derived().select_mapping,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<SelectMapping>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> as_owning_tree_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/std::move(this->derived().select_mapping),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const as_owning_tree_t<SelectMapping> &derived() const & {
    return static_cast<const as_owning_tree_t<SelectMapping> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR as_owning_tree_t<SelectMapping> &derived() & {
    return static_cast<as_owning_tree_t<SelectMapping> &>(*this);
  }
};

template <typename SelectMapping>
struct tdm_for_as_ref_tree {
  /// (QoL) Transform `.select_mapping` in one expression.
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const SelectMapping &>::value))
  constexpr auto tdm_select_mapping(F &&f) const &
    -> as_ref_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, const SelectMapping &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        this->derived().select_mapping),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.select_mapping`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_select_mapping(F &&f) &&
    -> as_ref_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, SelectMapping &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().select_mapping)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<SelectMapping>::value))
  constexpr auto tdm_config(F &&f) const & -> as_ref_tree_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/this->derived().select_mapping,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<SelectMapping>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> as_ref_tree_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*select_mapping=*/std::move(this->derived().select_mapping),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const as_ref_tree_t<SelectMapping> &derived() const & {
    return static_cast<const as_ref_tree_t<SelectMapping> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR as_ref_tree_t<SelectMapping> &derived() & {
    return static_cast<as_ref_tree_t<SelectMapping> &>(*this);
  }
};

template <typename SelectMapping>
struct tdm_for_serialize {
  /** (QoL) Transform data member `.format` inplace, in one expression.
   * f :: EmitType -> EmitType. The returned binding copies select_mapping. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_convertible<compat::invoke_result_t<F &&,
      const ::ryml::EmitType_e &>, ::ryml::EmitType_e>::value
      && std::is_copy_constructible<SelectMapping>::value))
  constexpr auto tdm_format(F &&f) const & -> serialize_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/compat::invoke(std::forward<F>(f), this->derived().format),
      /*select_mapping=*/this->derived().select_mapping,
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.format`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_convertible<compat::invoke_result_t<F &&,
      ::ryml::EmitType_e &&>, ::ryml::EmitType_e>::value
      && std::is_constructible<SelectMapping, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_format(F &&f) && -> serialize_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().format)),
      /*select_mapping=*/std::move(this->derived().select_mapping),
      /*config=*/this->derived().config,
    };
  }

  /// (QoL) Transform `.select_mapping` in one expression.
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const SelectMapping &>::value))
  constexpr auto tdm_select_mapping(F &&f) const &
    -> serialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, const SelectMapping &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        this->derived().select_mapping),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.select_mapping`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, SelectMapping &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_select_mapping(F &&f) &&
    -> serialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, SelectMapping &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*select_mapping=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().select_mapping)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<SelectMapping>::value))
  constexpr auto tdm_config(F &&f) const & -> serialize_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*select_mapping=*/this->derived().select_mapping,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<SelectMapping>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) && -> serialize_t<SelectMapping> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*select_mapping=*/std::move(this->derived().select_mapping),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const serialize_t<SelectMapping> &derived() const & {
    return static_cast<const serialize_t<SelectMapping> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR serialize_t<SelectMapping> &derived() & {
    return static_cast<serialize_t<SelectMapping> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
