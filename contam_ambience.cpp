/*! @file contam_ambience.cpp
 * \brief Contam ambience data struct and manager
 */

#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include "types.h" // Contam types: IX = int32_t, R8 = double
#include "contam_ambience.h"
#include <cstdlib> // abort

namespace Contam_gRPC {

std::string AmbienceSample::to_string() const
{
  using std::to_string;
  return " {" + to_string(time) + ", "
              + to_string(temperature) + ", "
              + to_string(pressure) + ", "
              + to_string(wind_speed) + ", "
              + to_string(wind_direction) + ", "
              + to_string(mass_fraction) + "}";
}

AmbienceData::AmbienceData()
: m_initialized(false), m_been_found(false)
{}

std::vector<AmbienceSample>& AmbienceData::data()
{
  if (m_initialized) {
    abort();
    //return {};
  }
  return m_data;
}

const std::vector<AmbienceSample>& AmbienceData::data() const
{
  return m_data;
}

void AmbienceData::init()
{
  m_initialized = true;
  std::sort(m_data.begin(), m_data.end());
  m_curr = m_data.begin();
}

void AmbienceData::insert(const AmbienceSample& s)
{
  if (m_initialized) {
    return;
  }
  m_data.emplace_back(s);
}

void AmbienceData::insert(AmbienceSample&& s)
{
  if (m_initialized) {
    return;
  }
  m_data.emplace_back(std::move(s));
}

std::tuple<AmbienceSample, bool, bool> AmbienceData::get_sample(const IX t)
{
  const IX t_curr = m_curr->time;
  if ((m_data.size() == 0ul) || (t < t_curr)) { // also !m_initialized
    return {AmbienceSample{}, false, false};
  }

  if ((t_curr == m_data.crbegin()->time) || (t < (m_curr+1)->time)) {
    if (!m_been_found) {
      // First time a sample has been found. Any previous call may have resulted in
      // (t < m_data.cbegin()->time)
      m_been_found = true;
      return {*m_curr, true, true};
    }
    return {*m_curr, true, false};
  }

  m_curr = std::upper_bound(m_curr+1, m_data.cend(), t);

  m_been_found = true;
  return {*(--m_curr), true, true};
}

} // end of namespace
