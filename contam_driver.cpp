/*
 * Copyright 2026 Lawrence Livermore National Security, LLC.
 * See the top-level LICENSE file for details.

 * SPDX-License-Identifier: BSD-3
 */

/** @file cxLibDriver_testOneZoneWthCtm.c
 *  @example cxLibDriver_testOneZoneWthCtm.c
 *  @brief contam-x-cosim API driver for test case: \b
 * testOneZoneWthCtm-UseApi.prj \n \n This test driver implements functions of
 * the contam-x-cosim API related to infiltration of contaminants through the
 * building envelope. These functions are akin to the use of the Wind Pressure
 * and Contaminant (WPC) file capabilities of CONTAM.
 */
#include "contam_driver.h"

#include <unistd.h>  // unlink, dup

#include <cstdio>  // freopen
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>

#include "contam-x-cosim.h"
#include "contam_ambience.h"

#define STDIO_REDIRECT 1

namespace Contam_gRPC {

using sim_state_map_t = std::unordered_map<std::string, void*>;
using iter_sim_state = sim_state_map_t::iterator;
using citer_sim_state = sim_state_map_t::const_iterator;
sim_state_map_t sim_states;
std::shared_mutex sim_state_mutex;

#if 1
/**
 * return the state and tell whether a new state has been created.
 */
std::tuple<void*, bool> get_or_create_state(std::string const& sim_id)
{
  {
    std::shared_lock read_lock(sim_state_mutex);
    citer_sim_state it = sim_states.find(sim_id);
    if (it != sim_states.cend()) {
      return {it->second, false};
    }
  }

  std::unique_lock write_lock(sim_state_mutex);
  citer_sim_state it = sim_states.find(sim_id);
  if (it != sim_states.cend()) {
    return {it->second, false};
  }

  void* pCxState = cxiGetContamState();
  sim_states.emplace(sim_id, pCxState);

  return {pCxState, true};
}
#else
// This is simpler and faster but readers don't run concurrently
std::tuple<void*, bool> get_or_create_state(std::string const& sim_id)
{
  std::unique_lock<std::shared_mutex> lock(sim_state_mutext);
  auto [it, inserted] = sim_states.try_emplace(sim_id, nullptr);

  if (inserted || it->second == nullptr) {
    it->second = cxiGetContamState();
  }
  return {it->second, inserted};
}
#endif

#define CTM_DT2SEC(_CTM_DATE_, _CTM_TIME_)                                     \
  (((_CTM_DATE_) -1) * 86400 + (_CTM_TIME_))

struct SimTime {
  IX step_size;
  IX sim_begin;
  IX sim_end;
  IX duration;
  IX num_steps;

