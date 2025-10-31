/*! @file contam_ambience.cpp
 * \brief Contam ambience data struct and manager
 */

#include "contam_ambience.h"

#include <algorithm>
#include <cstdlib>  // abort
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace Contam_gRPC {

AmbienceSample::AmbienceSample()
  : time(0),
    temperature(default_temperature),
    pressure(default_pressure),
    wind_speed(default_wind_speed),
    wind_direction(default_wind_direction),
    mass_fraction {}
{}

AmbienceSample::~AmbienceSample()
{
  mass_fraction.clear();
}

AmbienceSample::AmbienceSample(CTM_IX t,
                               CTM_R8 temp,
                               CTM_R8 pres,
                               CTM_R8 wspd,
                               CTM_R8 wdir,
                               std::vector<CTM_R8> const& mf)
  : time(t),
    temperature(temp),
    pressure(pres),
    wind_speed(wspd),
    wind_direction(wdir),
    mass_fraction(mf)
{}

AmbienceSample::AmbienceSample(
  CTM_IX t, CTM_R8 temp, CTM_R8 pres, CTM_R8 wspd, CTM_R8 wdir, CTM_R8 mf)
  : time(t),
    temperature(temp),
    pressure(pres),
    wind_speed(wspd),
    wind_direction(wdir),
    mass_fraction {mf}
{}

std::string AmbienceSample::to_string() const
{
  using std::string;
  using std::to_string;
  string str = "{time: " + to_string(time) + ", temperature: "
               + to_string(temperature) + ", pressure: " + to_string(pressure)
               + ", wind_speed: " + to_string(wind_speed) + ", wind_direction: "
               + to_string(wind_direction) + ", mass_fraction: [";
  for (auto const& mf : mass_fraction) {
    str += to_string(mf);
  }
  str += "}";
  return str;
}

AmbienceData::AmbienceData() : m_initialized(false), m_been_found(false)
{}

std::vector<AmbienceSample>& AmbienceData::data()
{
  if (m_initialized) {
    abort();
    // return {};
  }
  return m_data;
}

std::vector<AmbienceSample> const& AmbienceData::data() const
{
  return m_data;
}

bool AmbienceData::init()
{
  m_initialized = true;
  if (m_data.empty()) {
    std::cerr << "No ambience data\n";
    return true;
  }
  std::sort(m_data.begin(), m_data.end());
  m_curr = m_data.begin();
  auto t = m_data.begin()->time - 1;
  for (auto const& s : m_data) {
    if (t == s.time) {
      std::cerr << "Detected duplicate time points in ambience time series!\n";
      return false;
    }
  }
  return true;
}

bool AmbienceData::insert(AmbienceSample const& s)
{
  // After being finalized, no more sample can be added.
  if (m_initialized) {
    return false;
  }
  // Check number of contaminatns represetend in mass fraction per sample is the
  // same
  if (!m_data.empty()
      && (m_data.begin()->mass_fraction.size() != s.mass_fraction.size())) {
    return false;
  }
  m_data.emplace_back(s);
  return true;
}

bool AmbienceData::insert(AmbienceSample&& s)
{
  // After being finalized, no more sample can be added.
  if (m_initialized) {
    return false;
  }
  // Check number of contaminatns represetend in mass fraction per sample is the
  // same
  if (!m_data.empty()
      && (m_data.begin()->mass_fraction.size() != s.mass_fraction.size())) {
    return false;
  }
  m_data.emplace_back(std::move(s));
  return true;
}

std::tuple<AmbienceSample, bool, bool> AmbienceData::get_sample(const CTM_IX t)
{
  const CTM_IX t_curr = m_curr->time;
  if ((m_data.size() == 0ul) || (t < t_curr)) {  // also !m_initialized
    return {AmbienceSample {}, false, false};
  }

  if ((t_curr == m_data.crbegin()->time) || (t < (m_curr + 1)->time)) {
    if (!m_been_found) {
      // First time a sample has been found. Any previous call may have resulted
      // in (t < m_data.cbegin()->time)
      m_been_found = true;
      return {*m_curr, true, true};
    }
    return {*m_curr, true, false};
  }

  m_curr = std::upper_bound(m_curr + 1, m_data.cend(), t);

  m_been_found = true;
  return {*(--m_curr), true, true};
}

std::string AmbienceData::to_string(unsigned indent) const
{
  const std::string str_indent = std::string(indent, ' ');
  const std::string str_indent_inner = std::string(indent + 2, ' ');
  std::string str = str_indent + "[\n";
  for (const auto& as : m_data) {
    str += str_indent_inner + as.to_string() + ",\n";
  }
  str += str_indent + ']';
  return str;
}

}  // namespace Contam_gRPC
