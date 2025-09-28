/*! @file contam_ambience.h
 * \brief Contam ambience data struct and manager
 */

#ifndef CONTAM_GRPC_CONTAM_AMBIENCE_H
#define CONTAM_GRPC_CONTAM_AMBIENCE_H

#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include "types.h" // Contam types: IX = int32_t, R8 = double

namespace Contam_gRPC {

/*! \brief Contam ambience sample at a specific time */
struct AmbienceSample {
  IX time; ///< in second
  R8 temperature; ///< in K
  R8 pressure; ///< barometric pressure in Pa
  R8 wind_speed; ///< in m/s
  R8 wind_direction; ///< in degree
  R8 mass_fraction; ///< in kgc/kga

  AmbienceSample()
    : time(0), temperature(0.0), pressure(0.0), wind_speed(0.0),
      wind_direction(0.0), mass_fraction(0.0) {}

  AmbienceSample(IX time, R8 temp, R8 pres, R8 wind_spd, R8 wind_dir, R8 mf)
    : time(time), temperature(temp), pressure(pres), wind_speed(wind_spd),
      wind_direction(wind_dir), mass_fraction(mf) {}

  bool operator<(const AmbienceSample& other) const {
    return (time < other.time);
  }

  bool operator<(const IX t) const {
    return (time < t);
  }

  std::string to_string() const;
};

inline bool operator<(const IX t, const AmbienceSample& s) {
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
    std::tuple<AmbienceSample, bool, bool> get_sample(const IX t);
};

} // end of namespace
#endif
