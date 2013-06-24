// -*- C++ -*-
//
// Package:     Fireworks/Eve
// Class  :     WoofService
// 
// Implementation:
//     [Notes on implementation]
//
// Original Author:  Matevz Tadel
//         Created:  Fri Jun 25 18:57:39 CEST 2010
// $Id: WoofService.cc,v 1.9 2012/08/16 01:09:21 amraktad Exp $
//

// system include files
#include <iostream>

// user include files
#include "OssTools/Woof/interface/WoofService.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/ActivityRegistry.h"

// To extract coil current from ConditionsDB
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "CondFormats/RunInfo/interface/RunInfo.h"
#include "CondFormats/DataRecord/interface/RunSummaryRcd.h"

// To extract coil current from ConditionsInEdm
#include <FWCore/Framework/interface/Run.h>
#include <DataFormats/Common/interface/Handle.h>
#include "DataFormats/Common/interface/ConditionsInEdm.h"

#include "DataFormats/Provenance/interface/ModuleDescription.h"
#include "DataFormats/Provenance/interface/EventID.h"
#include "DataFormats/Provenance/interface/Timestamp.h"
#include "MagneticField/ParametrizedEngine/plugins/OAEParametrizedMagneticField.h"

#include "DataFormats/TrackReco/interface/Track.h"

#include "TROOT.h"
#include "TSystem.h"
#include "TRint.h"
#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"


//==============================================================================
// Beg IgProf stuffe
//==============================================================================

#define HIDDEN      __attribute__((visibility("hidden")))
#define VISIBLE     __attribute__((visibility("default")))
#define LIKELY(x)   __builtin_expect(bool(x), true)
#define UNLIKELY(x) __builtin_expect(bool(x), false)

typedef int IgProfAtomic; // correct for all supported platforms for now

HIDDEN
inline IgProfAtomic
IgProfAtomicInc (volatile IgProfAtomic *val)
{
# if __i386__ || __x86_64__
  IgProfAtomic result;
  __asm__ __volatile__
    (" lock; xaddl %0, (%1); incl %0;"
     : "=r" (result)
     : "r" (val), "0" (1)
     : "cc", "memory");
  return result;
# elif __ppc__
  IgProfAtomic result;
  __asm__ __volatile__
    (" lwsync\n"
     "1: lwarx %0, 0, %1\n"
     " addic %0, %0, 1\n"
     " stwcx. %0, 0, %1\n"
     " bne- 1b\n"
     " isync\n"
     : "=&r" (result)
     : "r" (val)
     : "cc", "memory");
  return result;
# endif
}

HIDDEN
inline IgProfAtomic
IgProfAtomicDec (volatile IgProfAtomic *val)
{
# if __i386__ || __x86_64__
  IgProfAtomic result;
  __asm__ __volatile__
    ("lock; xaddl %0, (%1); decl %0;"
     : "=r" (result)
     : "r" (val), "0" (-1)
     : "cc", "memory");
  return result;
# elif __ppc__
  IgProfAtomic result;
  __asm__ __volatile__
    (" lwsync\n"
     "1: lwarx %0, 0, %1\n"
     " addic %0, %0, -1\n"
     " stwcx. %0, 0, %1\n"
     " bne- 1b\n"
     " isync\n"
     : "=&r" (result)
     : "r" (val)
     : "cc", "memory");
  return result;
#endif
}

extern IgProfAtomic  s_igprof_enabled;

/** Enable the profiling system globally. Safe to call from anywhere. */
inline void
igprof_enable_globally(void)
{
  IgProfAtomicInc(&s_igprof_enabled);
}

/** Disable the profiling system globally. Safe to call from anywhere. */
inline void
igprof_disable_globally(void)
{
  IgProfAtomicDec(&s_igprof_enabled);
}

#undef HIDDEN
#undef VISIBLE
#undef LIKELY
#undef UNLIKELY

//==============================================================================
// End IgProf stuffe
//==============================================================================


namespace Woof
{
   MfCount mfc, *mfcp = &mfc;
}

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//==============================================================================
// constructors and destructor
//==============================================================================

