#ifndef MAM4XX_HPP
#define MAM4XX_HPP

// This header makes all MAM4 processes available.

#include <haero/aero_process.hpp>

#include "gasaerexch.hpp"
#include "nucleation.hpp"

namespace mam4 {

using Real = haero::Real;

using ColumnView = haero::ColumnView;

using NucleationProcess = haero::AeroProcess<AeroConfig, Nucleation>;
using GasAerExchProcess = haero::AeroProcess<AeroConfig, GasAerExch>;

}  // namespace mam4

#endif