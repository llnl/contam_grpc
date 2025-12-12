#ifndef CONTAM_GRPC_CONTAM_DRIVER_H
#define CONTAM_GRPC_CONTAM_DRIVER_H
#include <string>
#include <vector>

#include "contam_ambience.h"

namespace Contam_gRPC {

void runContam(std::string const& sim_id,
               std::string const& prj_fname,
               std::string const& out_fname,
               std::string const& err_fname,
               std::vector<AmbienceData>&& amb,
               unsigned wp_wo_envelope = 0,
               bool verbose = false);

}  // namespace Contam_gRPC

#endif  // CONTAM_GRPC_CONTAM_DRIVER_H
