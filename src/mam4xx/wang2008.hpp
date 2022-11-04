#ifndef MAM4XX_WANG2008_HPP
#define MAM4XX_WANG2008_HPP

#include <haero/haero.hpp>

namespace mam4::wang2008 {

using Real = haero::Real;
using PackType = haero::PackType;

/// The functions in this file implement parameterizations described in
/// Wang and Penner, Aerosol indirect forcing in a global model with particle
/// nucleation, Atmos. Chem. Phys. Discuss. 8, pp. 13943–13998 (2008)

/// These parameterizations assume that nucleated particles are 1 nm in
/// diameter.

/// Computes the nucleation rate within the planetary boundary layer using a
/// first-order reaction (Wang 2008 eq 1) adopted from the case studies in
/// Shito et al (2006).
/// @param [in] c_h2so4 The number concentration of H2SO4 gas [cm-3]
KOKKOS_INLINE_FUNCTION
PackType first_order_pbl_nucleation_rate(const PackType &c_h2so4) {
  return 1e-6 * c_h2so4;
}

/// Computes the nucleation rate within the planetary boundary layer using a
/// second-order reaction (Wang 2008 eq 2) adopted from the case studies in
/// Shito et al (2006).
/// @param [in] c_h2so4 The number concentration of H2SO4 gas [cm-3]
KOKKOS_INLINE_FUNCTION
PackType second_order_pbl_nucleation_rate(const PackType &c_h2so4) {
  return 1e-12 * c_h2so4;
}

} // namespace mam4::wang2008

#endif