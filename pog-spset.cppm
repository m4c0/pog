export module pog:spset;
import :eid;
import btree;
import hai;
import traits;

namespace pog {
export template <typename Tp> class sparse_set {
  static constexpr const auto initial_dense_cap = 16;
  static constexpr const auto dense_cap_increase = 16;

  struct dense {
    Tp value;
    eid id;
  };

  hai::varray<dense> m_dense;
  btree::db::storage m_storage{};
  btree::tree m_index{&m_storage};

  using nnid = btree::db::nnid;

  static constexpr void swapy(auto &a, auto &b) noexcept {
    auto tmp = a;
    a = b;
    b = tmp;
  }
  constexpr void swap(unsigned a, unsigned b) noexcept {
    if (a == b)
      return;

    swapy(m_dense[a], m_dense[b]);

    m_index.remove(nnid{m_dense[a].id});
    m_index.remove(nnid{m_dense[b].id});

    m_index.insert(nnid{m_dense[a].id}, nnid(a));
    m_index.insert(nnid{m_dense[b].id}, nnid(b));
  }

public:
  explicit constexpr sparse_set() : m_dense{initial_dense_cap} {}

  sparse_set(const sparse_set &) = delete;
  sparse_set &operator=(const sparse_set &) = delete;

  constexpr sparse_set(sparse_set &&o)
      : m_dense{traits::move(o.m_dense)}, m_storage{traits::move(o.m_storage)} {
    m_index.set_root(o.m_index.root());
  }
  constexpr sparse_set &operator=(sparse_set &&o) {
    m_dense = traits::move(o.m_dense);
    m_storage = traits::move(o.m_storage);
    m_index.set_root(o.m_index.root());
  }

  constexpr void add(eid id, Tp v) {
    // TODO: upsert or fail?

    if (m_dense.size() == m_dense.capacity())
      m_dense.add_capacity(dense_cap_increase);

    m_index.insert(nnid{id}, btree::db::nnid(m_dense.size()));
    m_dense.push_back(dense{v, id});
  }

  constexpr void update(eid id, Tp v) {
    // TODO: upsert or fail?
    m_dense[m_index.get(nnid{id}).index()].value = v;
  }

  [[nodiscard]] constexpr Tp get(eid id) const noexcept {
    auto i = m_index.get(nnid{id});
    return i ? m_dense[i.index()].value : Tp{};
  }
  [[nodiscard]] constexpr bool has(eid id) const {
    return m_index.has(nnid{id});
  }

  constexpr void for_each_r(auto &&fn) {
    for (auto ri = m_dense.size(); ri != 0; ri--) {
      const auto &[v, id] = m_dense[ri - 1];
      fn(v, id);
    }
  }

  constexpr void remove_if(auto &&fn) {
    for_each_r([&](auto v, auto id) {
      if (fn(v, id))
        remove(id);
    });
  }
  constexpr void remove(eid id) {
    if (!id)
      return;

    auto idx = m_index.get(nnid{id});
    if (!idx)
      return;

    swap(idx.index(), m_dense.size() - 1);
    m_index.remove(nnid{id});
    m_dense.pop_back();
  }

  [[nodiscard]] constexpr auto *begin() const noexcept {
    return m_dense.begin();
  }
  [[nodiscard]] constexpr auto *end() const noexcept { return m_dense.end(); }
  [[nodiscard]] constexpr auto *begin() noexcept { return m_dense.begin(); }
  [[nodiscard]] constexpr auto *end() noexcept { return m_dense.end(); }
  [[nodiscard]] constexpr auto size() const noexcept { return m_dense.size(); }
};
} // namespace pog

namespace {
static constexpr auto build_set() {
  pog::sparse_set<int> set{};
  set.add(pog::eid{20}, 2);
  set.add(pog::eid{40}, 4);
  set.add(pog::eid{30}, 3);
  return set;
}
static constexpr bool set_matches(const pog::sparse_set<int> &set,
                                  auto... ids) {
  if (!(set.has(pog::eid{static_cast<unsigned>(ids)}) && ...))
    throw 0;

  if (set.size() != sizeof...(ids))
    throw 1;

  auto p = set.begin();
  unsigned ns[] = {static_cast<unsigned>(ids)...};
  for (auto n : ns) {
    if (set.get(pog::eid{n}) != n / 10)
      throw 4;

    const auto &[v, id] = *p++;
    if (id != pog::eid{n})
      throw 2;
    if (v != n / 10)
      throw 3;
  }

  return true;
}
static constexpr bool set_hasnt(const pog::sparse_set<int> &set, auto... ids) {
  if ((set.has(pog::eid{static_cast<unsigned>(ids)}) && ...))
    throw 0;
  return true;
}

static_assert([] {
  auto set = build_set();
  return !set.has(pog::eid{}) && !set.has(pog::eid{1});
}());
static_assert([] {
  auto set = build_set();
  return set_matches(set, 20, 40, 30);
}());
static_assert([] {
  auto set = build_set();
  set.remove(pog::eid{40});
  return set_matches(set, 20, 30) && set_hasnt(set, 40);
}());
static_assert([] {
  auto set = build_set();
  set.remove(pog::eid{20});
  set.add(pog::eid{10}, 1);
  return set_matches(set, 30, 40, 10);
}());
static_assert([] {
  auto set = build_set();
  set.remove(pog::eid{30});
  return set_matches(set, 20, 40) && set_hasnt(set, 30);
}());
static_assert([] {
  auto set = build_set();
  set.remove(pog::eid{20});
  set.remove(pog::eid{40});
  return set_matches(set, 30) && set_hasnt(set, 20, 40);
}());
static_assert([] {
  auto set = build_set();
  set.remove(pog::eid{20});
  set.remove(pog::eid{40});
  set.remove(pog::eid{30});
  return set_matches(set) && set_hasnt(set, 20, 30, 40);
}());
static_assert([] {
  auto set = build_set();
  set.remove_if([](auto v, auto id) { return v == 3 && id == 30; });
  return set_matches(set, 20, 40) && set_hasnt(set, 30);
}());
static_assert([] {
  auto set = build_set();
  set.remove_if([](auto v, auto id) { return true; });
  return set_matches(set) && set_hasnt(set, 20, 30, 40);
}());
static_assert([] {
  auto set = build_set();
  set.remove_if([](auto v, auto id) { return true; });
  set.add(pog::eid{40}, 4);
  set.add(pog::eid{30}, 3);
  set.add(pog::eid{20}, 2);
  set.add(pog::eid{10}, 1);
  return set_matches(set, 40, 30, 20, 10);
}());
static_assert([] {
  build_set().add(pog::eid{50}, 5);
  return true;
}());
static_assert([] {
  build_set().remove(pog::eid{});
  build_set().remove(pog::eid{1});
  return true;
}());
} // namespace
