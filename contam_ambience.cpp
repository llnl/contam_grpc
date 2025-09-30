/*! @file contam_ambience.cpp
 * \brief Contam ambience data struct and manager
 */

#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include <cstdlib> // abort
#include <iostream>
#include "contam_ambience.h"

namespace Contam_gRPC {

AmbienceSample::AmbienceSample()
  : time(0), temperature(0.0), pressure(0.0), wind_speed(0.0),
    wind_direction(0.0), mass_fraction(0.0) {}

AmbienceSample::AmbienceSample(CTM_IX t, CTM_R8 temp, CTM_R8 pres, CTM_R8 wspd, CTM_R8 wdir, CTM_R8 mf)
  : time(t),
    temperature(temp),
    pressure(pres),
    wind_speed(wspd),
    wind_direction(wdir),
    mass_fraction(mf)
{}

std::string AmbienceSample::to_string() const
{
  using std::to_string;
  return
    std::string("{time: ") + to_string(time) +
    ", temperature: " + to_string(temperature) +
    ", pressure: " + to_string(pressure) +
    ", wind_speed: " + to_string(wind_speed) +
    ", wind_direction: " + to_string(wind_direction) +
    ", mass_fraction: " + to_string(mass_fraction) + "}";
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

bool AmbienceData::init()
{
  m_initialized = true;
  if (m_data.empty()) { // TODO: should this return true?
    std::cerr << "No ambience data\n";
    return false;
  }
  std::sort(m_data.begin(), m_data.end());
  m_curr = m_data.begin();
  auto t = m_data.begin()->time - 1;
  for (const auto& s: m_data) {
    if (t == s.time) {
      std::cerr << "Detected duplicate time points in ambience time series!\n";
      return false;
    }
  }
  return true;
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

std::tuple<AmbienceSample, bool, bool> AmbienceData::get_sample(const CTM_IX t)
{
  const CTM_IX t_curr = m_curr->time;
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
