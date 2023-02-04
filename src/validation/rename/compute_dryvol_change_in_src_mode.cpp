#include <mam4xx/mam4.hpp>

#include <mam4xx/aero_config.hpp>
#include <mam4xx/rename.hpp>
#include <skywalker.hpp>
#include <validation.hpp>

using namespace skywalker;
using namespace mam4;

void compute_dryvol_change_in_src_mode(Ensemble *ensemble) {

  ensemble->process([=](const Input &input, Output &output) {
    const int nmodes = AeroConfig::num_modes();
    const int naerosol_species = AeroConfig::num_aerosol_ids();

    const Real zero =0;

    // int dest_mode_of_mode = input.get_array("dest_mode_of_mode");
    int dest_mode_of_mode[nmodes] = {0, 1, 0, 0};

    auto set_mode_aerosol_values = [](const std::vector<Real> &vector_in,
                                      Real values[nmodes][naerosol_species]) {
      int count = 0;
      for (int m = 0; m < nmodes; ++m) {
        for (int ispec = 0; ispec < naerosol_species; ++ispec) {
          values[m][ispec] = vector_in[count];
          count++;
        }
      }
    };

    auto q_mmr_vector = input.get_array("q_mmr");
    Real q_mmr[nmodes][naerosol_species];
    set_mode_aerosol_values(q_mmr_vector, q_mmr );

    auto q_del_growth_vector = input.get_array("q_del_growth");
    Real q_del_growth[nmodes][naerosol_species];
    set_mode_aerosol_values(q_del_growth_vector, q_del_growth );

    Real sz_factor[nmodes];
    Real fmode_dist_tail_fac[nmodes];
    Real v2n_lo_rlx[nmodes];
    Real v2n_hi_rlx[nmodes];
    Real ln_diameter_tail_fac[nmodes];
    int num_pairs = 0;
    Real diameter_cutoff[nmodes];
    Real ln_dia_cutoff[nmodes];
    Real diameter_threshold[nmodes];
    Real mass_2_vol[naerosol_species];

    rename::find_renaming_pairs(dest_mode_of_mode,    // in
                                sz_factor,            // out
                                fmode_dist_tail_fac,  // out
                                v2n_lo_rlx,           // out
                                v2n_hi_rlx,           // out
                                ln_diameter_tail_fac, // out
                                num_pairs,            // out
                                diameter_cutoff,      // out
                                ln_dia_cutoff,
                                diameter_threshold);

    //
    Real molecular_weight_rename[naerosol_species] = {
        150, 115, 150, 12, 58.5, 135, 250092}; // [kg/kmol]
    for (int iaero = 0; iaero < naerosol_species; ++iaero) {
      mass_2_vol[iaero] = molecular_weight_rename[iaero] / aero_species(iaero).density;
    }

    Real dryvol[4] = {zero};
    Real deldryvol[4] = {zero}; 

    rename::compute_dryvol_change_in_src_mode(
    nmodes,              // in
    naerosol_species,              // in
    dest_mode_of_mode, // in
    q_mmr, // in
    q_del_growth, // in
    mass_2_vol,   // in
    dryvol,
    deldryvol);


    auto save_mode_values = [](const Real values[nmodes],
                               std::vector<Real> &values_vector) {
      for (int i = 0; i < nmodes; ++i)
        values_vector[i] = values[i];
    };

    std::vector<Real> dryvol_out(nmodes, 0);
    save_mode_values(dryvol, dryvol_out);
    output.set("dryvol", dryvol_out);

    std::vector<Real> deldryvol_out(nmodes, 0);
    save_mode_values(deldryvol, deldryvol_out);
    output.set("deldryvol", deldryvol_out);                                

                                
  });
}