  void set(void*);
  std::string to_string() const;
};

void SimTime::set(void* state)
{
  if (state == nullptr) {
    return;
  }

  step_size = cxiGetSimulationTimeStep(state);

  IX start_date = cxiGetSimulationStartDate(state);
  IX start_time = cxiGetSimulationStartTime(state);
  IX end_date = cxiGetSimulationEndDate(state);
  IX end_time = cxiGetSimulationEndTime(state);

  // express beginning and end of simulation in seconds
  sim_begin = CTM_DT2SEC(start_date, start_time);
  sim_end = CTM_DT2SEC(end_date, end_time);

  // Calculate the simulation duration in seconds.
  if (sim_begin < sim_end) {
    duration = sim_end - sim_begin;
  } else {
    duration = 365 * 86400 + sim_end - sim_begin;
  }

  // Calculate the number of steps to reach the end of the simulation.
  num_steps = duration / step_size;
}

std::string SimTime::to_string() const
{
  return std::string("SimTime\n{")
         + "\n  step_size: " + std::to_string(step_size) + "\n  sim_begin: "
         + std::to_string(sim_begin) + "\n  sim_end: " + std::to_string(sim_end)
         + "\n  duration: " + std::to_string(duration)
         + "\n  num_steps: " + std::to_string(num_steps) + "\n}";
}

/// Set the ambience conditions using the cxiSetAmbt API.
void setWthCtm(void* state, AmbienceSample const& amb)
{
  if (cxiSetAmbtTemperature(state, (R4) amb.temperature))
    exit(2);
  if (cxiSetAmbtPressure(state, (R4) amb.pressure))
    exit(3);
  if (cxiSetAmbtWndSpd(state, (R4) amb.wind_speed))
    exit(4);
  if (cxiSetAmbtWndDir(state, (R4) amb.wind_direction))
    exit(5);
  if (amb.mass_fraction.empty()
      || cxiSetAmbtMassFraction(state, 0, amb.mass_fraction[0]))
    exit(6);
}

/// Set the envelope conditions using the cxiSetEnvelope API.
void setEnvelopePathsWPC(void* state, IX envIndex, AmbienceSample const& env)
{
  auto rc = cxiSetEnvelopeWP(state, envIndex + 1, (R4) env.pressure);
  if (rc) {
    printf("cxiSetEnvelopeWP() returned the error code %d\n", rc);
    exit(1);
  }

  const size_t n_ctms = env.mass_fraction.size();
  for (size_t i = 0u; i < n_ctms; ++i) {
    auto rc = cxiSetEnvelopeMF(state, envIndex + 1, i, env.mass_fraction[i]);
    if (rc) {
      printf("cxiSetEnvelopeMF() returned the error code %d\n", rc);
      exit(2);
    }
  }
}

// Check if it's time to change to the next set of Ambient data.

/*---------------------------------------------------------------------------*/
/* Get simulation stage and print corresponding string.                      */
/*---------------------------------------------------------------------------*/
enum eSimStage getStage(void* state)
{
  enum eSimStage stage = cxiGetSimStage(state);
  char const* stageNames[] = {"simStage_Invalid",
                              "simStage_Initial",
                              "simStage_Setup",
                              "simStage_Calculation",
                              "simStage_End"};
  printf("Current stage is %s.", stageNames[stage]);

