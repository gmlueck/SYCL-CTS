/*******************************************************************************
//
//  SYCL 2020 Conformance Test Suite
//
//  Copyright (c) 2022-2023 The Khronos Group Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
*******************************************************************************/

#include "../common/common.h"
#include "../common/disabled_for_test_case.h"
#include "../common/random.h"
#include "../common/type_coverage.h"

#include <random>
#include <sstream>

namespace device_selector_aspect {
using namespace sycl_cts;

/** List of all device aspects. */
#define ASPECT_LIST                                                            \
  sycl::aspect::cpu, sycl::aspect::gpu, sycl::aspect::accelerator,             \
      sycl::aspect::custom, sycl::aspect::host_debuggable, sycl::aspect::fp16, \
      sycl::aspect::fp64, sycl::aspect::atomic64, sycl::aspect::image,         \
      sycl::aspect::online_compiler, sycl::aspect::online_linker,              \
      sycl::aspect::queue_profiling, sycl::aspect::usm_device_allocations,     \
      sycl::aspect::usm_host_allocations,                                      \
      sycl::aspect::usm_atomic_host_allocations,                               \
      sycl::aspect::usm_shared_allocations,                                    \
      sycl::aspect::usm_atomic_shared_allocations,                             \
      sycl::aspect::usm_system_allocations

/** Return a list of all defined aspects. */
static std::vector<sycl::aspect> get_aspect_list() {
#ifdef SYCL_CTS_COMPILING_WITH_DPCPP
  WARN(
      "DPCPP does not define sycl::aspect::emulated."
      "Skipping test cases for this aspect.");
#endif
  static const std::vector<sycl::aspect> types = {ASPECT_LIST
#ifndef SYCL_CTS_COMPILING_WITH_DPCPP
                                                  ,
                                                  sycl::aspect::emulated
#endif
  };
  return types;
}

/** Return a named value pack of all defined aspects. */
static auto get_aspect_pack() {
#ifdef SYCL_CTS_COMPILING_WITH_DPCPP
  WARN(
      "DPCPP does not define sycl::aspect::emulated."
      "Skipping test cases for this aspect.");
#endif
  static const auto types = value_pack<sycl::aspect, ASPECT_LIST
#ifndef SYCL_CTS_COMPILING_WITH_DPCPP
                                       ,
                                       sycl::aspect::emulated
#endif
                                       >::generate_named();
  return types;
}

/**
 * Check whether all specified constructors for \p aspect_selector are
 * available. */
void check_no_aspects() {
#ifdef SYCL_CTS_COMPILING_WITH_DPCPP
  CHECK(false);  // ensure workaround will be removed
#else
  const auto selector_vector = sycl::aspect_selector({});
  const auto selector_vector_deny = sycl::aspect_selector({}, {});
  const auto selector_args = sycl::aspect_selector();
  const auto selector_params = sycl::aspect_selector<>();
#endif  // SYCL_CTS_COMPILING_WITH_DPCPP
}

/**
 * Given a list of aspects and a list of forbidden aspects, find out if a
 * conforming device exists. */
bool device_exists(const std::vector<sycl::aspect>& aspect_list,
                   const std::vector<sycl::aspect>& deny_list) {
  const auto devices = sycl::device::get_devices();
  return std::any_of(
      devices.cbegin(), devices.cend(), [&](const sycl::device& dev) {
        const auto dev_has_aspect = [&](const sycl::aspect& aspect) {
          return dev.has(aspect);
        };
        return std::all_of(aspect_list.cbegin(), aspect_list.cend(),
                           dev_has_aspect) &&
               std::none_of(deny_list.cbegin(), deny_list.cend(),
                            dev_has_aspect);
      });
}

/**
 * Given a list of aspects, find out if the selector finds a conforming
 * device. */
template <typename Selector>
void test_selector_accept(const Selector& selector,
                          const std::vector<sycl::aspect>& accept_list) {
  sycl::device device{selector};
  for (const auto& aspect : accept_list) {
    CHECK(device.has(aspect));
  }

  sycl::queue queue{selector};
  for (const auto& aspect : accept_list) {
    CHECK(queue.get_device().has(aspect));
  }

  sycl::platform platform{selector};
  for (const auto& aspect : accept_list) {
    check_return_type<bool>(platform.has(aspect),
                            "sycl::platform::has(sycl::aspect)");
  }
}

/**
 * Given a list of forbidden aspects, find out if the selector finds a
 * conforming device. */
template <typename Selector>
void test_selector_deny(const Selector& selector,
                        const std::vector<sycl::aspect>& deny_list) {
  sycl::device device{selector};
  for (const auto& aspect : deny_list) {
    CHECK((!device.has(aspect)));
  }

  sycl::queue queue{selector};
  for (const auto& aspect : deny_list) {
    CHECK((!queue.get_device().has(aspect)));
  }

  sycl::platform platform{selector};
  // If all devices in the platform have an aspect, the platform itself has the
  // aspect. Hence, selected platform should not have any denied aspect.
  for (const auto& aspect : deny_list) {
    CHECK((!platform.has(aspect)));
  }
}

/**
 * Given a selector that selects a device not available in the system,
 * check if the error behavior is correct. */
template <typename Selector>
void check_selector_exception(const Selector& s) {
  INFO(
      "device with requested aspects does not exist, checking if error is "
      "correct");

  try {
    sycl::device device{s};
    FAIL("selected a device when none are available");
  } catch (const sycl::exception& e) {
    // ComputeCpp cannot make comparison
#ifndef SYCL_CTS_COMPILING_WITH_COMPUTECPP
    CHECK(sycl::errc::runtime == e.code());
#endif  // SYCL_CTS_COMPILING_WITH_COMPUTECPP
  }
}

/**
 * Tests whether a given selector conforms to a given list of required
 * aspects. */
template <typename Selector>
void test_selector(const Selector& selector,
                   const std::vector<sycl::aspect>& accept_list) {
  if (device_exists(accept_list, {})) {
    test_selector_accept(selector, accept_list);
  } else {
    check_selector_exception(selector);
  }
}

/**
 * Tests whether a given selector conforms to a given list of required
 * aspects and a list of denied aspects. */
template <typename Selector>
void test_selector(const Selector& selector,
                   const std::vector<sycl::aspect>& accept_list,
                   const std::vector<sycl::aspect>& deny_list) {
  if (device_exists(accept_list, deny_list)) {
    test_selector_accept(selector, accept_list);
    test_selector_deny(selector, deny_list);
  } else {
    check_selector_exception(selector);
  }
}

/**
 * Checks aspect selector's constructors: using an accept list
 * (and no deny list), using an accept list and a deny list, using accepted
 * variadic function arguments, and using accepted variadic template
 * parameters. */
template <sycl::aspect... Aspects>
void check_aspect_selector(const std::vector<sycl::aspect>& deny_list) {
  const std::vector<sycl::aspect> accept_list{Aspects...};
#ifndef SYCL_CTS_COMPILING_WITH_DPCPP
  test_selector(sycl::aspect_selector(accept_list), accept_list);
  if (!deny_list.empty()) {
    test_selector(sycl::aspect_selector(accept_list, deny_list), accept_list,
                  deny_list);
  }
  test_selector(sycl::aspect_selector(Aspects...), accept_list);
  test_selector(sycl::aspect_selector<Aspects...>(), accept_list);
#endif  // SYCL_CTS_COMPILING_WITH_DPCPP
}

/**
 * Functor that checks a selector with a single aspect and an empty list of
 * denied aspects. */
template <typename AspectT>
class check_for_single_aspect {
  static constexpr sycl::aspect aspect = AspectT::value;

