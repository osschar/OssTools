#ifndef OssTools_Woof_MFCount_h
#define OssTools_Woof_MFCount_h

#include "OssTools/Woof/interface/WoofService.h"

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

   class MfCount : public WoofModule
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

      MfCount();
      virtual ~MfCount() {}

      virtual void book_branches(TTree* tree);

      virtual void begin_module(int e, int s, const std::string& m, const std::string& l);
      virtual void end_module();

      const GlobalPoint* gpp_prev = 0; //!
      GlobalPoint        gpv_prev;     //!

      void check(const GlobalPoint& gp);
      void traja(int XCmagF, int XCmagFiigOX, int XCmagFiigSX);

      static MfCount *theOne;

      ClassDef(MfCount, 1);
   };
}

#endif
