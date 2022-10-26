#ifndef INDEX_HPP_W1NZBQZJ
#define INDEX_HPP_W1NZBQZJ

#include <map>
#include <kmindex/index/index_infos.hpp>
#include <kmindex/index/kindex.hpp>

namespace kmq {

  class index
  {
    public:
      index(const std::string& index_path);

      void add_index(const std::string& name, const std::string& km_path);
      void save() const;

      auto begin();
      auto end();

      const index_infos& get(const std::string& name) const;

    private:
      void init(const std::string& ipath);

    private:
      std::string m_index_path;
      std::map<std::string, index_infos> m_indexes;
  };

}


#endif /* end of include guard: INDEX_HPP_W1NZBQZJ */
