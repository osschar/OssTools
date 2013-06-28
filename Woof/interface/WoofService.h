#ifndef OssTools_Woof_WoofService_h
#define OssTools_Woof_WoofService_h
// -*- C++ -*-
//
// Package:     OssTools/Woof
// Class  :     WoofService
// 
/**\class WoofService WoofService.h OssTools/Woof/interface/WoofService.h

 Description: [one line class summary]

 Usage:
    <usage>

*/
//
// Original Author:  Matevz Tadel
//         Created:  Fri Jun 25 18:56:52 CEST 2010
// $Id$
//

#include <string>
#include <stdexcept>

#include "TStopwatch.h"
#include "TSystem.h"

namespace edm
{
   class ParameterSet;
   class ActivityRegistry;
   class Run;
   class Event;
   class EventSetup;
   class EventID;
   class Timestamp;

   class ModuleDescription;
}

class TRint;
class TFile;
class TTree;
class TBranch;

//------------------------------------------------------------------------------

namespace Woof
{
   class WoofModule
   {
   protected:
      TBranch *m_branch; //!

   public:
      WoofModule() : m_branch(0) {}
         
      virtual ~WoofModule() {}

      virtual void book_branches(TTree*) {}

      virtual void begin_event(int event) {}
      virtual void end_event() {}

      virtual void begin_module(int event, int module, const std::string& name, const std::string& label) {}
      virtual void end_module() {}

      ClassDef(WoofModule, 1);
   };
}

//==============================================================================
//==============================================================================


class WoofService
{
public:
   WoofService(const edm::ParameterSet&, edm::ActivityRegistry&);
   virtual ~WoofService();

   // ---------- const member functions ---------------------

   // ---------- static member functions --------------------

   // ---------- member functions ---------------------------

   void postBeginJob();
   void postEndJob();

   void postBeginRun(const edm::Run&, const edm::EventSetup&);

   void preProcessEvent (const edm::EventID&, const edm::Timestamp&);
   void postProcessEvent(const edm::Event&, const edm::EventSetup&);

   void preModule(const edm::ModuleDescription&);
   void postModule(const edm::ModuleDescription&);

   bool shouldSkip()    const { return (m_EvCount  < m_EvSkipCount); }
   bool shouldProcess() const { return (m_EvCount >= m_EvSkipCount); }

protected:
   std::vector<Woof::WoofModule*> m_Woofs;

private:
   WoofService(const WoofService&);                  // stop default
   const WoofService& operator=(const WoofService&); // stop default

   typedef std::vector<std::string> vStr_t;

   // ---------- member data --------------------------------

   TRint        *m_Rint = 0;
   bool          m_RunOnEveryEvent = false;

   int           m_EvSkipCount = 0;

   bool          m_TreeActive = true;
   vStr_t        m_WoofNames;

   bool          m_IgPfPerEvent  = true;
   bool          m_IgPfPerModule = false;

   std::string   m_OutFileName = "OutWoof.root";
   TFile        *m_OutFile = 0;
   TTree        *m_OutTree = 0;

   int           m_EvCount  = 0;
   int           m_ModCount = 0;

   ProcInfo_t    mProcInfoBegin, mProcInfoEnd;
   TStopwatch    mStopwatch;

   ClassDef(WoofService, 0);
};

#endif
