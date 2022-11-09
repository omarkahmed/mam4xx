#ifndef MAM4XX_CALCSIZE_HPP
#define MAM4XX_CALCSIZE_HPP

#include <haero/atmosphere.hpp>
#include <haero/math.hpp>

// I think the style preference is angle brackets here
#include <mam4xx/aero_config.hpp>
#include <mam4xx/conversions.hpp>

namespace mam4 {

using Atmosphere = haero::Atmosphere;
using Constants = haero::Constants;
using IntPack = haero::IntPackType;
using Pack = haero::PackType;
using PackInfo = haero::PackInfo;
using Real = haero::Real;
using ThreadTeam = haero::ThreadTeam;

using ColumnView = haero::ColumnView;
using RealView = haero::RealView1D;

using haero::max;
using haero::min;
using haero::sqrt;

namespace calcsize {

/*-----------------------------------------------------------------------------
Compute initial dry volume based on bulk mass mixing ratio (mmr) and specie
density  volume = mmr/density
TODO: Is this used?
 -----------------------------------------------------------------------------*/
// KOKKOS_INLINE_FUNCTION
// void compute_dry_volume(const Diagnostics &diagnostics, // in
//                         const Prognostics &prognostics, // in
//                         const ColumnView &dryvol_i,     // out
//                         const ColumnView &dryvol_c)     // out
// {
//   // Pack dryvol = 0;
//   const int nlevels = diagnostics.num_levels();
//   const auto nk = PackInfo::num_packs(nlevels);

//   const auto q_i = prognostics.q_aero_i;
//   const auto q_c = prognostics.q_aero_c;
//   const auto nmodes = AeroConfig::num_modes();

//   for (int k = 0; k < nk; ++k) {
//     for (int imode = 0; imode < nmodes; ++imode) {
//       const auto n_spec = num_species_mode[imode];
//       for (int ispec = 0; ispec < n_spec; ispec++) {
//         const Real inv_density = 1.0 / get_density_aero_species(imode, ispec);
//         dryvol_i(k) += max(0.0, q_i[imode][ispec](k)) * inv_density;
//         dryvol_c(k) += max(0.0, q_c[imode][ispec](k)) * inv_density;
//       } // end imode

//     } // end ispec
//   }   // end k

// } // end

KOKKOS_INLINE_FUNCTION
void compute_dry_volume_k(int k, int imode, const Real inv_density[4][7], 
                          const Prognostics &prognostics, // in
                          Pack &dryvol_i,                 // out
                          Pack &dryvol_c)                 // out
{
  // Pack dryvol = 0;
  const auto q_i = prognostics.q_aero_i;
  const auto q_c = prognostics.q_aero_c;
  dryvol_i = 0;
  dryvol_c = 0;
  const auto n_spec = num_species_mode[imode];
  // int count =0;
  for (int ispec = 0; ispec < n_spec; ispec++) {
    dryvol_i += max(0.0, q_i[imode][ispec](k)) * inv_density[imode][ispec];
    dryvol_c += max(0.0, q_c[imode][ispec](k)) * inv_density[imode][ispec];
    // count += ispec; 
  } // end ispec

} // end

/*
 * \brief Get relaxed limits for volume_to_num (we use relaxed limits for
 * aerosol number "adjustment" calculations via "adjust_num_sizes" subroutine.
 * Note: The relaxed limits will be artificially inflated (or deflated) for the
 * aitken and accumulation modes if "do_aitacc_transfer" flag is true to
 * effectively shut-off aerosol number "adjustment" calculations for these
 * modes because we do the explicit transfer (via "aitken_accum_exchange"
 * subroutine) from one mode to another instead of adjustments for these
 * modes).
 *
 * \note v2nmin and v2nmax are only updated for aitken and accumulation modes.
 */
KOKKOS_INLINE_FUNCTION
void get_relaxed_v2n_limits(const bool do_aitacc_transfer,
                            const bool is_aitken_mode, const bool is_accum_mode,
                            Real &v2nmin,   // in
                            Real &v2nmax,   // in
                            Real &v2nminrl, // out
                            Real &v2nmaxrl) // out
{
  /*
   * Relaxation factor is currently assumed to be a factor of 3 in diameter
   * which makes it 3**3=27 for volume.  i.e. dgnumlo_relaxed = dgnumlo/3 and
   * dgnumhi_relaxed = dgnumhi*3; therefore we use 3**3=27 as a relaxation
   * factor for volume.
   *
   * \see get_relaxed_v2n_limits
   */

  //   intent-ins
  // logical,  intent(in) :: do_aitacc_transfer flag to control whether to
  // transfer aerosols from one mode to another logical,  intent(in) ::
  // is_aitken_mode     true if this mode is aitken mode logical,  intent(in)
  // :: is_accum_mode      true if this mode is accumulation mode

  // intent-(in)outs
  // real(wp), intent(inout) :: v2nmin, v2nmax     volume_to_num min/max ratios
  // real(wp), intent(out)   :: v2nminrl, v2nmaxrl  relaxed counterparts of
  // volume_to_num min/max ratios

  // local
  // (relaxation factor is currently assumed to be a factor of 3 in diameter
  // which makes it 3**3=27 for volume)
  // i.e. dgnumlo_relaxed = dgnumlo/3 and dgnumhi_relaxed = dgnumhi*3;
  // therefore we use 3**3=27 as a relaxation factor for volume

  static constexpr Real relax_factor = 27.0;

  // factor to artificially inflate or deflate v2nmin and v2nmax
  static constexpr Real szadj_block_fac = 1.0e6;

  // default relaxation:
  v2nminrl = v2nmin / relax_factor;
  v2nmaxrl = v2nmax * relax_factor;
  // if do_aitacc_transfer is turned on, we will do the ait<->acc transfer
  // separately in aitken_accum_exchange subroutine, so we are effectively
  // turning OFF the size adjustment for these two modes here by artificially
  // inflating (or deflating) v2min and v2nmax using "szadj_block_fac" and then
  // computing v2minrl and v2nmaxrl based on newly computed v2min and v2nmax.

  if (do_aitacc_transfer) {
    // for aitken mode, divide v2nmin by 1.0e6 to effectively turn off the
    //          adjustment when number is too small (size is too big)
    if (is_aitken_mode)
      v2nmin /= szadj_block_fac;
    // for accumulation, multiply v2nmax by 1.0e6 to effectively turn off the
    //          adjustment when number is too big (size is too small)
    if (is_accum_mode)
      v2nmax *= szadj_block_fac;

    // Also change the v2nmaxrl/v2nminrl so that
    // the interstitial<-->activated number adjustment is effectively turned
    // off
    v2nminrl = v2nmin / relax_factor;
    v2nmaxrl = v2nmax * relax_factor;
  }
}

/*----------------------------------------------------------------------------
 * Compute particle diameter and volume to number ratios using dry bulk volume
 * (drv)
 *--------------------------------------------------------------------------*/
KOKKOS_INLINE_FUNCTION
void update_diameter_and_vol2num(/*std::size_t klev, std::size_t imode, */
                                 const Pack &drv, const Pack &num, Real v2nmin,
                                 Real v2nmax, Real dgnmin, Real dgnmax,
                                 Real cmn_factor, Pack &dgncur_k_i,
                                 Pack &v2ncur_k_i) {
  const auto drv_gt_0 = drv > 0.0;
  if (!drv_gt_0.any())
    return;

  const auto drv_mul_v2nmin = drv * v2nmin;
  const auto drv_mul_v2nmax = drv * v2nmax;

  // auto &dgncur_k_i = dgncur(klev, imode);
  // auto &v2ncur_k_i = v2ncur(klev, imode);

  dgncur_k_i.set(num <= drv_mul_v2nmin, dgnmin);
  dgncur_k_i.set(num >= drv_mul_v2nmax, dgnmax);
  dgncur_k_i.set(num > drv_mul_v2nmin and num < drv_mul_v2nmax,
                 pow((drv / (cmn_factor * num)), (1.0 / 3.0)));

  v2ncur_k_i.set(num <= drv_mul_v2nmin, v2nmin);
  v2ncur_k_i.set(num >= drv_mul_v2nmax, v2nmax);
  v2ncur_k_i.set(num > drv_mul_v2nmin and num < drv_mul_v2nmax, num / drv);
}

KOKKOS_INLINE_FUNCTION
// rename to match ported fortran version
static Pack update_num_adj_tends(const Pack &num, const Pack &num0,
                                 const Pack &dt_inverse) {
  return (num - num0) * dt_inverse;
}

KOKKOS_INLINE_FUNCTION
static Pack min_max_bounded(const Pack &drv, const Pack &v2nmin,
                            const Pack &v2nmax, const Pack &num) {
  return max(drv * v2nmin, min(drv * v2nmax, num));
}

/*
 * \brief number adjustment routine. See the implementation for more detailed
 * comments.
 */
KOKKOS_INLINE_FUNCTION
void adjust_num_sizes(const Pack &drv_i, const Pack &drv_c,
                      const Pack &init_num_i, const Pack &init_num_c,
                      const Real &dt, const Real &v2nmin, const Real &v2nmax,
                      const Real &v2nminrl, const Real &v2nmaxrl,
                      const Real &adj_tscale_inv, const Real &close_to_one,
                      Pack &num_i, Pack &num_c, Pack &dqdt, Pack &dqqcwdt) {

  // intent-ins
  // real(wp), intent(in) :: drv_i, drv_c      dry volumes [TODO:units]
  // real(wp), intent(in) :: init_num_a, init_num_c    initial number mixing
  // ratios [TODO:units] real(wp), intent(in) :: dt                time step
  // [s] real(wp), intent(in) :: v2nmin, v2nmax    volume to number min and
  // max[TODO:units] real(wp), intent(in) :: v2nminrl, v2nmaxrlvolume to number
  // "relaxed" min and max[TODO:units]

  // intent-outs
  // real(wp), intent(out):: num_a, num_c  final number  mixing ratios after
  // size adjument real(wp), intent(out):: dqdt, dqqcwdt  number mixing ratio
  // tendencies

  /*
   *
   * The logic behind the number adjustment is described in detail in the
   * "else" section of the following "if" condition.
   *
   * We accomplish number adjustments in 3 steps:
   *
   *   1. Ensure that number mixing ratios are either zero or positive to
   * begin with. If both of them are zero (or less), we make them zero and
   * update tendencies accordingly (logic in the first "if" block")
   *   2. In this step, we use "relaxed" bounds for bringing number mixing
   *      ratios in their bounds. This is accomplished in three sub-steps
   * [(a), (b) and (c)] described in "Step 2" below.
   *   3. In this step, we use the actual bounds for bringing number mixing
   *      ratios in their bounds. This is also accomplished in three sub-steps
   *      [(a), (b) and (c)] described in "Step 3" below.
   *
   * If the number mixing ratio in a mode is out of mode's min/max range, we
   * re-balance interstitial and cloud borne aerosols such that the number
   * mixing ratio comes within the range. Time step for such an operation is
   * assumed to be one day (in seconds). That is, it is assumed that number
   * mixing ratios will be within range in a day. "adj_tscale" represents that
   * time scale
   *
   */

  // fraction of adj_tscale covered in the current time step "dt"
  const auto frac_adj_in_dt = max(0.0, min(1.0, dt * adj_tscale_inv));

  // inverse of time step
  const auto dtinv = 1.0 / (dt * close_to_one);

  /*
   * The masks below represent four if-else conditions in the original fortran
   * code. The masks represent whether a given branch should be traversed for
   * a given element of the pack, and this pack is passed to the function
   * invocations.
   */
  const auto drva_le_zero = drv_i <= 0.0;
  num_i.set(drva_le_zero, 0.0);

  const auto drvc_le_zero = drv_c <= 0.0;
  num_c.set(drvc_le_zero, 0.0);

  /* If both interstitial (drv_i) and cloud borne (drv_c) dry volumes are zero
   * (or less) adjust numbers(num_i and num_c respectively) for both of them
   * to be zero for this mode and level
   */
  const auto drv_i_c_le_zero = drva_le_zero && drvc_le_zero;
  dqdt.set(drv_i_c_le_zero, update_num_adj_tends(num_i, init_num_i, dtinv));
  dqqcwdt.set(drv_i_c_le_zero, update_num_adj_tends(num_c, init_num_c, dtinv));

  /* if cloud borne dry volume (drv_c) is zero(or less), the interstitial
   * number/volume == total/combined apply step 1 and 3, but skip the relaxed
   * adjustment (step 2, see below)
   */
  const auto only_drvc_le_zero = !drva_le_zero && drvc_le_zero;
  {
    const auto numbnd = min_max_bounded(drv_i, v2nmin, v2nmax, num_i);
    num_i.set(only_drvc_le_zero, num_i + (numbnd - num_i) * frac_adj_in_dt);
  }

  /* interstitial volume is zero, treat similar to above */
  const auto only_drva_le_zero = !drvc_le_zero && drva_le_zero;
  {
    const auto numbnd = min_max_bounded(drv_c, v2nmin, v2nmax, num_c);
    num_c.set(only_drva_le_zero, num_c + (numbnd - num_c) * frac_adj_in_dt);
  }

  /* Note that anything in this scope that touches a pack outside this scope,
   * it must also refer to `drv_i_c_gt_zero`. e.g., `pk.set(drv_i_c_gt_zero &&
   * some_other_cond, val);`
   */
  const auto drv_i_c_gt_zero = !drvc_le_zero && !drva_le_zero;
  if (drv_i_c_gt_zero.any()) {
    /*
     * The number adjustment is done in 3 steps:
     *
     * Step 1: assumes that num_i and num_c are non-negative (nothing to be
     * done here)
     */
    const auto num_i_stp1 = num_i;
    const auto num_c_stp1 = num_c;

    /*
     * Step 2 [Apply relaxed bounds] has 3 parts (a), (b) and (c)
     *
     * Step 2: (a) Apply relaxed bounds to bound num_i and num_c within
     * "relaxed" bounds.
     */
    auto numbnd = min_max_bounded(drv_i, v2nminrl, v2nmaxrl, num_i_stp1);

    /*
     * 2(b) Ideally, num_* should be in range. If they are not, we assume
     * that they will reach their maximum (or minimum)for this mode
     * within a day (time scale). We then compute how much num_* will
     * change in a time step by multiplying the difference between num_*
     * and its maximum(or minimum) with "frac_adj_in_dt".
     */
    const auto delta_num_i_stp2 = (numbnd - num_i_stp1) * frac_adj_in_dt;

    // change in num_i in one time step
    auto num_i_stp2 = num_i_stp1 + delta_num_i_stp2;

    // bounded to relaxed min and max
    numbnd = min_max_bounded(drv_c, v2nminrl, v2nmaxrl, num_c_stp1);
    const auto delta_num_c_stp2 = (numbnd - num_c_stp1) * frac_adj_in_dt;

    // change in num_i in one time step
    auto num_c_stp2 = num_c_stp1 + delta_num_c_stp2;

    /*
     * 2(c) We now also need to balance num_* in case only one among the
     * interstitial or cloud- borne is changing. If interstitial stayed the
     * same (i.e. it is within range) but cloud-borne is predicted to reach
     * its maximum(or minimum), we modify interstitial number (num_i), so as
     * to accommodate change in the cloud-borne aerosols (and vice-versa). We
     * try to balance these by moving the num_* in the opposite direction as
     * much as possible to conserve num_i + num_c (such that num_i+num_c stays
     * close to its original value)
     */
    const auto delta_num_i_stp2_eq0 = delta_num_i_stp2 == 0.0;
    const auto delta_num_c_stp2_eq0 = delta_num_c_stp2 == 0.0;

    num_i_stp2.set(delta_num_i_stp2_eq0 && !delta_num_c_stp2_eq0,
                   min_max_bounded(drv_i, v2nminrl, v2nmaxrl,
                                   num_i_stp1 - delta_num_c_stp2));

    num_c_stp2.set(delta_num_c_stp2_eq0 && !delta_num_i_stp2_eq0,
                   min_max_bounded(drv_c, v2nminrl, v2nmaxrl,
                                   num_c_stp1 - delta_num_i_stp2));

    /*
     * Step 3 [apply stricter bounds] has 3 parts (a), (b) and (c)
     * Step 3:(a) compute combined total of num_i and num_c
     */
    const auto total_drv = drv_i + drv_c;
    const auto total_num = num_i_stp2 + num_c_stp2;

    /*
     * 3(b) We now compute amount of num_* to change if total_num
     *     is out of range. If total_num is within range, we don't do anything
     * (i.e. delta_numa3 and delta_num_c_stp3 remain zero)
     */
    auto delta_num_i_stp3 = Pack(0.0);
    auto delta_num_c_stp3 = Pack(0.0);

    /*
     * "total_drv*v2nmin" represents minimum number for this mode, and
     * "total_drv*v2nmxn" represents maximum number for this mode
     */
    const auto min_number_bound = total_drv * v2nmin;
    const auto max_number_bound = total_drv * v2nmax;

    const auto total_lt_lowerbound = total_num < min_number_bound;
    {
      // change in total_num in one time step
      const auto delta_num_t3 = (min_number_bound - total_num) * frac_adj_in_dt;

      /*
       * Now we need to decide how to distribute "delta_num" (change in
       * number) for num_i and num_c.
       *
       * if both num_i and num_c are less than the lower bound distribute
       * "delta_num" using weighted ratios
       */
      const auto do_dist_delta_num =
          (num_i_stp2 < drv_i * v2nmin) && (num_c_stp2 < drv_c * v2nmin);

      delta_num_i_stp3.set(total_lt_lowerbound && do_dist_delta_num,
                           delta_num_t3 * (num_i_stp2 / total_num));

      delta_num_c_stp3.set(total_lt_lowerbound && do_dist_delta_num,
                           delta_num_t3 * (num_c_stp2 / total_num));

      // if only num_c is less than lower bound, assign total change to num_c
      delta_num_c_stp3.set(total_lt_lowerbound && (num_c_stp2 < drv_c * v2nmin),
                           delta_num_t3);

      // if only num_i is less than lower bound, assign total change to num_i
      delta_num_i_stp3.set(total_lt_lowerbound && (num_i_stp2 < drv_i * v2nmin),
                           delta_num_t3);
    }

    const auto total_gt_upperbound = total_num > max_number_bound;
    {
      // change in total_num in one time step
      const auto delta_num_t3 = (max_number_bound - total_num) * frac_adj_in_dt;

      // decide how to distribute "delta_num"(change in number) for num_i and
      // num_c
      const auto do_dist_delta_num =
          (num_i_stp2 > drv_i * v2nmax) && (num_c_stp2 > drv_c * v2nmax);

      /*
       * if both num_i and num_c are more than the upper bound distribute
       * "delta_num" using weighted ratios
       */
      delta_num_i_stp3.set(total_gt_upperbound && do_dist_delta_num,
                           delta_num_t3 * (num_i_stp2 / total_num));
      delta_num_c_stp3.set(total_gt_upperbound && do_dist_delta_num,
                           delta_num_t3 * (num_c_stp2 / total_num));

      // if only num_c is more than the upper bound, assign total change to
      // num_c
      delta_num_c_stp3.set(total_gt_upperbound && (num_c_stp2 > drv_c * v2nmax),
                           delta_num_t3);

      // if only num_i is more than the upper bound, assign total change to
      // num_i
      delta_num_i_stp3.set(total_gt_upperbound && (num_i_stp2 > drv_i * v2nmax),
                           delta_num_t3);
    }

    // Update num_i/c
    num_i.set(drv_i_c_gt_zero, num_i_stp2 + delta_num_i_stp3);
    num_c.set(drv_i_c_gt_zero, num_c_stp2 + delta_num_c_stp3);
  }

  // Update tendencies
  dqdt = update_num_adj_tends(num_i, init_num_i, dtinv);
  dqqcwdt = update_num_adj_tends(num_c, init_num_c, dtinv);
}

/*
 * \brief Exchange aerosols between aitken and accumulation modes based on new
    sizes.
 */
// @mjs:**HERE**
KOKKOS_INLINE_FUNCTION
void aitken_accum_exchange() // nlevs, top_lev, &
                             // aitken_idx,  accum_idx, adj_tscale_inv, &
                             // dt, q_i, q_c, n_i, n_c, &
                             // drv_a_aitsv, num_a_aitsv, drv_c_aitsv,
                             // num_c_aitsv,     & drv_a_accsv,num_a_accsv,
                             // drv_c_accsv, num_c_accsv,      & dgncur_a,
                             // v2ncur_a, dgncur_c, v2ncur_c, & didt, dcdt,
                             // dnidt, dncdt)
// NOTE: skipping the existence checks and index verification for now

{
  // compute geometric mean of v2n's for aitken and accumulation modes
  // auto v2n_geo_mean = sqrt(v2n);


}

// aitken_accum_exchange

} // namespace calcsize

/// @class CalcSize
/// This class implements MAM4's CalcSize parameterization.
class CalcSize {
public:
  // nucleation-specific configuration
  struct Config {

