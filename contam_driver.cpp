/** @file cxLibDriver_testOneZoneWthCtm.c 
 *  @example cxLibDriver_testOneZoneWthCtm.c
 *  @brief contam-x-cosim API driver for test case: \b testOneZoneWthCtm-UseApi.prj 
 *  \n \n
 *  This test driver implements functions of the contam-x-cosim API related to 
 *  infiltration of contaminants through the building envelope. These functions
 *  are akin to the use of the Wind Pressure and Contaminant (WPC) file
 *  capabilities of CONTAM.
 */
#include "contam-x-cosim.h"
#include "contam_driver.h"
#include <cstdlib>
#include <cstring>
#include <cstdio> // freopen
#include <unistd.h> // unlink, dup

namespace Contam_gRPC {

/*---------------------------------------------------------------------------*/
/* contam-x API driver for test case: testOneZoneWpcUseApi.prj               */
/*---------------------------------------------------------------------------*/

//---------- Global variables.
I1 prj_file_name[_MAX_PATH];	// Project file.
IX envCount;					// Number of envelope paths.
IX WthCtm_index = 0;

/*---------------------------------------------------------------------------*/
/* PROJECT-specific data and functions.                                      */
/*---------------------------------------------------------------------------*/
// WTH and CTM data for test case testOneZoneWthCtmUseApi.prj.
//   This PRJ has one contaminant.
// WTH and CTM data for test case testOneZoneWthCtm-UseApi.prj.
//   This PRJ has one contaminant.
// time in seconds.
//                 00:00,   05:00,     05:05,     06:05,     06:10,     14:00,  14:05,  16:00,  16:05,  23:00,  23:05,  24:00
IX times[]  =   {      0,   18000,     18300,     21900,     22200,     50400,  50700,  57600,  57900,  82800,  83100,  86400};
// T[]  temperature, K.
R8 Tambt[]  =   { 293.15,  293.15,    293.15,    293.15,    293.15,    293.15, 293.15, 293.15, 293.15, 293.15, 293.15, 293.15};
// P[]  barometric pressure, Pa.
R8 Pambt[]  =   {101325,   101325,    101325,    101325,    101325,    101325, 101325, 101325, 101325, 101325, 101325, 101325};
// WS[] wind speed, m/s.
R8 WSambt[] =   {   0.0,      0.0,       0.0,     7.397,     7.397,     7.397,    0.0,    0.0,  7.397,  7.397,    0.0,    0.0};
// WD[] wind direction, degree.
R8 WDambt[] =   {   0.0,      0.0,       0.0,     270.0,     270.0,     270.0,    0.0,    0.0,   90.0,   90.0,    0.0,    0.0};
// Mf[] mass fraction, kgc/kga.
R8 MFambt[] =   {    0.0,     0.0, 0.0023254, 0.0023254, 0.0023254, 0.0023254,    0.0,    0.0,   0.00,    0.0,     0.0,   0.0};

// Set the inital conditions for the SetAmbt API test.
void setWthCtmInit( void *state )
{
  if( cxiSetAmbtTemperature( state, (R4)Tambt[0] ) )
    exit(2);
  if( cxiSetAmbtPressure( state, (R4)Pambt[0] ) )
    exit(3);
  if( cxiSetAmbtWndSpd( state, (R4)WSambt[0] ) )
    exit(4);
  if( cxiSetAmbtWndDir( state, (R4)WDambt[0] ) )
    exit(5);
  if( cxiSetAmbtMassFraction( state, 0, MFambt[0] ) )
    exit(6);
  ++WthCtm_index;
}

// Update conditions when appropriate.
void setWthCtm( void* state, IX currentTime, IX currentDate, IX simTimeStep )
{
  // Check if it's time to change to the next set of Ambient data.
  if (currentTime + simTimeStep == times[WthCtm_index])
  {
    if( cxiSetAmbtTemperature( state, (R4)Tambt[WthCtm_index] ) )
      exit( 2 );
    if( cxiSetAmbtPressure( state, (R4)Pambt[WthCtm_index] ) )
      exit( 3 );
    if( cxiSetAmbtWndSpd( state, (R4)WSambt[WthCtm_index] ) )
      exit( 4 );
    if( cxiSetAmbtWndDir( state, (R4)WDambt[WthCtm_index] ) )
      exit( 5 );
    if( cxiSetAmbtMassFraction( state, 0, MFambt[WthCtm_index] ) )
      exit( 6 );
    ++WthCtm_index;
  }
}

/*---------------------------------------------------------------------------*/
/* Get simulation stage and print corresponding string.                      */
/*---------------------------------------------------------------------------*/
enum eSimStage getStage(void* state)
{
  enum eSimStage stage = cxiGetSimStage(state);
  const char* stageNames[] = { "simStage_Invalid","simStage_Initial","simStage_Setup","simStage_Calculation","simStage_End" };
  printf("Current stage is %s.", stageNames[stage]);

