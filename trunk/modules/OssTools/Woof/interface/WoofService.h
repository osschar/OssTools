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

//------------------------------------------------------------------------------

// #ifndef __CINT__

#include "DataFormats/GeometryVector/interface/GlobalPoint.h"
#include <map>

namespace Woof
{
   struct Poont
   {
      float x=0, y=0, z=0;

      Poont() {}
      Poont(const GlobalPoint& gp) : x(gp.x()), y(gp.y()), z(gp.z()) {}

      bool operator<(const Poont& o) const
      {
         if (x != o.x) return (x < o.x);
         if (y != o.y) return (y < o.y);
         return (z < o.z);
      }

      float d_sqr(const Poont& o) const
      {
         float dx = x - o.x, dy = y - o.y, dz = z - o.z;
         return dx*dx + dy*dy + dz*dz;
      }

      float dx_sqr(const Poont& o) const
      {
         float dx = x - o.x;
         return dx*dx;
      }
   };

   class TrajCnt
   {
   public:
      int XCmagF = 0, XCmagFiigOX = 0, XCmagFiigSX = 0;

      TrajCnt() : XCmagF(0), XCmagFiigOX(0), XCmagFiigSX(0) {}
      TrajCnt(int c, int iiO, int iiS) : XCmagF(c), XCmagFiigOX(iiO), XCmagFiigSX(iiS) {}

      ClassDefNV(TrajCnt, 1);
   };

   typedef std::map<Poont, int> PoontMap_t;

   class MfCount
   {
   public:
      static const int HitMax = 10;

      bool             in_loop = false; //!

      Int_t            ev,     seq;
      std::string      module, label;

      Int_t            all = 0, diff = 0, same_ptr = 0, same_val = 0, same_ptr_and_val = 0;

      PoontMap_t       pmap; //!
      Int_t            map_size,  map_size_5mu,  map_size_25mu;
      std::vector<int> hit_count, hit_count_5mu, hit_count_25mu;

      std::vector<TrajCnt> trajcnt;

      void count_hits(std::vector<int>& hc);
      void reduce_hits(float max_R);

      // ----------------------------------------------------------------

      MfCount()  {}
      ~MfCount() {}

      void begin(int e, int s, const std::string& m, const std::string& l);
      void end();

      const GlobalPoint* gpp_prev = 0; //!
      GlobalPoint        gpv_prev;     //!

      void check(const GlobalPoint& gp);
      void traja(int XCmagF, int XCmagFiigOX, int XCmagFiigSX);

      ClassDefNV(MfCount, 1);
   };

   extern MfCount mfc;
}

// #endif


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

private:
   WoofService(const WoofService&);                  // stop default
   const WoofService& operator=(const WoofService&); // stop default

   // ---------- member data --------------------------------

   TRint        *m_Rint = 0;
   bool          m_RunOnEveryEvent = false;

   int           m_EvSkipCount = 0;

   bool          m_TreeActive    = true;
   bool          m_IgPfPerEvent  = true;
   bool          m_IgPfPerModule = false;

   std::string   m_MfcFileName = "MfcWoof.root";
   TFile        *m_MfcFile = 0;
   TTree        *m_MfcTree = 0;

   int           m_EvCount = 0;
   int           m_ModCount = 0;

   ProcInfo_t    mProcInfoBegin, mProcInfoEnd;
   TStopwatch    mStopwatch;

   ClassDef(WoofService, 0);
};

#endif