WoofService::WoofService(const edm::ParameterSet& ps, edm::ActivityRegistry& ar)
{
   printf("WoofService::WoofService CTOR\n");

   m_EvSkipCount   = ps.getUntrackedParameter<int>("skip_count", 0);
   m_MfcFileName   = ps.getUntrackedParameter<std::string>("file_name", "MfcWoof.root");
   m_TreeActive    = ps.getUntrackedParameter<bool>("tree_active", true);
   m_IgPfPerEvent  = ps.getUntrackedParameter<bool>("igprof_per_event",  true);
   m_IgPfPerModule = ps.getUntrackedParameter<bool>("igprof_per_module", false);

   if (m_IgPfPerEvent && m_IgPfPerModule)
      throw cms::Exception("WoofServiceInvalidConfig") << "igprof_per_event and igprof_per_module should not be both true.";

   std::cout <<" gApplication "<< gApplication <<std::endl;
   std::cout <<" is batch "    << gROOT->IsBatch() <<std::endl;
   std::cout <<" display "     << gSystem->Getenv("DISPLAY") <<std::endl;

   const char* dummyArgvArray[] = {"cmsRun"};
   char**      dummyArgv = const_cast<char**>(dummyArgvArray);
   int         dummyArgc = 1;

   m_Rint = new TRint("App", &dummyArgc, dummyArgv);
   assert(TApplication::GetApplications()->GetSize());
  
   gROOT->SetBatch(kFALSE);
   std::cout<<"calling NeedGraphicsLibs()"<<std::endl;
   TApplication::NeedGraphicsLibs();

   if (m_TreeActive)
   {
      m_MfcFile = TFile::Open(m_MfcFileName.c_str(), "recreate");
      m_MfcTree = new TTree("T", "Woof::MfCount Tree for MF access profiling");
      m_MfcTree->Branch("M", &Woof::mfcp);
   }

   // ----------------------------------------------------------------

   ar.watchPostBeginJob(this, &WoofService::postBeginJob);
   ar.watchPostEndJob  (this, &WoofService::postEndJob);

   ar.watchPostBeginRun(this, &WoofService::postBeginRun);

   ar.watchPreProcessEvent (this, &WoofService::preProcessEvent);
   ar.watchPostProcessEvent(this, &WoofService::postProcessEvent);

   ar.watchPreModule (this, &WoofService::preModule);
   ar.watchPostModule(this, &WoofService::postModule);
}

WoofService::~WoofService()
{
   printf("WoofService::~WoofService DTOR\n");

   if (m_TreeActive)
   {
      m_MfcFile->cd();
      m_MfcTree->Write();
      m_MfcFile->Close();
      delete m_MfcFile;
   }
}


//==============================================================================
// Service watchers
//==============================================================================

void WoofService::postBeginJob()
{
   printf("WoofService::postBeginJob\n");

   // Show the GUI ...
   gSystem->ProcessEvents();

   gSystem->GetProcInfo(&mProcInfoBegin);
   mStopwatch.Start(true);
}

void WoofService::postEndJob()
{
   mStopwatch.Stop();
   gSystem->GetProcInfo(&mProcInfoEnd);

   printf("WoofService::postEndJob\n");
   printf("UserTime = %f  SysTime = %f  RealTime=%f  CpuTime=%f\n",
          mProcInfoEnd.fCpuUser - mProcInfoBegin.fCpuUser,
          mProcInfoEnd.fCpuSys  - mProcInfoBegin.fCpuSys,
          mStopwatch.RealTime(),  mStopwatch.CpuTime());
}

//------------------------------------------------------------------------------

void WoofService::postBeginRun(const edm::Run& iRun, const edm::EventSetup& iSetup)
{}

//------------------------------------------------------------------------------

void WoofService::preProcessEvent(const edm::EventID&, const edm::Timestamp&)
{
   m_ModCount = 0;

   if (m_IgPfPerEvent && shouldProcess())
   {
      igprof_enable_globally();
   }
}

void WoofService::postProcessEvent(const edm::Event& iEvent,
                                   const edm::EventSetup&)
{
   if (m_IgPfPerEvent && shouldProcess())
   {
      igprof_disable_globally();
   }

   // printf("WoofService::postProcessEvent: Starting TRint loop.\n");

   if (m_RunOnEveryEvent)
   {
     m_Rint->Run(kTRUE);
   }

   // Extract globalTracks, high-purity.
   // XXXX This was never finished, just ran normal EDM repacker job.
   {
      using namespace edm;
      Handle<View<reco::Track> > tracks;
      iEvent.getByLabel("generalTracks", tracks); 
      int cnt = 0;

      for (View<reco::Track>::const_iterator track = tracks->begin();
           track != tracks->end(); ++track, ++cnt)
      {
        if ( ! track->quality(reco::Track::highPurity)) continue;

        // Do something impressive
      }
   }

   ++m_EvCount;
}