 public:
  void operator()(const std::string& aspect_name) {
    INFO("for aspect " << aspect_name);
    check_aspect_selector<aspect>({});
  }
};

/**
 * Functor that checks a selector with two aspects and an empty list of
 * denied aspects. */
template <typename Aspect1T, typename Aspect2T>
class check_for_two_aspects {
  static constexpr sycl::aspect aspect1 = Aspect1T::value;
  static constexpr sycl::aspect aspect2 = Aspect2T::value;

 public:
  void operator()(const std::string& aspect1_name,
                  const std::string& aspect2_name) {
    INFO("for aspects " << aspect1_name << " and " << aspect2_name);
    check_aspect_selector<aspect1, aspect2>({});
  }
};

/**
 * Returns a randomly-sized list of random denied aspects that are not part of
 * the requested aspects \p selected_aspect_list. */
template <typename Rng>
std::vector<sycl::aspect> generate_denied_list(
    const std::vector<sycl::aspect>& aspect_list,
    const std::vector<sycl::aspect>& selected_aspect_list, Rng& rng) {
  std::vector<sycl::aspect> denied_list;
  for (const auto& aspect : aspect_list) {
    bool is_selected =
        std::find(selected_aspect_list.begin(), selected_aspect_list.end(),
                  aspect) != selected_aspect_list.end();
    // if the aspect is already part of the selected aspects, it cannot be
    // part of the denied aspects
    if (is_selected) {
      continue;
    }
    // else, select the aspect with a probability of
    // 1 over (#aspects - #selected aspects), which has an expected value of 1
    unsigned int rng_range = Rng::max() - Rng::min();
    unsigned int non_selected_aspect_count =
        aspect_list.size() - selected_aspect_list.size();
    if (rng() - Rng::min() < rng_range / non_selected_aspect_count) {
      denied_list.push_back(aspect);
    }
  }
  return denied_list;
}

/**
 * Functor that checks a selector with multiple aspects and optionally
 * a list of denied aspects. */
template <typename... AspectsT>
class check_for_multiple_aspects {
  static constexpr std::size_t aspect_count = sizeof...(AspectsT);