    // default constructor -- sets default values for parameters
    Config() {}

    Config(const Config &) = default;
    ~Config() = default;
    Config &operator=(const Config &) = default;
  };

private:
  Config config_;

  Real v2nmin_nmodes[4], v2nmax_nmodes[4], v2nnom_nmodes[4];
  // v2nnom_nmodes[4];
  // Mode parameters
  Real dgnnom_nmodes[4], // mean geometric number diameter
      dgnmax_nmodes[4],  // max geometric number diameter
      dgnmin_nmodes[4];  // min geometric number diameter

  // There is a common factor calculated over and over in the core loop of this
  // process. This factor has been pulled out so the calculation only has to be
  // performed once.
  Real common_factor_nmodes[4];

  Real _inv_density[4][7];

public:
  // name -- unique name of the process implemented by this class
  const char *name() const { return "MAM4 calcsize"; }

  // init -- initializes the implementation with MAM4's configuration and with
  // a process-specific configuration.
  void init(const AeroConfig &aero_config,
            const Config &calsize_config = Config()) {
    // Set nucleation-specific config parameters.
    config_ = calsize_config;
  
    // Set mode parameters.
    for (int m = 0; m < 4; ++m) {
      // FIXME: There is no mean geometric number diameter in a mode.
      // FIXME: Assume "nominal" diameter for now?
      // FIXME: There is a comment in modal_aero_newnuc.F90 that Dick Easter
      // FIXME: thinks that dgnum_aer isn't used in MAM4, but it is actually
      // FIXME: used in this nucleation parameterization. So we will have to
      // FIXME: figure this out.
      dgnnom_nmodes[m] = modes(m).nom_diameter;
      dgnmin_nmodes[m] = modes(m).min_diameter;
      dgnmax_nmodes[m] = modes(m).max_diameter;
      common_factor_nmodes[m] =
          exp(4.5 * log(modes(m).mean_std_dev) * log(modes(m).mean_std_dev)) *
          Constants::pi_sixth; // A common factor
      v2nnom_nmodes[m] =
          1.0 / (common_factor_nmodes[m] * pow(dgnnom_nmodes[m], 3.0));
      v2nmin_nmodes[m] =
          1.0 / (common_factor_nmodes[m] * pow(dgnmax_nmodes[m], 3.0));
      v2nmax_nmodes[m] =
          1.0 / (common_factor_nmodes[m] * pow(dgnmin_nmodes[m], 3.0));
      // min_vol2num
      // = 1.0_wp/(pi_sixth*(imode%max_diameter**3.0_wp)*exp(4.5_wp*(log(imode%mean_std_dev))**2.0_wp))

      // compute inv density; density is constant, so we can computer in int. 
      const auto n_spec = num_species_mode[m];
      for (int ispec = 0; ispec < n_spec; ispec++) {
        int aero_id = int(mode_aero_species[m][ispec]);
        _inv_density[m][ispec] = Real(1.0) / aero_species(aero_id).density;
      } // for(ispec)

    } // for(m)

  } // end(int)