  return stage;
}

/*---------------------------------------------------------------------------*/
/* Callback error function.                                                  */
/*---------------------------------------------------------------------------*/
static void errorHandler(void* state,
                         char const* const headerStr,
                         char* const messageStr,
                         int* pNumErrors)
{
  IX stateValid = cxiIsStateValid(state);
  char const* headers[] = {"NOTE", "WARNING", "ERROR", "FATAL"};
  printf("stateValid = %d, nErrors = %d, %s:%s",
         stateValid,
         *pNumErrors,
         headerStr,
         messageStr);

  if (strcmp(headerStr, "FATAL") == 0) {
    printf(
      "===== A FATAL error occurred => calling cxiEndSimulation() =====\n");
    cxiEndSimulation(state);
  }
}

/*---------------------------------------------------------------------------*/
/* Callback projectData function to query contam-x about the project.        */
/*---------------------------------------------------------------------------*/
void projectDataReady(void* state, void* pUserData)
{
  IX numPaths = cxiGetNumPaths(state);
  PATH_COSIM_DSC pathDesc;

  enum eSimStage myStage = getStage(state);
  printf("\n==========> 2. simStage = %d\n", myStage);

  IX envCount = 0;
  for (IX i = 1; i <= numPaths; ++i) {
    if (cxiGetPathInfo(state, i, &pathDesc))
      exit(1);
    // envIndex > 1 designates an envelope path.
    if (pathDesc.envIndex > 0)
      envCount++;
  }

  // Get Contaminant information.
  IX nctm = cxiGetNumCtms(state);
  I1 name[NAMEFIELDLEN];
  for (IX i = 0; i < nctm; ++i) {
    if (cxiGetCtmName(state, i, name)) {
      printf("Error getting name for contaminant # %d\n", i);
      exit(1);
    }
    printf("Contaminant %d, Name %s\n", i, name);
  }

  // Get Zone information - test purposes only, i.e., not used to drive
  // simulation.
  IX numZones = cxiGetNumZones(state);
  printf("zoneNr\tzoneName\tlevelNum\tlevelName\tzoneVol[m3]\n");
  for (IX i = 1; i <= numZones; i++) {
    IX zoneNumber = -999;
    IX retVal = 0;
    ZONE_COSIM_DSC zDesc;
    retVal = cxiGetZoneInfo(state, i, &zDesc);
    if (retVal > 0) {
      printf("Error, cxiGetZoneInfo() returned %d\n", retVal);
      exit(1);
    }
    printf("%d\t%s\t%d\t%s\t%f\n",
           zDesc.nr,
           zDesc.name,
           zDesc.level_nr,
           zDesc.level_name,
           zDesc.Vol);
    IX zoneNr =
      cxiGetZoneNumber(state, zDesc.name, zDesc.level_nr, &zoneNumber);
    printf("\tReverse check using cxiGetZoneNumber(%s, %d) => %d\n",
           zDesc.name,
           zDesc.level_nr,
           zoneNumber);
  }
}

void getZoneMf(void* state, IX date, IX time, IX nz, IX nc)
{
  for (IX z = 1; z <= nz; ++z) {
    for (IX c = 0; c < nc; ++c) {
      R8 mf;
      char ctm_name[NAMEFIELDLEN];
      if (cxiGetZoneMF(state, z, c, &mf))
        exit(1);
      if (cxiGetCtmName(state, c, ctm_name))
        exit(1);
      printf(
        "Day %d Time %d Zone %d %s %g kg/kg\n", date, time, z, ctm_name, mf);
    }
  }
}

void start_io_redirection(std::string const& out_fname,
                          std::string const& err_fname,
                          int& saved_stdout,
                          int& saved_stderr)
{
#if STDIO_REDIRECT
  printf("\n==========> start IO redirection\n");
  saved_stdout = dup(fileno(stdout));
  saved_stderr = dup(fileno(stderr));
  if (std::freopen(out_fname.c_str(), "w", stdout) == NULL) {
    fprintf(stderr, "Error: Failed to reopen stdin\n");
    exit(1);
  }
  if (std::freopen(err_fname.c_str(), "w", stderr) == NULL) {
    fprintf(stderr, "Error: Failed to reopen stderr\n");
    exit(1);
  }
#endif
}

void stop_io_redirection(int saved_stdout, int saved_stderr)
{
  std::fflush(stdout);
  std::fflush(stderr);

#if STDIO_REDIRECT
  // Restore original stdout
  dup2(saved_stdout, fileno(stdout));
  close(saved_stdout);
  dup2(saved_stderr, fileno(stderr));
  close(saved_stderr);
  printf("\n==========> finish IO redirection\n");
#endif
}

bool runContam(std::string const& sim_id,
               std::string const& prj_fname,
               std::string const& out_fname,
               std::string const& err_fname,
               std::vector<AmbienceData>&& ambs,
               unsigned wp_wo_envelope,
               bool verbose)
{
  int saved_stdout = -1;
  int saved_stderr = -1;
  start_io_redirection(out_fname, err_fname, saved_stdout, saved_stderr);

  enum eSimStage simStage = simStage_Initial;

  // Get the ContamXState.
  auto [pCxState, new_state] = get_or_create_state(sim_id);
  std::cerr << (new_state ? "New state has been created for sim_id="
                          : "Existing state is picked up for sim_id=")
            << sim_id << std::endl;

  simStage = getStage(pCxState);
  printf("\n==========> 1. simStage = %d\n", simStage);

  // Set function pointer for the prjDataReady() function to be called.
  cxiRegisterCallback_PrjDataReady(pCxState, NULL, projectDataReady);
  cxiRegisterCallback_Error(pCxState, errorHandler);

  //-----  Set wind pressure mode to WTH/CTM method.
  // Must be called before cxiSetupSimulation().
  if (wp_wo_envelope == 1u) {
    cxiSetWindPressureMode(pCxState, 0);
  } else if (wp_wo_envelope == 0u) {
    cxiSetWindPressureMode(pCxState, 1);
  } else {
    fprintf(stderr,
            "Incorrect wp_wo_envelope (%u), which is for "
            "the second argument of cxiSetWindPressureMode()\n",
            wp_wo_envelope);
    stop_io_redirection(saved_stdout, saved_stderr);
    return false;
  }

  // Call cxiSetupSimulation() to read the project, set up nodes, links, etc.,
  //  and tell it to use the co-simulation mode which will then cause
  //  projectDataReady(), function defined above, to be called.
  cxiSetupSimulation(pCxState, const_cast<char*>(prj_fname.c_str()), 1);

  simStage = getStage(pCxState);

  printf("\n==========> 3. simStage = %d\n", simStage);

  // Get simulation timing information.
  SimTime simTime;
  simTime.set(pCxState);
  if (verbose) {
    printf("%s\n", simTime.to_string().c_str());
  }

  // Get the current date/time after initial steady state simulation.
  IX currTime = cxiGetCurrentTime(pCxState);
  IX currDate = cxiGetCurrentDate(pCxState);

  // Set initial Ambient conditions.
  IX t_curr = CTM_DT2SEC(currDate, currTime);
  if (wp_wo_envelope == 1u) {
    if (ambs.size() != 1u) {
      fprintf(
        stderr, "Incorrect number of ambience series (%lu)\n", ambs.size());
      stop_io_redirection(saved_stdout, saved_stderr);
      return false;
    }
    auto& amb = ambs[0];
    [[maybe_unused]] auto [amb_init, found_init, _] = amb.get_sample(t_curr);
    if (!found_init) {  // Use the default
      fprintf(stderr,
              "Initial ambience condition is missing for date %d time %d.\n",
              currDate,
              currTime);
    } else {
      setWthCtm(pCxState, amb_init);
    }
  } else {
    const size_t n_envs = ambs.size();
    if (n_envs == 0u) {
      fprintf(stderr, "No envelope series\n");
      stop_io_redirection(saved_stdout, saved_stderr);
      return false;
    }
    for (size_t i = 0u; i < n_envs; ++i) {
      auto& env = ambs[i];
      [[maybe_unused]] auto [env_init, found_init, _] = env.get_sample(t_curr);
      if (!found_init) {  // Use the default
        fprintf(stderr,
                "Initial envelop (%lu-th) condition is missing for date %d "
                "time %d.\n",
                i,
                currDate,
                currTime);
      } else {
        setEnvelopePathsWPC(pCxState, i, env_init);
      }
    }
  }

  // Get project information needed for this case.
  const IX numZones = cxiGetNumZones(pCxState);
  const IX numContams = cxiGetNumCtms(pCxState);

  // Get initial zone mass fractions.
  getZoneMf(pCxState, currDate, currTime, numZones, numContams);

  // Run through the simulation one time step at a time.
  while (t_curr < simTime.sim_end) {
    t_curr += simTime.step_size;
    if (wp_wo_envelope == 1u) {
      auto& amb = ambs[0];
      [[maybe_unused]] auto [amb_curr, _, updated] = amb.get_sample(t_curr);
      if (updated) {
        setWthCtm(pCxState, amb_curr);
        // printf("Ambience updated: %s\n", amb_curr.to_string().c_str());
      }
    } else {
      const size_t n_envs = ambs.size();
      for (size_t i = 0u; i < n_envs; ++i) {
        auto& env = ambs[i];
        [[maybe_unused]] auto [env_curr, _, updated] = env.get_sample(t_curr);
        if (updated) {
          setEnvelopePathsWPC(pCxState, i, env_curr);
        }
      }
    }
    // Run next time step.
    cxiDoCoSimStep(pCxState, 1);

    currTime = cxiGetCurrentTime(pCxState);
    currDate = cxiGetCurrentDate(pCxState);

    // Get zone mass fractions.
    getZoneMf(pCxState, currDate, currTime, numZones, numContams);
  }

  simStage = getStage(pCxState);
  printf("\n==========> 4. simStage = %d\n", simStage);
  if (simStage == simStage_End) {
    cxiEndSimulation(pCxState);
  }

  IX stateValidity = cxiIsStateValid(pCxState);
  simStage = getStage(pCxState);
  printf("\n==========> 5. simStage = %d, stateValid = %d\n",
         simStage,
         stateValidity);

  stop_io_redirection(saved_stdout, saved_stderr);

  return true;
}

}  // namespace Contam_gRPC
