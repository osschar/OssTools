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
// IgProf stuffe
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
// Woof modules configuration -- this sux, I know.
//==============================================================================

// Hmmh ... all is gone now.

//==============================================================================
//==============================================================================
// The true WoofService
//==============================================================================
//==============================================================================

//==============================================================================
// Constructors and destructor
//==============================================================================

WoofService::WoofService(const edm::ParameterSet& ps, edm::ActivityRegistry& ar)
{
   printf("WoofService::WoofService CTOR\n");

   m_EvSkipCount   = ps.getUntrackedParameter<int>("skip_count", 0);
   m_OutFileName   = ps.getUntrackedParameter<std::string>("file_name", "OutWoof.root");
   m_TreeActive    = ps.getUntrackedParameter<bool>("tree_active", true);
   m_WoofNames     = ps.getUntrackedParameter<vStr_t>("woof_modules", vStr_t());
   m_IgPfPerEvent  = ps.getUntrackedParameter<bool>("igprof_per_event",  true);
   m_IgPfPerModule = ps.getUntrackedParameter<bool>("igprof_per_module", false);

   if (m_IgPfPerEvent && m_IgPfPerModule)
      throw cms::Exception("WoofServiceInvalidConfig") << "igprof_per_event and igprof_per_module should not be both true.";

   // std::cout <<" gApplication "<< gApplication <<std::endl;
   // std::cout <<" is batch "    << gROOT->IsBatch() <<std::endl;
   // std::cout <<" display "     << gSystem->Getenv("DISPLAY") <<std::endl;

   const char* dummyArgvArray[] = {"cmsRun"};
   char**      dummyArgv = const_cast<char**>(dummyArgvArray);
   int         dummyArgc = 1;

   m_Rint = new TRint("App", &dummyArgc, dummyArgv);
   assert(TApplication::GetApplications()->GetSize());
  
   gROOT->SetBatch(kFALSE);
   // std::cout<<"calling NeedGraphicsLibs()"<<std::endl;
   TApplication::NeedGraphicsLibs();

   for (auto i = m_WoofNames.begin(); i != m_WoofNames.end(); ++i)
   {
      std::string classname("Woof::");
      classname += *i;
      Woof::WoofModule *wm = (Woof::WoofModule *) TClass::GetClass(classname.c_str())->New();
      m_Woofs.push_back(wm);
   }

   if (m_TreeActive)
   {
      m_OutFile = TFile::Open(m_OutFileName.c_str(), "recreate");
      m_OutTree = new TTree("T", "WoofService Tree holding WoofModule branches");

      for (auto i = m_Woofs.begin(); i != m_Woofs.end(); ++i)
      {
         (*i)->book_branches(m_OutTree);
      }
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
      m_OutFile->cd();
      m_OutTree->Write();
      m_OutFile->Close();
      delete m_OutFile;
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

   for (auto i = m_Woofs.begin(); i != m_Woofs.end(); ++i)
   {
      (*i)->begin_event(m_EvCount);
   }

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

   for (auto i = m_Woofs.begin(); i != m_Woofs.end(); ++i)
   {
      (*i)->end_event();
   }

   // printf("WoofService::postProcessEvent: Starting TRint loop.\n");

   if (m_RunOnEveryEvent)
   {
     m_Rint->Run(kTRUE);
   }

   ++m_EvCount;
}

//------------------------------------------------------------------------------

void WoofService::preModule(const edm::ModuleDescription& md)
{
   // printf("WoofService::preModule  %-40s %s\n", md.moduleName().c_str(), md.moduleLabel().c_str());

   if (m_TreeActive && shouldProcess())
   {
      for (auto i = m_Woofs.begin(); i != m_Woofs.end(); ++i)
      {
         (*i)->begin_module(m_EvCount, m_ModCount, md.moduleName(), md.moduleLabel());
      }
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
      for (auto i = m_Woofs.begin(); i != m_Woofs.end(); ++i)
      {
         (*i)->end_module();
      }
   }

   ++m_ModCount;
}