  KOKKOS_INLINE_FUNCTION
  void compute_tendencies(const AeroConfig &config, const ThreadTeam &team,
                          Real t, Real dt, const Atmosphere &atmosphere,
                          const Prognostics &prognostics,
                          const Diagnostics &diagnostics,
                          const Tendencies &tendencies) const {

    const int nlevels = diagnostics.num_levels();

    static constexpr std::size_t num_levels_upper_bound = 128;
    // See declaration of num_levels_upper_bound for its documentation
    EKAT_KERNEL_ASSERT(nlevels <= num_levels_upper_bound);
    static constexpr bool do_aitacc_transfer = true;
    static constexpr bool do_adjust = true;

    const int nk = PackInfo::num_packs(atmosphere.num_levels());
    const int aitken_idx = int(ModeIndex::Aitken);
    const int accumulation_idx = int(ModeIndex::Accumulation);
    const int nmodes = AeroConfig::num_modes();
    auto &dgncur_i = diagnostics.dgncur_i;
    auto &v2ncur_i = diagnostics.v2ncur_i;
    auto &dgncur_c = diagnostics.dgncur_c;
    auto &v2ncur_c = diagnostics.v2ncur_c;
    const auto inv_density = _inv_density; 

    Kokkos::parallel_for(
        Kokkos::TeamThreadRange(team, nk), KOKKOS_CLASS_LAMBDA(int k) {
          // Oscar is working on this: start
          const auto n_i = prognostics.n_mode_i;
          const auto n_c = prognostics.n_mode_c;

          // tendencies for interstitial number mixing ratios
          const auto dnidt = tendencies.n_mode_i;

          // tendencies for cloud-borne number mixing ratios
          const auto dncdt = tendencies.n_mode_c;

          Pack dryvol_i = 0;
          Pack dryvol_c = 0;
          for (int imode = 0; imode < nmodes; imode++) {

            // FIXME: as compared to the oldHaero_fortranPort.f90, we appear to
            // be missing this Initialize diameter(dgnum), volume to number
            // ratios(v2ncur) and dry volume (dryvol) for both interstitial and
            // cloudborne aerosols
            // DONE

            // call set_initial_sz_and_volumes (imode, top_lev, nlevs, dgncur_a,
            // v2ncur_a, dryvol_a) for interstitial aerosols call
            // set_initial_sz_and_volumes (imode, top_lev, nlevs, dgncur_c,
            // v2ncur_c, dryvol_c) for cloud-borne aerosols

            // ----------------------------------------------------------------------
            // Algorithm to compute dry aerosol diameter:
            // calculate aerosol diameter volume, volume is computed from mass
            // and density
            // ----------------------------------------------------------------------

            // find start and end index of species in this mode in the
            // "population" array The indices are same for interstitial and
            // cloudborne species s_spec_ind = population_offsets(imode) start
            // index e_spec_ind = population_offsets(imode+1) - 1 end index of
            // species for all (modes expect the last mode)

            // if(imode.eq.nmodes) then  for last mode
            //    e_spec_ind = num_populations if imode==nmodes, end index is
            //    the total number of species
            // endif

            // nspec = num_mode_species(imode) total number of species in mode
            // "imode"

            // capture densities for each specie in this mode
            // density(1:max_nspec) = huge(density) initialize the whole array
            // to a huge value [FIXME: NaN would be better than huge]
            // density(1:nspec) = spec_density(imode, 1:nspec) assign density
            // till nspec (as nspec can be different for each mode)

            // Initialize diameter(dgnum), volume to number ratios(v2ncur) and
            // dry volume (dryvol) for both interstitial and cloudborne
            // aerosols we did not implement set_initial_sz_and_volumes
            dgncur_i[imode](k) = dgnnom_nmodes[imode]; // diameter [m]
            v2ncur_i[imode](k) = v2nnom_nmodes[imode]; // volume to number

            dgncur_c[imode](k) = dgnnom_nmodes[imode]; // diameter [m]
            v2ncur_c[imode](k) = v2nnom_nmodes[imode]; // volume to number

            // dry volume is set to zero inside compute_dry_volume_k
            calcsize::compute_dry_volume_k(k, imode, inv_density,  prognostics, dryvol_i,
                                           dryvol_c);

            auto v2nmin = v2nmin_nmodes[imode];
            auto v2nmax = v2nmax_nmodes[imode];
            const auto dgnmin = dgnmin_nmodes[imode];
            const auto dgnmax = dgnmax_nmodes[imode];
            const auto common_factor = common_factor_nmodes[imode];

            Real v2nminrl, v2nmaxrl;

            // compute upper and lower limits for volume to num (v2n) ratios and
            // diameters (dgn)
            //      Get relaxed limits for volume_to_num
            // (we use relaxed limits for aerosol number "adjustment"
            // calculations via "adjust_num_sizes" subroutine. Note: The
            // relaxed limits will be artificially inflated (or deflated) for
            // the aitken and accumulation modes if "do_aitacc_transfer" flag is
            // true to effectively shut-off aerosol number "adjustment"
            // calculations for these modes because we do the explicit transfer
            // (via "aitken_accum_exchange" subroutine) from one mode to
            // another instead of adjustments for these modes)

            calcsize::get_relaxed_v2n_limits(
                do_aitacc_transfer, imode == aitken_idx,
                imode == accumulation_idx, v2nmin, v2nmax, v2nminrl,
                v2nmaxrl); // outputs (NOTE: v2nmin and v2nmax are only updated
                           // for aitken and accumulation modes)

            // initial value of num interstitial for this pack and mode
            auto init_num_i = n_i[imode](k);

            // `adjust_num_sizes` will use the initial value, but other
            // calculations require this to be nonzero.
            // Make it non-negative
            auto num_i_k = Pack(init_num_i < 0, Pack(0.0), init_num_i);

            auto init_num_c = n_c[imode](k);
            // Make it non-negative
            auto num_c_k = Pack(init_num_c < 0, Pack(0.0), init_num_c);

            static constexpr Real close_to_one = 1.0 + 1.0e-15;
            static constexpr Real seconds_in_a_day = 86400.0;

            // these quantities are required for adjust_num_sizes() and
            // aitken_accum_exchange() [within, specifically
            // compute_coef_ait_acc_transfer() and
            // compute_coef_acc_ait_transfer()]
            // time scale for number adjustment
            const auto adj_tscale = max(seconds_in_a_day, dt);

            // inverse of the adjustment time scale
            const auto adj_tscale_inv = 1.0 / (adj_tscale * close_to_one);

            if (do_adjust) {
              /*------------------------------------------------------------------
               *  Do number adjustment for interstitial and activated particles
               *------------------------------------------------------------------
               * Adjustments that are applied over time-scale deltat
               * (model time step in seconds):
               *
               *   1. make numbers non-negative or
               *   2. make numbers zero when volume is zero
               *
               *
               * Adjustments that are applied over time-scale of a day (in
               *seconds)
               *   3. bring numbers to within specified bounds
               *
               * (Adjustment details are explained in the process)
               *------------------------------------------------------------------*/

              // number tendencies to be updated by adjust_num_sizes subroutine

              auto &interstitial_tend = dnidt[imode](k);
              auto &cloudborne_tend = dncdt[imode](k);

              /*NOTE: Only number tendencies (NOT mass mixing ratios) are
               updated in adjust_num_sizes Effect of these adjustment will be
               reflected in the particle diameters (via
               "update_diameter_and_vol2num" subroutine call below) */
              calcsize::adjust_num_sizes(
                  dryvol_i, dryvol_c, init_num_i, init_num_c, dt,     // in
                  v2nmin, v2nmax, v2nminrl, v2nmaxrl, adj_tscale_inv, // in
                  close_to_one,                                       // in
                  num_i_k, num_c_k,                                   // out
                  interstitial_tend, cloudborne_tend);                // out
            }

            // update diameters and volume to num ratios for interstitial
            // aerosols
            auto &dgncur_i_k = dgncur_i[imode](k);
            auto &v2ncur_i_k = v2ncur_i[imode](k);

            calcsize::update_diameter_and_vol2num(
                dryvol_i, num_i_k, v2nmin, v2nmax, dgnmin, dgnmax,
                common_factor, dgncur_i_k, v2ncur_i_k);

            // update diameters and volume to num ratios for cloudborne aerosols
            auto &dgncur_c_k = dgncur_c[imode](k);
            auto &v2ncur_c_k = v2ncur_c[imode](k);
            calcsize::update_diameter_and_vol2num(
                dryvol_c, num_c_k, v2nmin, v2nmax, dgnmin, dgnmax,
                common_factor, dgncur_c_k, v2ncur_c_k);

            // save number concentrations and dry volumes for explicit
            // aitken <--> accum mode transfer, which is the next step in
            // the calcSize process
            if (do_aitacc_transfer) {
              if (imode == aitken_idx) {
                // TODO: determine if we need to save these--i.e., is drv_i ever
                // changed before the max() calculation in
                // aitken_accum_exchange() if yet, maybe better to skip the
                // logic and do it, regardless?
                const auto sdryvol_i_ait = dryvol_i;
                const auto snum_i_k_ait = num_i_k;
                const auto sdryvol_c_ait = dryvol_c;
                const auto snum_c_k_ait = num_c_k;
              } else if (imode == accumulation_idx) {
                const auto sdryvol_i_acc = dryvol_i;
                const auto snum_i_k_acc = num_i_k;
                const auto sdryvol_c_acc = dryvol_c;
                const auto snum_c_k_acc = num_c_k;
              }
            }
          } // for(imode)

          // ------------------------------------------------------------------
          //  Overall logic for aitken<-->accumulation transfer:
          //  ------------------------------------------------
          //  when the aitken mode mean size is too big, the largest
          //     aitken particles are transferred into the accum mode
          //     to reduce the aitken mode mean size
          //  when the accum mode mean size is too small, the smallest
          //     accum particles are transferred into the aitken mode
          //     to increase the accum mode mean size
          // ------------------------------------------------------------------

          if (do_aitacc_transfer) {
            calcsize::aitken_accum_exchange();
            // nlevs, top_lev, &aitken_idx, accum_idx, adj_tscale_inv, &dt,
            // q_i, q_c, n_i, n_c, &sdrv_a_ait, snum_a_ait, sdrv_c_ait,
            // snum_c_ait, &sdrv_a_acc, snum_a_acc, sdrv_c_acc, snum_c_acc,
            // &dgncur_a, v2ncur_a, dgncur_c, v2ncur_c, &didt, dcdt, dnidt,
            // dncdt)
          }

          // if ( do_aitacc_transfer ) then
          //    call aitken_accum_exchange( nlevs, top_lev, &
          //         aitken_idx,  accum_idx, adj_tscale_inv, &
          //         dt, q_i, q_c, n_i, n_c,&
          //         sdrv_a_ait, snum_a_ait, sdrv_c_ait, snum_c_ait,     &
          //         sdrv_a_acc,snum_a_acc, sdrv_c_acc, snum_c_acc,      &
          //         dgncur_a, v2ncur_a, dgncur_c, v2ncur_c, &
          //         didt, dcdt, dnidt, dncdt )
          // end if
        }); // kokkos::parfor(k)

    // This is from haero fortran port:
    // TODO: after aitken<->accum transfer, the rest of the code deals
    // with history field output and updating the tendencies
  }

private:
};

} // namespace mam4

#endif
