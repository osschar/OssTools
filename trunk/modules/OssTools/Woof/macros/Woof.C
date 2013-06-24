namespace Woof
{
   class MfCount
   {
   public:
      static const int HitMax = 10;

      Int_t            ev,     seq;
      std::string      module, label;

      Int_t            all = 0, diff = 0, same_ptr = 0, same_val = 0, same_ptr_and_val = 0;

      Int_t            map_size,  map_size_5mu,  map_size_25mu;
      std::vector<int> hit_count, hit_count_5mu, hit_count_25mu;

      // ----------------------------------------------------------------

      MfCount()  {}
      ~MfCount() {}

      ClassDefNV(MfCount, 1);
   };
}
