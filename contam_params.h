/*! @file contam_params.h
 * \brief Contam simulation and client parameters
 */

#ifndef CONTAM_GRPC_CONTAM_PARAMS_H
#define CONTAM_GRPC_CONTAM_PARAMS_H

#include <string>
#include <tuple>
#include <vector>

#include "contam_ambience.h"

namespace Contam_gRPC {

struct ContamParams {
  std::string prj_filename;  ///< Contam project filename
  unsigned wp_wo_envelope;   ///< Simulation mode regarding wind pressure
  std::vector<std::vector<AmbienceSample>>
    ambiences;  ///< Set of ambience timeseries

  void clear();
  std::string to_string(unsigned indent = 0u) const;
};

struct ClientParams {
  bool of_out;      ///< Whether to write stdout from Contam server into a file
  bool of_err;      ///< Whether to write stderr from Contam server into a file
  bool of_ach;      ///< Whether to retrieve .ach output file from Contam server
  bool of_age;      ///< Whether to retrieve .age output file from Contam server
  bool of_bal;      ///< Whether to retrieve .bal output file from Contam server
  bool of_cbw;      ///< Whether to retrieve .cbw output file from Contam server
  bool of_cex;      ///< Whether to retrieve .cex output file from Contam server
  bool of_csm;      ///< Whether to retrieve .csm output file from Contam server
  bool of_ebw;      ///< Whether to retrieve .ebw output file from Contam server
  bool of_log;      ///< Whether to retrieve .log output file from Contam server
  bool of_rst;      ///< Whether to retrieve .rst output file from Contam server
  bool of_rxr;      ///< Whether to retrieve .rxr output file from Contam server
  bool of_rzf;      ///< Whether to retrieve .rzf output file from Contam server
  bool of_rzm;      ///< Whether to retrieve .rzm output file from Contam server
  bool of_rzi;      ///< Whether to retrieve .rzi output file from Contam server
  bool of_sim;      ///< Whether to retrieve .sim output file from Contam server
  bool of_sqlite3;  ///< Whether to retrieve .sqlite3 output file from Contam
                    ///< server
  bool of_srf;      ///< Whether to retrieve .srf output file from Contam server
  bool of_val;      ///< Whether to retrieve .val output file from Contam server
  bool of_xlog;  ///< Whether to retrieve .xlog output file from Contam server
  std::string of_prefix;  ///< prefix path to the contam output files

  ClientParams();
  void clear();
  std::string to_string(unsigned indent = 0u) const;
};

std::tuple<ContamParams, ClientParams>
read_proto_params(std::string const& filename, bool verbose);

}  // namespace Contam_gRPC

#endif  // CONTAM_GRPC_CONTAM_PARAMS_H
