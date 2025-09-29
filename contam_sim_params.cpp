#include <string>
#include <iostream>
#include <fstream>
#include <tuple>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

namespace Contam_gRPC {

void pbuf_log_collector(
       google::protobuf::LogLevel level,
       const char* filename,
       int line,
       const std::string& message);

template<typename T>
bool read_prototext(const std::string& file_name, const bool is_binary,
                    T& proto_params)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  std::ifstream input(file_name, std::ios::in | std::ios::binary);

  google::protobuf::SetLogHandler(pbuf_log_collector);

  if (!input) {
    std::cerr << file_name << ": File not found!" << std::endl;
    return false;
  }
  if (is_binary) {
    if (!proto_params.ParseFromIstream(&input)) {
      std::cerr << "Failed to parse simulation params in binary-formatted input file: "
                << file_name << std::endl;
      return false;
    }
  } else {
    google::protobuf::io::IstreamInputStream istrm(&input);
    if (!google::protobuf::TextFormat::Parse(&istrm, &proto_params)) {
      std::cerr << "Failed to parse simulation params in text-formatted input file: "
                << file_name << std::endl;
      return false;
    }
  }
  return true;
}

static void set_contam_options(const contam::Sim_Params::Contam_Params& cfg,
                               ContamParams& cp, bool verbose = false)
{
  using Contam_Params = contam::Sim_Params::Contam_Params;

  cp.prj_filename = cfg.prj_filename();
  size_t n = cfg.amb_series_size();
  cp.amb_series.reserve(n)
  for(const auto& s: cfg.amb_series()) {
    cp.amb_series.emplace_back(s);
  }

  if (verbose) {
    std::cout << cp.to_string() << std::endl;
  }
}

static void set_client_options(const contam::Sim_Params::Client_Params& cfg,
                               ClientParams& cp, bool verbose = false)
{
  using Client_Params = contam::Sim_Params::Client_Params;

  if (verbose) {
    std::cout << cp.to_string() << std::endl;
  }
}

std::tuple<ContamParams, ClientParams>
read_proto_params(const std::string& filename, bool verbose)
{
  contam::Sim_Params::Simulation_Params sim_params;
  read_prototext(filename, false, sim_params);


  if (verbose) {
    std::string str;
    google::protobuf::TextFormat::PrintToString(sim_params, &str);
    std::cout << "---- Prototext '" << filename << "' read ----" << std::endl
              << str << std::endl;
  }

  ContamParams contam_p;
  ClientParams client_p;
  set_contam_options(sim_params.contam_params(), contam_p, verbose);
  set_client_options(sim_params.client_params(), client_p, verbose);

  return {contam_p, client_p};
}

} // end of namespace
