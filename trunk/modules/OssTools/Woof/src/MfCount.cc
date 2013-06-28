#include "OssTools/Woof/interface/MfCount.h"

#include "TTree.h"
#include "TBranch.h"

namespace Woof
{
   MfCount *MfCount::theOne = 0;
}

Woof::MfCount::MfCount()
{
   if (theOne)
      throw std::runtime_error("Woof::MfCount second instantiation attempted.");

   theOne = this;
}

void Woof::MfCount::book_branches(TTree* tree)
{
  m_branch = tree->Branch("M", &theOne);
}

void Woof::MfCount::begin_module(int e, int s, const std::string& m, const std::string& l)
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

void Woof::MfCount::end_module()
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

   if (m_branch) m_branch->Fill();
}

//------------------------------------------------------------------------------

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