  return stage;
}

/*---------------------------------------------------------------------------*/
/* Callback error function.                                                  */
/*---------------------------------------------------------------------------*/
static void errorHandler(void* state, const char* const headerStr, char* const messageStr, int *pNumErrors)
{
  IX stateValid = cxiIsStateValid(state);
  const char* headers[] = { "NOTE", "WARNING", "ERROR", "FATAL" };
  printf("stateValid = %d, nErrors = %d, %s:%s", stateValid, *pNumErrors, headerStr, messageStr);

  if (strcmp(headerStr, "FATAL") == 0)
  {
    printf("===== A FATAL error occurred => calling cxiEndSimulation() =====\n");
    cxiEndSimulation(state);
  }
}

/*---------------------------------------------------------------------------*/
/* Callback projectData function to query contam-x about the project.        */
/*---------------------------------------------------------------------------*/
void projectDataReady(void* state, void* pUserData)
{
  IX numPaths = cxiGetNumPaths( state );
  PATH_COSIM_DSC pathDesc;

  enum eSimStage myStage = getStage(state);
  printf("\n==========> 2. simStage = %d\n", myStage);

  envCount = 0;
  for (IX i = 1; i <= numPaths; ++i)
  {
    if( cxiGetPathInfo( state, i, &pathDesc ) )
      exit(1);
    // envIndex > 1 designates an envelope path.
    if (pathDesc.envIndex > 0)
      envCount++;
  }

  // Get Contaminant information.
  IX nctm = cxiGetNumCtms( state );
  I1 name[NAMEFIELDLEN];
  for (IX i = 0; i < nctm; ++i)
  {
    if( cxiGetCtmName( state, i, name ) )
    {
      printf("Error getting name for contaminant # %d\n", i);
      exit(1);
    }
    printf("Contaminant %d, Name %s\n", i, name);
  }

  // Get Zone information - test purposes only, i.e., not used to drive simulation.
  IX numZones = cxiGetNumZones( state );
  printf("zoneNr\tzoneName\tlevelNum\tlevelName\tzoneVol[m3]\n");
  for(IX i = 1; i <= numZones; i++)
  {
    IX zoneNumber = -999;
    IX retVal = 0;
    ZONE_COSIM_DSC zDesc;
    retVal = cxiGetZoneInfo( state, i, &zDesc );
    if( retVal > 0)
    {
      printf("Error, cxiGetZoneInfo() returned %d\n", retVal);
      exit(1);
    }
    printf("%d\t%s\t%d\t%s\t%f\n", zDesc.nr, zDesc.name, zDesc.level_nr, zDesc.level_name, zDesc.Vol);
    IX zoneNr = cxiGetZoneNumber( state, zDesc.name, zDesc.level_nr, &zoneNumber );
    printf("\tReverse check using cxiGetZoneNumber(%s, %d) => %d\n", zDesc.name, zDesc.level_nr, zoneNumber);
  }

  // Set initial Ambient conditions.
  setWthCtmInit( state );
}

void getZoneMf( void* state, IX date, IX time, IX nz, IX nc )
{
  for(IX z = 1; z <= nz; ++z)
  {
    for(IX c = 0; c < nc; ++c)
    {
      R8 mf;
      char ctm_name[NAMEFIELDLEN];
      if( cxiGetZoneMF( state, z, c, &mf ) )
        exit(1);
      if( cxiGetCtmName( state, c, ctm_name ) )
        exit(1);
      printf("Day %d Time %d Zone %d %s %g kg/kg\n",
        date, time, z, ctm_name, mf);
    }
  }
}


void runContam(const std::string& prj_fname, const std::string& out_fname, const std::string& err_fname)
{
  int saved_stdout = dup(fileno(stdout));
  int saved_stderr = dup(fileno(stderr));
  std::freopen(out_fname.c_str(), "w", stdout);
  std::freopen(err_fname.c_str(), "w", stderr);

  enum eSimStage simStage = simStage_Initial;

  // Get the project file path from the command line.

  // Get the ContamXState.
  void *pCxState = cxiGetContamState();

  simStage = getStage(pCxState);
  printf("\n==========> 1. simStage = %d\n", simStage);

  // Set function pointer for the prjDataReady() function to be called.
  cxiRegisterCallback_PrjDataReady( pCxState, NULL, projectDataReady );
  cxiRegisterCallback_Error(pCxState, errorHandler);

  //-----  Set wind pressure mode to WTH/CTM method.
  // Must be called before cxiSetupSimulation().
  cxiSetWindPressureMode( pCxState, 0 );

  // Call cxiSetupSimulation() to read the project, set up nodes, links, etc.,
  //  and tell it to use the co-simulation mode which will then cause
  //  projectDataReady(), function defined above, to be called.
  cxiSetupSimulation( pCxState, const_cast<char*>(prj_fname.c_str()), 1 );

  simStage = getStage(pCxState);
  printf("\n==========> 3. simStage = %d\n", simStage);

  // Get project information needed for this case.
  IX numZones = cxiGetNumZones( pCxState );
  IX numContams = cxiGetNumCtms( pCxState );

  // Get simulation timing information.
  IX simTimeStep = cxiGetSimulationTimeStep( pCxState );
  IX simStartDate = cxiGetSimulationStartDate( pCxState );
  IX simStartTime = cxiGetSimulationStartTime( pCxState );
  IX simEndDate = cxiGetSimulationEndDate( pCxState );
  IX simEndTime = cxiGetSimulationEndTime( pCxState );

  IX simBegin = (simStartDate - 1) * 86400 + simStartTime;
  IX simEnd = (simEndDate - 1) * 86400 + simEndTime;

  // Calculate the simulation duration in seconds.
  IX simDuration;
  if (simBegin < simEnd)
    simDuration = simEnd - simBegin;
  else
    simDuration = 365 * 86400 - simEnd + simBegin;

  // Calculate the number of steps to reach the end of the simulation.
  IX numTimeSteps = simDuration / simTimeStep;

  // Get the current date/time after initial steady state simulation.
  IX currentTime = cxiGetCurrentTime( pCxState );
  IX currentDate = cxiGetCurrentDate( pCxState );

  // Get initial zone mass fractions.
  getZoneMf( pCxState, currentDate, currentTime, numZones, numContams );

  // Loop through the simulation one time step at a time.
  for (int i = 0; i < numTimeSteps; ++i)
  {
    // Tasks to perform before current time step.

    setWthCtm( pCxState, currentTime, currentDate, simTimeStep);

    // Run current time step.
    cxiDoCoSimStep( pCxState, 1 );

    // Tasks to perform after current time step.
    // Get new date/time.
    currentTime = cxiGetCurrentTime( pCxState );
    currentDate = cxiGetCurrentDate( pCxState );

    // Get zone mass fractions.
    getZoneMf( pCxState, currentDate, currentTime, numZones, numContams );

  }
  simStage = getStage(pCxState);
  printf("\n==========> 4. simStage = %d\n", simStage);
  if (simStage == simStage_End) {
    cxiEndSimulation(pCxState);
  }

  IX stateValidity = cxiIsStateValid(pCxState);
  simStage = getStage(pCxState);
  printf("\n==========> 5. simStage = %d, stateValid = %d\n", simStage, stateValidity);

  std::fflush(stdout);
  std::fflush(stderr);

  // Restore original stdout
  dup2(saved_stdout, fileno(stdout));
  close(saved_stdout);
  dup2(saved_stderr, fileno(stderr));
  close(saved_stderr);
}

} // end of namespace
