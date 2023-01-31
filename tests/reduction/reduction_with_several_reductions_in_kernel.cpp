/*******************************************************************************
//
//  SYCL 2020 Conformance Test Suite
//
//  Provides tests for several reductions in one kernel.
//
*******************************************************************************/
#include "../common/disabled_for_test_case.h"

// FIXME: re-enable when sycl::reduction is implemented in hipSYCL
#if !SYCL_CTS_COMPILING_WITH_HIPSYCL
#include "reduction_with_several_reductions_in_kernel.h"
#endif

namespace reduction_with_several_reductions_in_kernel {
using namespace sycl_cts;

// FIXME: re-enable when span reduction is supported in ComputeCpp and
// sycl::reduction is implemented in hipSYCL
DISABLED_FOR_TEST_CASE(ComputeCpp, hipSYCL)
("reduction_with_several_reductions_in_kernel", "[reduction]")({
  auto queue = util::get_cts_object::queue();
  reduction_with_several_reductions_in_kernel_h::run_all_tests(queue);
});
}  // namespace reduction_with_several_reductions_in_kernel
