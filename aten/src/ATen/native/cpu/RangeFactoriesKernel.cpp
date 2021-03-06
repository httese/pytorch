#include <cmath>
#include <ATen/Config.h>
#include <ATen/Dispatch.h>

#include <ATen/cpu/vec256/vec256.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>


namespace at { namespace native {
namespace {

using namespace vec256;

static void linspace_kernel(TensorIterator& iter, Scalar scalar_start, Scalar scalar_end, int64_t steps) {
  AT_DISPATCH_ALL_TYPES_AND_C10_COMPLEX(iter.dtype(), "linspace_cpu", [&]() {
    // step should be of double type for all integral types
    using step_t = std::conditional_t<std::is_integral<scalar_t>::value, double, scalar_t>;
    const scalar_t start = scalar_start.to<scalar_t>();
    const scalar_t end = scalar_end.to<scalar_t>();
    const step_t step = static_cast<step_t>(end - start) / (steps - 1);
    int64_t halfway = steps / 2;
    at::parallel_for(0, steps, internal::GRAIN_SIZE, [&](int64_t p_begin, int64_t p_end) {
      int64_t idx(p_begin);
      TensorIterator it(iter);
      cpu_serial_kernel_vec(
          it,
          [start, end, step, halfway, steps, &idx]() -> scalar_t {
            if (idx < halfway) {
              return start + step * (idx++);
            } else {
              return end - step * (steps - (idx++) - 1);
            }
          },
          [start, end, step, halfway, steps, &idx]() -> Vec256<scalar_t> {
            Vec256<scalar_t> res;
            if (idx < halfway) {
              res = Vec256<scalar_t>::arange(start + step * idx, step);
            } else {
              res = Vec256<scalar_t>::arange(
                  end - step * (steps - idx - 1), step);
            }
            idx += Vec256<scalar_t>::size();
            return res;
          }, {p_begin, p_end});
    });
  });
}

} // anonymous namespace

REGISTER_DISPATCH(linspace_stub, &linspace_kernel);

}} // namespace at::native
