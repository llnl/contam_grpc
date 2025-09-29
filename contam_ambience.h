/*! @file contam_ambience.h
 * \brief Contam ambience data struct and manager
 */

#ifndef CONTAM_GRPC_CONTAM_AMBIENCE_H
#define CONTAM_GRPC_CONTAM_AMBIENCE_H

#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#if !defined(CLIENT_ONLY) || (CLIENT_ONLY == 0)
#include "types.h" // Contam types: IX = int32_t, R8 = double
#endif

namespace Contam_gRPC {

#ifdef TYPES_H // contamx types.h is included
using CTM_IX = IX;
using CTM_R8 = R8;
#else
using CTM_IX = int32_t;
using CTM_R8 = double;
#endif

/*! \brief Contam ambience sample at a specific time */
struct AmbienceSample {
  CTM_IX time; ///< in second
  CTM_R8 temperature; ///< in K
  CTM_R8 pressure; ///< barometric pressure in Pa
  CTM_R8 wind_speed; ///< in m/s
  CTM_R8 wind_direction; ///< in degree
  CTM_R8 mass_fraction; ///< in kgc/kga

  AmbienceSample();
  AmbienceSample(CTM_IX time, CTM_R8 temp, CTM_R8 pres, CTM_R8 wind_spd, CTM_R8 wind_dir, CTM_R8 mf);
  AmbienceSample(const AmbienceSample& other) = default;
  AmbienceSample(AmbienceSample&& other) = default;
  AmbienceSample& operator=(const AmbienceSample& other) = default;
  AmbienceSample& operator=(AmbienceSample&& other) = default;

  bool operator<(const AmbienceSample& other) const {
    return (time < other.time);
  }

  bool operator<(const CTM_IX t) const {
    return (time < t);
  }

  std::string to_string() const;
};

inline bool operator<(const CTM_IX t, const AmbienceSample& s) {
  return (t < s.time);
}

/*! \brief Contam ambience data over time */
class AmbienceData {
  protected:
    bool m_initialized; ///< Tell if already initialized
    bool m_been_found; ///< sample has been found at least once
    std::vector<AmbienceSample>::const_iterator m_curr; ///< Current index
    std::vector<AmbienceSample> m_data; ///< Samples ordered by the time

  public:
    AmbienceData();
    /// Allow direct access to data after initialized
    std::vector<AmbienceSample>& data();
    /// Allow direct read-only access to data
    const std::vector<AmbienceSample>& data() const;

    void init();
    bool is_initialized() const { return m_initialized; }
    /// Add an ambience sample before initialized
    void insert(const AmbienceSample& s);
    /// Add an ambience sample before initialized
    void insert(AmbienceSample&& s);

    /** Return the last known abience sample earlier than or equal to t.
        Also, indicate where a sample was found and if a new sample was found */
    std::tuple<AmbienceSample, bool, bool> get_sample(const CTM_IX t);
};

} // end of namespace
#endif