 public:
  void operator()(
      const std::vector<sycl::aspect>& deny_list,
      const std::array<std::string, aspect_count>& accept_aspect_names) {
    std::ostringstream os;
    os << "for aspects (" << aspect_count << "):\n";
    for (const auto& aspect_name : accept_aspect_names) {
      os << aspect_name << "\n";
    }
    os << "for denied aspects (" << deny_list.size() << "):\n";
    for (const auto& aspect : deny_list) {
      os << Catch::StringMaker<sycl::aspect>::convert(aspect) << "\n";
    }
    INFO(os.str());

    check_aspect_selector<AspectsT::value...>(deny_list);
  }
};

template <typename... SelectedAspectsT>
constexpr void check_subset() {
  // create a value pack using the selected aspects
  const auto aspect_pack =
      value_pack<sycl::aspect, SelectedAspectsT::value...>::generate_named();
  check_for_multiple_aspects<SelectedAspectsT...>{}({}, aspect_pack.names);
}

template <typename... AspectsT, std::size_t... Indices>
constexpr void create_subset(std::tuple<AspectsT...>,
                             std::index_sequence<Indices...>) {
  using aspect_tuple_t = std::tuple<AspectsT...>;
  // expand indices sequence and use pattern to index into the aspect tuple
  check_subset<typename std::tuple_element<Indices, aspect_tuple_t>::type...>();
}

template <std::size_t SmallestSubset, typename... AspectsT,
          std::size_t... Sizes>
constexpr void create_subsets(std::tuple<AspectsT...>,
                              std::index_sequence<Sizes...>) {
  // create tuple of aspects to pass to function
  auto aspect_tuple = std::tuple<AspectsT...>{};
  // expand sizes pack and use pattern to create one index sequence
  // for each desired subset size
  (create_subset(aspect_tuple,
                 std::make_index_sequence<SmallestSubset + Sizes>{}),
   ...);
}

/** Checks subsets of size \p SmallestSubset, \p SmallestSubset + 1, etc. */
template <std::size_t SmallestSubset, typename... AspectsT>
constexpr void check_for_subset(const named_type_pack<AspectsT...>&) {
  static_assert(sizeof...(AspectsT) >= SmallestSubset);
  constexpr std::size_t subset_count = sizeof...(AspectsT) + 1 - SmallestSubset;
  // create tuple of aspects to pass to function
  auto aspect_tuple = std::tuple<AspectsT...>{};
  // create an index sequence with one index for each subset size
  auto subset_sizes = std::make_index_sequence<subset_count>{};
  create_subsets<SmallestSubset>(aspect_tuple, subset_sizes);
}

template <unsigned int ArrayCount, typename... AspectsT>
struct helper_random {
  static constexpr unsigned int aspect_count = sizeof...(AspectsT);

  template <typename RngCompileTime, unsigned int RemainingElements,
            typename... SelectedAspectsT>
  struct create_single_array {
    template <typename RngRuntime>
    static void check(const std::vector<sycl::aspect>& aspect_list,
                      RngRuntime& rng_runtime) {
      // generate a random index and select the associated element
      constexpr unsigned int idx = RngCompileTime::value % aspect_count;
      using rng_next = typename RngCompileTime::next;
      using elem =
          typename std::tuple_element<idx, std::tuple<AspectsT...>>::type;
      create_single_array<rng_next, RemainingElements - 1, elem,
                          SelectedAspectsT...>::check(aspect_list, rng_runtime);
    }
  };

