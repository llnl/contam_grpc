/*
 * Copyright 2026 Lawrence Livermore National Security, LLC.
 * See the top-level LICENSE file for details.

 * SPDX-License-Identifier: BSD-3
 */

/*! @file contam_ambience.h
 * \brief Contam ambience data struct and manager
 */

#ifndef CONTAM_GRPC_CONTAM_AMBIENCE_H
#define CONTAM_GRPC_CONTAM_AMBIENCE_H

#include <algorithm>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#if !defined(CLIENT_ONLY) || (CLIENT_ONLY == 0)
#include "types.h"  // Contam types: IX = int32_t, R8 = double
#endif

namespace Contam_gRPC {

#ifdef TYPES_H  // contamx types.h is included
using CTM_IX = IX;
using CTM_R8 = R8;
#else
using CTM_IX = int32_t;
using CTM_R8 = double;
#endif

/**
 * \brief Contam ambience sample at a specific time
 *
 * The altitude of the building site will be used to determine a
 * default barometric pressure.
 * Other default conditions are no wind, 20 °C and a barometric
 * pressure of 1 atmosphere (approximately 101 kPa).
 */

struct AmbienceSample {
  static constexpr CTM_R8 default_temperature =
    293.15;  // 20 degree celsius in Kelvin
  static constexpr CTM_R8 default_pressure =
    101000;  // Barometric pressure in Pa
  static constexpr CTM_R8 default_wind_speed = 0.0;
  static constexpr CTM_R8 default_wind_direction = 0.0;

  CTM_IX time;            ///< in second
  CTM_R8 temperature;     ///< in K
  CTM_R8 pressure;        ///< barometric pressure in Pa
  CTM_R8 wind_speed;      ///< in m/s
  CTM_R8 wind_direction;  ///< in degree
  // TODO: change the vector type to array
  std::vector<CTM_R8> mass_fraction;  ///< in kgc/kga

  AmbienceSample();
  ~AmbienceSample();
  AmbienceSample(CTM_IX t,
                 CTM_R8 temp,
                 CTM_R8 pres,
                 CTM_R8 wspd,
                 CTM_R8 wdir,
                 std::vector<CTM_R8> const& mf);
  AmbienceSample(
    CTM_IX t, CTM_R8 temp, CTM_R8 pres, CTM_R8 wspd, CTM_R8 wdir, CTM_R8 mf);

  // For envelope wind pressure and contaminant mass fractions
  AmbienceSample(CTM_IX t, CTM_R8 pres, std::vector<CTM_R8> const& mf);
  // For envelope wind pressure and contaminant mass fractions
  AmbienceSample(CTM_IX t, CTM_R8 pres, CTM_R8 mf);

  AmbienceSample(AmbienceSample const& other) = default;
  AmbienceSample(AmbienceSample&& other) = default;
  AmbienceSample& operator=(AmbienceSample const& other) = default;
  AmbienceSample& operator=(AmbienceSample&& other) = default;

  bool operator<(AmbienceSample const& other) const
  {
    return (time < other.time);
  }

  bool operator<(const CTM_IX t) const { return (time < t); }

  std::string to_string() const;
};

inline bool operator<(const CTM_IX t, AmbienceSample const& s)
{
  return (t < s.time);
}

/*! \brief Contam ambience data over time */
class AmbienceData {
protected:
  bool m_initialized;  ///< Tell if already initialized (i.e., finalized)
  bool m_been_found;   ///< sample has been found at least once
  std::vector<AmbienceSample>::const_iterator m_curr;  ///< Current index
  std::vector<AmbienceSample> m_data;  ///< Samples ordered by the time

public:
  AmbienceData();
  /// Allow direct access to data after initialized
  std::vector<AmbienceSample>& data();
  /// Allow direct read-only access to data
  std::vector<AmbienceSample> const& data() const;

  bool init();
  bool is_initialized() const { return m_initialized; }
  /// Add an ambience sample before initialized
  bool insert(AmbienceSample const& s);
  /// Add an ambience sample before initialized
  bool insert(AmbienceSample&& s);

  /** Return the last known abience sample earlier than or equal to t.
      Also, indicate where a sample was found and if a new sample was found */
  std::tuple<AmbienceSample, bool, bool> get_sample(const CTM_IX t);

  std::string to_string(unsigned indent = 0u) const;
};

std::string to_string(std::vector<AmbienceSample> const& data,
                      unsigned indent = 0u);

}  // namespace Contam_gRPC
#endif
