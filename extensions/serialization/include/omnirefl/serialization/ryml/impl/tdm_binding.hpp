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

template <typename Config, typename MapNode>
struct map_by_schema_t;

template <typename Config, typename MapNode>
struct deserialize_t;

template <typename MapNode>
struct as_owning_tree_t;

template <typename MapNode>
struct as_ref_tree_t;

template <typename MapNode>
struct serialize_t;

namespace detail {

template <typename Config, typename MapNode>
struct tdm_for_map_by_schema {
  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> map_by_schema_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>, MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
      /*map_node=*/this->derived().map_node,
    };
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> map_by_schema_t<
      compat::decay_t<compat::invoke_result_t<F &&, Config &&>>, MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
      /*map_node=*/std::move(this->derived().map_node),
    };
  }

  /** (QoL) Transform data member `.map_node` inplace, in one expression.
   * f :: const MapNode& -> NewMapNode. Other bound data is copied. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const MapNode &>::value))
  constexpr auto tdm_map_node(F &&f) const &
    -> map_by_schema_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, const MapNode &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/this->derived().config,
      /*map_node=*/compat::invoke(std::forward<F>(f), this->derived().map_node),
    };
  }

  /** (QoL) Transform `.map_node`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_map_node(F &&f) &&
    -> map_by_schema_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, MapNode &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/std::move(this->derived().config),
      /*map_node=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().map_node)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_by_schema_t<Config, MapNode> &derived() const & {
    return static_cast<const map_by_schema_t<Config, MapNode> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_by_schema_t<Config, MapNode> &derived() & {
    return static_cast<map_by_schema_t<Config, MapNode> &>(*this);
  }
};

template <typename Config, typename MapNode>
struct tdm_for_deserialize {
  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>, MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
      /*map_node=*/this->derived().map_node,
    };
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, Config &&>>, MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
      /*map_node=*/std::move(this->derived().map_node),
    };
  }

  /** (QoL) Transform data member `.map_node` inplace, in one expression.
   * f :: const MapNode& -> NewMapNode. Other bound data is copied. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const MapNode &>::value))
  constexpr auto tdm_map_node(F &&f) const &
    -> deserialize_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, const MapNode &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/this->derived().config,
      /*map_node=*/compat::invoke(std::forward<F>(f), this->derived().map_node),
    };
  }

  /** (QoL) Transform `.map_node`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_map_node(F &&f) &&
    -> deserialize_t<Config, compat::decay_t<
      compat::invoke_result_t<F &&, MapNode &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*config=*/std::move(this->derived().config),
      /*map_node=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().map_node)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const deserialize_t<Config, MapNode> &derived() const & {
    return static_cast<const deserialize_t<Config, MapNode> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR deserialize_t<Config, MapNode> &derived() & {
    return static_cast<deserialize_t<Config, MapNode> &>(*this);
  }
};

template <typename MapNode>
struct tdm_for_as_owning_tree {
  /** (QoL) Transform data member `.map_node` inplace, in one expression. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const MapNode &>::value))
  constexpr auto tdm_map_node(F &&f) const &
    -> as_owning_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, const MapNode &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/compat::invoke(std::forward<F>(f), this->derived().map_node),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.map_node`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_map_node(F &&f) &&
    -> as_owning_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, MapNode &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().map_node)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<MapNode>::value))
  constexpr auto tdm_config(F &&f) const & -> as_owning_tree_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/this->derived().map_node,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<MapNode>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) && -> as_owning_tree_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/std::move(this->derived().map_node),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const as_owning_tree_t<MapNode> &derived() const & {
    return static_cast<const as_owning_tree_t<MapNode> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR as_owning_tree_t<MapNode> &derived() & {
    return static_cast<as_owning_tree_t<MapNode> &>(*this);
  }
};

template <typename MapNode>
struct tdm_for_as_ref_tree {
  /** (QoL) Transform data member `.map_node` inplace, in one expression. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const MapNode &>::value))
  constexpr auto tdm_map_node(F &&f) const &
    -> as_ref_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, const MapNode &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/compat::invoke(std::forward<F>(f), this->derived().map_node),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.map_node`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_map_node(F &&f) &&
    -> as_ref_tree_t<compat::decay_t<
      compat::invoke_result_t<F &&, MapNode &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().map_node)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<MapNode>::value))
  constexpr auto tdm_config(F &&f) const & -> as_ref_tree_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/this->derived().map_node,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<MapNode>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) && -> as_ref_tree_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*map_node=*/std::move(this->derived().map_node),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const as_ref_tree_t<MapNode> &derived() const & {
    return static_cast<const as_ref_tree_t<MapNode> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR as_ref_tree_t<MapNode> &derived() & {
    return static_cast<as_ref_tree_t<MapNode> &>(*this);
  }
};

template <typename MapNode>
struct tdm_for_serialize {
  /** (QoL) Transform data member `.format` inplace, in one expression.
   * f :: EmitType -> EmitType. The returned binding copies map_node. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_convertible<compat::invoke_result_t<F &&,
      const ::ryml::EmitType_e &>, ::ryml::EmitType_e>::value
      && std::is_copy_constructible<MapNode>::value))
  constexpr auto tdm_format(F &&f) const & -> serialize_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/compat::invoke(std::forward<F>(f), this->derived().format),
      /*map_node=*/this->derived().map_node,
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.format`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_convertible<compat::invoke_result_t<F &&,
      ::ryml::EmitType_e &&>, ::ryml::EmitType_e>::value
      && std::is_constructible<MapNode, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_format(F &&f) && -> serialize_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().format)),
      /*map_node=*/std::move(this->derived().map_node),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform data member `.map_node` inplace, in one expression. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const MapNode &>::value))
  constexpr auto tdm_map_node(F &&f) const &
    -> serialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, const MapNode &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*map_node=*/compat::invoke(std::forward<F>(f), this->derived().map_node),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Transform `.map_node`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, MapNode &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_map_node(F &&f) &&
    -> serialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, MapNode &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*map_node=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().map_node)),
      /*config=*/this->derived().config,
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<
      F &&, const serialize_config_t &>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_copy_constructible<MapNode>::value))
  constexpr auto tdm_config(F &&f) const & -> serialize_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*map_node=*/this->derived().map_node,
      /*config=*/compat::invoke(std::forward<F>(f), this->derived().config),
    };
  }

  /** (QoL) Functional update of the shared serialization policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, serialize_config_t &&>),
    (std::is_constructible<serialize_config_t, Result>::value
      && std::is_move_constructible<MapNode>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) && -> serialize_t<MapNode> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*format=*/this->derived().format,
      /*map_node=*/std::move(this->derived().map_node),
      /*config=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().config)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const serialize_t<MapNode> &derived() const & {
    return static_cast<const serialize_t<MapNode> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR serialize_t<MapNode> &derived() & {
    return static_cast<serialize_t<MapNode> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