  template <typename RngCompileTime, typename... SelectedAspectsT>
  struct create_single_array<RngCompileTime, 0, SelectedAspectsT...> {
    template <typename RngRuntime>
    static void check(const std::vector<sycl::aspect>& aspect_list,
                      RngRuntime& rng_runtime) {
      // no elements remaining, instantiate the check with the running array
      static const auto aspect_pack =
          value_pack<sycl::aspect,
                     SelectedAspectsT::value...>::generate_named();
      static const std::vector<sycl::aspect> selected_aspect_list = {
          SelectedAspectsT::value...};
      const std::vector<sycl::aspect> deny_list =
          generate_denied_list(aspect_list, selected_aspect_list, rng_runtime);
      check_for_multiple_aspects<SelectedAspectsT...>{}(deny_list,
                                                        aspect_pack.names);
    }
  };

  template <unsigned int RemainingArrayCount, typename RngCompileTime>
  struct create_arrays {
    template <typename RngRuntime>
    static void check(const std::vector<sycl::aspect>& aspect_list,
                      RngRuntime& rng_runtime) {
      // obtain random non-zero length, no longer than number of aspects
      constexpr unsigned int array_size =
          1 + (RngCompileTime::value % (aspect_count - 1));
      using rng_next = typename RngCompileTime::next;
      // fill the array with random elements
      create_single_array<rng_next, array_size>::check(aspect_list,
                                                       rng_runtime);

      // to prevent generating duplicate indices for consecutive arrays,
      // skip the rng as many steps forward as the number of values that will be
      // required to generate random indices for the previous array
      using rng_skip = typename discard<rng_next, array_size>::type;
      create_arrays<RemainingArrayCount - 1, rng_skip>::check(aspect_list,
                                                              rng_runtime);
    }
  };

  template <typename RngCompileTime>
  struct create_arrays<0, RngCompileTime> {
    // no arrays remaining
    template <typename RngRuntime>
    static void check(const std::vector<sycl::aspect>&, RngRuntime&) {}
  };

  static void check(const std::vector<sycl::aspect>& aspect_list) {
    // random number generator seed is fixed to produce deterministic results
    constexpr unsigned int seed = 1;

    // runtime random number generator for generating the list of
    // denied aspects, which is passed in as a std::vector
    std::minstd_rand rng_runtime(seed);

    // compile-time random number generator for generating aspects, required
    // since aspect selector has a constructor that accepts template arguments.
    using rng_compile_time = minstd_rand<seed>;

    create_arrays<ArrayCount, rng_compile_time>::check(aspect_list,
                                                       rng_runtime);
  }
};

template <unsigned int ArrayCount, typename... AspectsT>
void check_for_random(const named_type_pack<AspectsT...>&,
                      const std::vector<sycl::aspect>& aspect_list) {
  helper_random<ArrayCount, AspectsT...>::check(aspect_list);
}

// DPCPP does not implement sycl::aspect_selector
DISABLED_FOR_TEST_CASE(DPCPP)
("aspect", "[device_selector]")({
#if SYCL_CTS_COMPILING_WITH_COMPUTECPP
  WARN("ComputeCPP cannot compare exception code. Workaround is in place.");
#endif

  // check whether all constructors compile when no aspects are specified
  check_no_aspects();

  // obtain a named value pack of all defined aspects
  const auto aspect_pack = get_aspect_pack();

  // every single aspect
  for_all_combinations<check_for_single_aspect>(aspect_pack);

  // every possible combination of two aspects
  for_all_combinations<check_for_two_aspects>(aspect_pack, aspect_pack);

  // a subset of three aspects, four aspects, five aspects, etc.
  check_for_subset<3>(aspect_pack);

  // obtain a list of all defined aspects
  const std::vector<sycl::aspect> aspect_list = get_aspect_list();

  // randomly-sized list of random aspects (greater than two), in addition to a
  // randomly-generated list of forbidden aspects
  constexpr unsigned int random_aspects_count = 100;
  check_for_random<random_aspects_count>(aspect_pack, aspect_list);
})

}  // namespace device_selector_aspect