//------------------------------------------------------------------------------

void WoofService::preModule(const edm::ModuleDescription& md)
{
   // printf("WoofService::preModule  %-40s %s\n", md.moduleName().c_str(), md.moduleLabel().c_str());

   if (m_TreeActive && shouldProcess())
   {
      Woof::mfc.begin(m_EvCount, m_ModCount, md.moduleName(), md.moduleLabel());
   }

   if (m_IgPfPerModule && md.moduleName() == "CkfTrackCandidateMaker")
   {
      igprof_enable_globally();
   }
}

void WoofService::postModule(const edm::ModuleDescription& md)
{
   // printf("WoofService::postModule %-40s %s\n", md.moduleName().c_str(), md.moduleLabel().c_str());

   if (m_IgPfPerModule && md.moduleName() == "CkfTrackCandidateMaker")
   {
      igprof_disable_globally();
   }

   if (m_TreeActive && shouldProcess())
   {
      Woof::mfc.end();

      if (Woof::mfc.all > 0)
      {
         m_MfcTree->Fill();
      }
   }

   ++m_ModCount;
}


//==============================================================================
// Woof::MfCounter
//==============================================================================

void Woof::MfCount::count_hits(std::vector<int>& hc)
{
   for (auto i = pmap.begin(); i != pmap.end(); ++i)
   {
      ++hc[i->second <= HitMax ? i->second : 0];
   }
}

void Woof::MfCount::reduce_hits(float max_R)
{
   if (pmap.size() < 2) return;

   const float max_R_sqr = max_R * max_R;

   auto i = pmap.begin();
   auto j = i;
   while (i != pmap.end())
   {
      ++j;
      if (j == pmap.end() || i->first.dx_sqr(j->first) > max_R_sqr)
      {
         ++i; j = i;
         continue;
      }
      if (i->first.d_sqr(j->first) < max_R_sqr)
      {
         auto k = j;
         i->second += j->second;
         --j;
         pmap.erase(k);
      }
   }
}

//------------------------------------------------------------------------------

void Woof::MfCount::begin(int e, int s, const std::string& m, const std::string& l)
{
   ev  = e;
   seq = s;
   module = m;
   label  = l;

   // clear map, sizes, counts
   all = diff = same_ptr = same_val = same_ptr_and_val = 0;

   pmap.clear();
   map_size = map_size_5mu = map_size_25mu = 0;
   hit_count     .assign(HitMax + 1, 0);
   hit_count_5mu .assign(HitMax + 1, 0);
   hit_count_25mu.assign(HitMax + 1, 0);

   in_loop = true;
}

void Woof::MfCount::end()
{
   in_loop = false;

   if (all == 0) return;

   map_size = (Int_t) pmap.size();
   count_hits(hit_count);

   reduce_hits(0.0005);

   map_size_5mu = (Int_t) pmap.size();
   count_hits(hit_count_5mu);

   reduce_hits(0.0025);

   map_size_25mu = (Int_t) pmap.size();
   count_hits(hit_count_25mu);
}

//------------------------------------------------------------------------------

void Woof::MfCount::check(const GlobalPoint& gp)
{
   if ( ! in_loop) return;

   ++all;

   ++pmap[gp];

   if (&gp == gpp_prev && gp == gpv_prev)
   {
      ++same_ptr_and_val;
   }
   else if (&gp == gpp_prev)
   {
      ++same_ptr;
   }
   else if (gpv_prev == gp)
   {
      ++same_val;
   }
   else
   {
      ++diff;
   }
   gpp_prev = &gp;
   gpv_prev = gp;
}

void Woof::MfCount::traja(int XCmagF, int XCmagFiigOX, int XCmagFiigSX)
{
   if ( ! in_loop) return;

   trajcnt.push_back(TrajCnt(XCmagF, XCmagFiigOX, XCmagFiigSX));
}
