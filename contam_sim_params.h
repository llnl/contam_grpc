/*! @file contam_sim_params.h
 * \brief Contam simulation and client parameters
 */

#ifndef CONTAM_GRPC_CONTAM_SIM_PARAMS_H
#define CONTAM_GRPC_CONTAM_SIM_PARAMS_H

#include <string>
#include <tuple>
#include <vector>

namespace Contam_gRPC {

struct AmbienceSample {
  int time;
  double temperature;
  double pressure;
  double wind_speed;
  double wind_direction;
  double mass_fraction;

  std::string to_string() const;
};

struct ContamParams {
  std::string prj_filename; ///< Contam project filename
  std::vector<AmbienceSample> amb_series; ///< Ambience timeseries

  std::string to_string() const;
};

struct ClientParams {
  bool of_out; ///< Whether to write stdout from Contam server into a file
  bool of_err; ///< Whether to write stderr from Contam server into a file
  bool of_ach; ///< Whether to retrieve .ach output file from Contam server
  bool of_cex; ///< Whether to retrieve .cex output file from Contam server
  bool of_csm; ///< Whether to retrieve .csm output file from Contam server
  bool of_log; ///< Whether to retrieve .log output file from Contam server
  bool of_rst; ///< Whether to retrieve .rst output file from Contam server
  bool of_sim; ///< Whether to retrieve .sim output file from Contam server
  bool of_xlog; ///< Whether to retrieve .xlog output file from Contam server
  std::string of_prefix; ///< prefix path to the contam output files
  std::string server_addr; ///< hostname:port of the server

  std::string to_string() const;
};

std::tuple<ContamParams, ClientParams>
read_proto_params(const std::string& filename, bool verbose);

} // end of namespace

#endif // CONTAM_GRPC_CONTAM_SIM_PARAMS_H
